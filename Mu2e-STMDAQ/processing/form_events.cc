// Form events header
#include "Mu2e-STMDAQ/processing/form_events.hh"

// Constructor
FormEvents::FormEvents(const std::shared_ptr<AsyncLogger>& logger_,
                       const std::shared_ptr<STMdata>& stm_) :
  logger(logger_), stm(stm_),
  event_to_copy(fw_eHdr_len+stm->mu2e_config.max_event_len,0),
  max_event_num(stm->buffer_config.max_event_num){

  // Register operations for OperationManager
  register_operation("get_events", [this](auto& b){ get_events(b); });
  
}

// Form data into events
void FormEvents::get_events(std::shared_ptr<DataStruct>& buffer){

  // The adc length
  size_t n = buffer->zs_len;

  // Define a pointer to the raw data buffer
  int16_t* data_ptr = buffer->zs.data();
  
  // Overall counter
  size_t count = 0;

  // The buffer's EWT data
  EWTinfo& EWTs = buffer->EWTs;

  // ADC data counter
  size_t adc_count = 0;

  // Loop through all elements
  while (count < n){
    
    // Get the packet start location
    size_t packet_start = count;
    
    // Calculate how much left in packet after packet header
    size_t leftInPacket = MAX_PACKET_LEN - pHdr_Len;
    
    // Increment the packet counter
    packet_count++;
    
    // While leftinPacket is more than a trigger header length
    while(leftInPacket > fw_eHdr_len){

      // Get header start index location
      size_t hdr_start_loc = packet_start + MAX_PACKET_LEN - leftInPacket;

      // Get event in packet length index location
      size_t data_len_loc = hdr_start_loc + fw_eHdr.EvInPacket;
    
      // Get event length
      size_t data_len = static_cast<uint16_t>(data_ptr[data_len_loc]);      
    
      // Get total event length
      size_t event_len = static_cast<uint16_t>(data_ptr[hdr_start_loc + fw_eHdr.EvLen]);      
    
      // Get the event window tag
      uint64_t EWT = stm->get_EWT(data_ptr,hdr_start_loc);

      // Get event mode
      uint64_t EM = stm->get_event_mode(data_ptr,hdr_start_loc);      
              
      // The start index of the adc data
      size_t data_start = hdr_start_loc + fw_eHdr_len;

      // If null hb detected
      if (EM == 0){

        // Notify user of null hb
        logger->log("FormEvents::get_events: Null heartbeat detected after event " +
                    std::to_string(current_EWT) + ".",2);
        
        // Check rest of packet for deadbeef
        leftInPacket = stm->check_dead_beef(data_ptr,packet_start,leftInPacket-fw_eHdr_len);

        // If return non-zero - FAIL! return!
        if (leftInPacket != 0) return;

        // Else store last event
        if (store_event(buffer,adc_count) != 0) return;
        
        // Set first event is true for more data
        first_event = true;
        
        // Exit loop through packet
        break;
    	
      }
            
      // If DEAD BEEF padding after timeout event 
      if (static_cast<uint16_t>(data_ptr[hdr_start_loc])     == BEEF &&
        static_cast<uint16_t>(data_ptr[hdr_start_loc + 1]) == DEAD) {
        logger->log("FormEvents::get_events: DEADBEEFs after firmware timeout event " +
		    std::to_string(current_EWT) +
		    ".",2);

	// Store last event
	if (store_event(buffer, adc_count) != 0) return;

	// Set first event is true for more data
	first_event = true;

	// Exit packet loop
        break;
      }

      // If this is the first event of the data run, set first event
      if (first_event){
	
        // Setup new event
        new_event(data_ptr,hdr_start_loc,EWT,event_len);
        
       	// Set first event boolean to false
       	first_event = false;
	
      } // End if first event

      // Recalculate left in packet
      leftInPacket -= fw_eHdr_len;            

      // If a new event
      if (EWT != current_EWT){

        // Store completed event
        if (store_event(buffer,adc_count) != 0) return;

        // Setup new event
        new_event(data_ptr,hdr_start_loc,EWT,event_len);
        
      } // End if !EWT != current_EWT


      // Check the data length is not longer than the packet
      if (data_len > leftInPacket) {
        logger->log("FormEvents: Warning! Data length to copy = " + std::to_string(data_len) +
                ", which exceeds the length left in the packet = " + std::to_string(leftInPacket) +
                ". At EWT = " + std::to_string(current_EWT) + ", skipping packet", 2);
        break; // advance to next packet
      }
            
      // Check the event is not larger than the single event buffer
      if (current_event_count+data_len > buffer->zs.size()){
        logger->log("FormEvents: Error: Attemping to copy event portion of length " +
                    std::to_string(data_len) +
                    " resulting in a total of " +
                    std::to_string(current_event_count+data_len) +
                    " ADCs to single event buffer of length " +
                    std::to_string(event_to_copy.size()),0);
        return;
      }      
      
      // Copy the portion of data back into the vector with the header
      std::copy(data_ptr + data_start,
          data_ptr + data_start + data_len,
          event_to_copy.begin() + current_event_count);

      
      // Increase event counters
      current_event_count += data_len;
      
      // Recalculate left in packet
      leftInPacket -= data_len;      	

    } // End while(leftInPacket > fw_eHdr_len)

    // Increase count by packet length
    count += MAX_PACKET_LEN;   
    
  } // while (count < buffer->raw_len)
  
  // Update the raw buffer data size
  buffer->raw_len = adc_count;

  // Insert header for any EWTs lost to dropped packets
  insert_missing_ewts(buffer);

  // Check the data total and EWTs data total match  
  EWT_info& last = buffer->EWTs[buffer->EWT_count-1];
  uint64_t last_EWT = last.EWT;
  size_t last_event_start = last.raw.start;
  size_t last_event_length = last.raw.len;
  size_t total =  last_event_start + last_event_length;
  // If unequal, throw errors
  if (buffer->raw_len != total){
    std::string error = "ERROR in FormEvents::get_events. For last EWT in buffer = "
      + std::to_string(last_EWT) + ", total data size ("
      + std::to_string(buffer->raw_len) + ") != accumulated data size in header map ("
      + std::to_string(total) + ")";
    logger->log(error,0);
    return;
  }

  // Ensure zs array data length is set to zero
  buffer->zs_len = 0;
  
  return;
  
}

// Setup new event
void FormEvents::new_event(int16_t* data_ptr, size_t hdr_start_loc,
                           uint64_t EWT, size_t event_len){
  
  // Get new software event header
  eHdr_to_copy = stm->create_sw_eHdr(data_ptr,hdr_start_loc);
  
  // Store the total length of this event
  current_event_len = event_len;
  
  // Reset current EWT
  current_EWT = EWT;	
  
  // Reset the event counters to zero
  current_event_count = 0;

}


// Store complete event
int FormEvents::store_event(std::shared_ptr<DataStruct>& buffer, size_t& adc_count){

  // The buffer's EWT data
  EWTinfo& EWTs = buffer->EWTs;
  
  // Check the length of the previous is correct
  if (current_event_count != current_event_len){
    logger->log("FormEvents: Error: Event " +
                std::to_string(current_EWT) +
                " has accumulated length of " +
                std::to_string(current_event_count) +
                " ADC values but expected " +
                std::to_string(current_event_len) +
                " ADC values!",2);
  }
  
  // Check that EWT info vector is large enough
  if (buffer->EWT_count >= EWTs.size()){
    logger->log("FormEvents: Error: Attemping to copy the info for event " +
                std::to_string(buffer->EWT_count) +
                " of this buffer to EWT info vector with capacity of " +
                std::to_string(EWTs.size()),0);
    return 1;
  }
  
  
  // Store info and map the EWT
  EWT_info& this_EWT = EWTs[buffer->EWT_count];
  this_EWT.EWT = current_EWT; // EWT
  this_EWT.raw.start = adc_count; // Buffer index of start of event's ADC data
  this_EWT.raw.len = current_event_count; // Length of event's ADC data in the buffer
  eHdr_to_copy[sw_eHdr.RAW_LEN] = current_event_count; // Length of event's ADC data in header
  this_EWT.hdr = eHdr_to_copy; // The actual header data

  // Check the event is not larger than the single event buffer
  if (adc_count+current_event_count > buffer->raw.size()){
    logger->log("FormEvents: Error: Attemping to copy event of length " +
                std::to_string(current_event_count) +
                " resulting in a total of " +
                std::to_string(adc_count+current_event_count) +
                " ADCs to DataStruct buffer of length " +
                std::to_string(buffer->raw.size()),0);
    return 1;
  }
  
  // Copy event data back to buffer
  std::copy(event_to_copy.begin(),
            event_to_copy.begin() + current_event_count,
            buffer->raw.begin() + adc_count);
  
  // Increase the data counter
  adc_count += current_event_count;

  tot_adc_count += current_event_count;
  
  // Increment the EWT counters
  ++EWT_count;
  ++buffer->EWT_count;

  return 0;

}

void FormEvents::insert_missing_ewts(std::shared_ptr<DataStruct>& buffer) {
    
    size_t lost_EWT_count = buffer->lost_EWT_count;
    if (lost_EWT_count == 0) return;

    size_t event_slots_available = max_event_num - buffer->EWT_count;
    if (lost_EWT_count > event_slots_available) {
	logger->log("FormEvents:insert_missing_ewts: Error! Number of EWTs "
		    "lost for buffer " + std::to_string(lost_EWT_count) + 
		    " exceeds events space left of " + std::to_string(event_slots_available), 0);
	return;
    }

    EWTinfo& EWTs = buffer->EWTs;

    for (size_t g = 0; g < buffer->lost_EWTs.size(); ++g) {

        uint64_t first_missing = buffer->lost_EWTs[g].first;
        uint64_t last_missing  = buffer->lost_EWTs[g].second;

        for (uint64_t missing = first_missing; missing <= last_missing; ++missing) {

            if (buffer->EWT_count >= max_event_num) {
                logger->log("FormEvents:insert_missing_ewts: EWT vector full, "
                            "cannot insert stub for EWT=" +
                            std::to_string(missing), 2);
                return;
            }

            EWT_info& stub = EWTs[buffer->EWT_count];

            // Identity and bad-data flag
            stub.EWT      = missing;
            stub.bad_data = true;

            // Zero-length raw data — points to current end of real data
            stub.raw.start    = buffer->raw_len; //temp
            stub.raw.len      = 0;
            stub.raw.prescale = false;

            // Build header using existing overload: EWT set, everything else zero
            stub.hdr = stm->create_sw_eHdr(missing, 0, 0, 0);
            stub.hdr[sw_eHdr.RAW_LEN]   = 0;
	    stub.hdr[sw_eHdr.DATA_FLAGS] = static_cast<int16_t>((1 << 0) | (1 << 1));

            ++buffer->EWT_count;
            ++EWT_count;
        }
    }

    // Re-sort so downstream sees EWTs in ascending order
    std::sort(EWTs.begin(), EWTs.begin() + buffer->EWT_count,
              [](const EWT_info& a, const EWT_info& b){
                  return a.EWT < b.EWT;
              });

    size_t running_end = 0;
    for (size_t i = 0; i < buffer->EWT_count; ++i) {
        EWT_info& ewt = EWTs[i];
        if (ewt.bad_data) {
            // Place stub at current running end with zero length
            ewt.raw.start = running_end;
        } else {
            // Real event - update running end
            running_end = ewt.raw.start + ewt.raw.len;
        }
    }
}
