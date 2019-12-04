#include "otsdaq-mu2e-stm/FEInterfaces/ROCStoppingTargetMonitorInterface.h"

#include "otsdaq/Macros/InterfacePluginMacros.h"
#include <unistd.h>//used for usleep(microseconds)

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FE-ROCStoppingTargetMonitorInterface"

//=========================================================================================
ROCStoppingTargetMonitorInterface::ROCStoppingTargetMonitorInterface(
    const std::string&       rocUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       theConfigurationPath)
    : ROCCoreVInterface(rocUID, theXDAQContextConfigTree, theConfigurationPath)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__MCOUT_INFO__("ROCStoppingTargetMonitorInterface instantiated with link: "
	               << linkID_ << " and EventWindowDelayOffset = " << delay_ << __E__);

	ConfigurationTree rocTypeLink =
	    Configurable::getSelfNode().getNode("ROCTypeLinkTable");

	STMParameter_1_ = rocTypeLink.getNode("NumberParam1").getValue<int>();

	STMParameter_2_ = rocTypeLink.getNode("TrueFalseParam2").getValue<bool>();

	STMParameter_3_ = rocTypeLink.getNode("tempColumn1").getValueAsString();
				
	//std::string STMParameter_3 = rocTypeLink.getNode("STMMustBeUniqueParam1").getValue<std::string>();
        
        __FE_COUTV__(STMParameter_1_);
        __FE_COUTV__(STMParameter_2_);
    //    __FE_COUTV__(STMParameter_3);                                
                 

}

//==========================================================================================
ROCStoppingTargetMonitorInterface::~ROCStoppingTargetMonitorInterface(void)
{
	// NOTE:: be careful not to call __FE_COUT__ decoration because it uses the
	// tree and it may already be destructed partially
	__COUT__ << FEVInterface::interfaceUID_ << " Destructor" << __E__;
}

//==================================================================================================
void ROCStoppingTargetMonitorInterface::writeROCRegister(uint16_t address,
                                                         uint16_t data_to_write)
{
	__FE_COUT__ << "Calling write ROC register: link number " << std::dec << linkID_
	            << ", address = " << address << ", write data = " << data_to_write
	            << __E__;

	bool requestAck = false;

	thisDTC_->WriteROCRegister(linkID_, address, data_to_write, requestAck);

	return;
}

//==================================================================================================
uint16_t ROCStoppingTargetMonitorInterface::readROCRegister(uint16_t address)
{
	__FE_COUT__ << "Calling read ROC register: link number " << std::dec << linkID_
	            << ", address = " << address << __E__;

	int read_data = 0;

	try
	{
		read_data = thisDTC_->ReadROCRegister(linkID_, address, 1);
	}
	catch(...)
	{
		__COUT__ << "DTC failed DCS read" << __E__;
		read_data = -999;
	}

	return read_data;
}

//============================================================================================
void ROCStoppingTargetMonitorInterface::writeEmulatorRegister(uint16_t address,
                                                              uint16_t data_to_write)
{
	__FE_COUT__ << "Calling write ROC Emulator register: link number " << std::dec
	            << linkID_ << ", address = " << address
	            << ", write data = " << data_to_write << __E__;

	return;
}

//==================================================================================================
uint16_t ROCStoppingTargetMonitorInterface::readEmulatorRegister(uint16_t address)
{
	__FE_COUT__ << "Calling read ROC Emulator register: link number " << std::dec
	            << linkID_ << ", address = " << address << __E__;

	return -1;
}

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readTimestamp() { return this->readRegister(12); }

//==================================================================================================
void ROCStoppingTargetMonitorInterface::writeDelay(uint16_t delay)
{
	this->writeRegister(21, delay);
	return;
}

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readDelay() { return this->readRegister(7); }

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readDTCLinkLossCounter()
{
	return this->readRegister(8);
}

//==================================================================================================
void ROCStoppingTargetMonitorInterface::resetDTCLinkLossCounter()
{
	this->writeRegister(24, 0x1);
	return;
}

//==================================================================================================
void ROCStoppingTargetMonitorInterface::configure(void) try {
  __MCOUT_INFO__(".... STM parameter 1 = " << STMParameter_1_<<__E__);
  __MCOUT_INFO__(".... STM parameter 2 = " << STMParameter_2_<<__E__);
  __MCOUT_INFO__(".... STM parameter 3 = " << STMParameter_3_<<__E__);
  
  this->writeRegister(0,STMParameter_1_);
  __MCOUT_INFO__("... STM ROC Register 0, Write par1, Read " << this->readRegister(0) << __E__);

  this->writeRegister(0,1);
  __MCOUT_INFO__("... STM ROC Register 0, Write 1, Read " << this->readRegister(0) << __E__);

  this->writeRegister(1,2);
  __MCOUT_INFO__("... STM ROC Register 1, Write 2, Read " << this->readRegister(1) << __E__);

  this->writeRegister(2,3);
  __MCOUT_INFO__("... STM ROC Register 2, Write 3, Read " << this->readRegister(2) << __E__);

  this->writeRegister(3,4);
  __MCOUT_INFO__("... STM ROC Register 3, Write 4, Read " << this->readRegister(3) << __E__);



  // __MCOUT_INFO__("......... Clear DCS FIFOs" << __E__);
  // this->writeRegister(0,1);
  // this->writeRegister(0,0);

  // setup needToResetAlignment using rising edge of register 22
  // (i.e., force synchronization of ROC clock with 40MHz system clock)
  //__MCOUT_INFO__("......... setup to synchronize ROC clock with 40 MHz clock
  // edge" << __E__);  this->writeRegister(22,0);  this->writeRegister(22,1);

  // this->writeDelay(delay_);
  // usleep(100);

  //__MCOUT_INFO__ ("........." << " Set delay = " << delay_ << ", readback = "
  //<< this->readDelay() << " ... ");

  //__FE_COUT__ << "Debugging ROC-DCS" << __E__;

  // unsigned int val;

  // read 6 should read back 0x12fc
  // for (int i = 0; i<1; i++)
  //{
  //	val = this->readRegister(6);
  //
  //	//__MCOUT_INFO__(i << " read register 6 = " << val << __E__);
  //	if(val != 4860)
  //	{
  //		__FE_SS__ << "Bad read not 4860! val = " << val << __E__;
  //		__FE_SS_THROW__;
  //	}
  //
  //	val = this->readDelay();
  //	//__MCOUT_INFO__(i << " read register 7 = " << val << __E__);
  //	if(val != delay_)
  //	{
  //		__FE_SS__ << "Bad read not " << delay_ << "! val = " << val <<
  //__E__;
  //		__FE_SS_THROW__;
  //	}
  //}

  // if(0) //random intense check
  //{
  //	highRateCheck();
  //}

  //__MCOUT_INFO__ ("......... reset DTC link loss counter ... ");
  // resetDTCLinkLossCounter();

} catch (const std::runtime_error &e) {
  __FE_MOUT__ << "Error caught: " << e.what() << __E__;
  throw;
} catch (...) {
  __FE_SS__ << "Unknown error caught. Check printouts!" << __E__;
  __FE_MOUT__ << ss.str();
  __FE_SS_THROW__;
}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::halt(void) {
  __MCOUT_INFO__("In ::halt()"<<event_number_);
  return;
}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::pause(void) {
  __MCOUT_INFO__("In ::pause()"<<event_number_);
  return;
}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::resume(void) {
  __MCOUT_INFO__("In ::resume()"<<event_number_);
  return;
}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::start(std::string runNumber)
{
  number_of_good_events_ = 0;
  number_of_bad_events_ = 0;
  number_of_empty_events_ = 0;
  event_number_ = 0;
  __MCOUT_INFO__("In ::start() "<<event_number_<<"  runNo.="<<runNumber);
  return;
}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::stop(void) {
  __MCOUT_INFO__("Stopping run " << std::dec << event_number_ << __E__);
  __MCOUT_INFO__("Stopping run reset nGood=0 from " << std::dec << number_of_good_events_ << __E__);
  number_of_good_events_ = 0;
  event_number_=0;
  return;
}

//========================================================================================================================
bool ROCStoppingTargetMonitorInterface::running(void) { 
	event_number_++;
	usleep(3000000);
	unsigned FPGA_Te=readRegister(36880);
	unsigned DTCFDVR=readRegister(0x9000);
	unsigned VVR=readRegister(0x900C);
	
	  __MCOUT_INFO__("Running event number " << std::dec << event_number_ << "  Par1="<<STMParameter_1_<<"  Par2="<<STMParameter_2_<<"  Par3="<<STMParameter_3_<<"  temprature: "<<FPGA_Te*503.975/4096<<"  ADC="<<FPGA_Te<<" DTCFDVR="<<DTCFDVR<<" VVR="<<VVR<<__E__);

	//datafile_ << "# Event " << event_number_;
/*
	int fail = 0;

	std::vector<uint16_t> val;

	unsigned FIFOdepth = 0;
	FIFOdepth = readRegister(35);

	unsigned counter = 0;    // don't wait forever
	    
	while ((FIFOdepth == 65535 || FIFOdepth == 0) && counter < 1000) {
	
	  readRegister(6);
	  if (counter % 100 == 0) {
	    __FE_COUT__ << "... waiting for non-zero depth" << __E__;
	  }
	  FIFOdepth = readRegister(35);
	  counter++;
       
	}
	
	if (FIFOdepth > 0 && FIFOdepth != 65535) {
			
	  unsigned depth_to_read = 200;
	  if (FIFOdepth < depth_to_read) {
	  	depth_to_read = FIFOdepth;	
	  }
	  

//	  datafile_ << " ==> FIFOdepth = " << FIFOdepth << "... Number of words to read = " 
//	            << depth_to_read << std::endl;
	      
	  //readBlock(val, 42 , FIFOdepth, 0);
	  readBlock(val, 42 , depth_to_read, 0);

	  if (abs( val.size() - depth_to_read) < 0.5) {
	      
	    for(size_t rr = 0; rr < val.size(); rr++)
	      {
		    
		if ( (rr+1)%5 == 0) {
//		  datafile_ << " " << std::hex << val[rr] << std::dec << std::endl;
		} else {
//		  datafile_ << " " << std::hex << val[rr] << std::dec;
		}
		    
		// check for data integrity here...
		//	    if(val[rr] != correct[j])
		//	      {
		//	    	      fail++;
		//	    	      __MCOUT__("... fail on read " << rr 
		//	    			<< ":  read = " << val[rr] 
		//	    			<< ", expected = " << correct[j] << __E__);
		//	    	      // __SS__ << roc->interfaceUID_ << i << "\tx " << r << " :\t "
		//	    //	    //	    //	    //	    //	    //	    //	    //	    	      //	   << "read register " << baseAddress + j << ". Mismatch on read " << val[rr]
		//	   << " vs " << correct[j] << ". Read failed on read number "
		//	   << cnt << __E__;
		//__MOUT__ << ss.str();
		//__SS_THROW__;
		//	    	    }
	      }
	  } else {
	    
//	    datafile_ << "# ERROR --> Number of words returned by DTC = " << val.size() << std::endl;
	    
	    fail++;
	        
	    //	    __MCOUT__("... DTC returns " << val.size() << " words instead of " 
	    //	  		<< r << "... punt on this event" << __E__); 

	  }

	} else {

//	  datafile_ << std::endl << "# EMPTY... FIFO did not report data for this event " << std::endl;
	  number_of_empty_events_++;

	}

	if (fail > 0) {
	  number_of_bad_events_++;
	} else {
	  number_of_good_events_++;
	}


	if (0){
	  unsigned data_to_check = readRegister(0x6);

	  while (data_to_check != 4860) {
	    data_to_check = readRegister(0x6);
	  }

	  data_to_check = readRegister(0x7);
	  
	  while (data_to_check != delay_) {
	    data_to_check = readRegister(0x7);
	  }
	}

	//		unsigned int          r;
	//       	//int          loops  = loops;//10 * 1000;
	//	int          cnt    = 0;
	//	int          cnts[] = {0, 0};
	//	
	//	int baseAddress = 6;
	//	unsigned int correctRegisterValue0 = 4860;
	//	unsigned int correctRegisterValue1 = delay_;
	//	
	//	unsigned int correct[] = {correctRegisterValue0,correctRegisterValue1};//{4860, 10};
	//	
	    //	for(unsigned int j = 0; j < 2; j++)
	    //	  {
	    //	    r = (rand() % 100) + 1;  //avoid calling block reads "0" times by adding 1
	    //
	    //	    //__MCOUT__(interfaceUID_ << " :\t read register " << baseAddress + j 
	    //	      << " " << r << " times" << __E__);
	    //
	    //	    readBlock(val, baseAddress + j,r,0);
	    //
	    //	    datafile_ << " ==> Number of expected words = " << r << std::endl;
	    //
	    //	    int fail = 0;

	    //	    if (abs( val.size() - r) < 0.5) {
	      //	      
	      //	      for(size_t rr = 0; rr < val.size(); rr++)
	      //	      {
	      //	        ++cnt;
	    //	    	  ++cnts[j];
	    //	    
	    //	    	  if ( (rr+1)%5 == 0) {
	    //	    	    datafile_ << std::hex << val[rr] << std::endl;
	    //	    	  } else {
	    //	    	    datafile_ << std::hex << val[rr];
	    //	    	  }
	    //	    			
	    //	    	  if(val[rr] != correct[j])
	    //	    	    {
	    //	    	      fail++;
	    //	    	      __MCOUT__("... fail on read " << rr 
	    //	    			<< ":  read = " << val[rr] 
	    //	    			<< ", expected = " << correct[j] << __E__);
	    //	    	      // __SS__ << roc->interfaceUID_ << i << "\tx " << r << " :\t "
	    //	    //	    //	    //	    //	    //	    //	    //	    //	    	      //	   << "read register " << baseAddress + j << ". Mismatch on read " << val[rr]
		      //	   << " vs " << correct[j] << ". Read failed on read number "
		      //	   << cnt << __E__;
		      //__MOUT__ << ss.str();
		      //__SS_THROW__;
	    //	    	    }
	    //	    	}
	    //	  } else {
	    //	    
	    //	    datafile_ << "#ERROR --> Number of words returned by DTC = " << val.size() << std::endl;
	    //	    
	    //	    fail++;
	    //	    
	    //	    __MCOUT__("... DTC returns " << val.size() << " words instead of " 
	    //	    		<< r << "... punt on this event" << __E__); 
	    //	    
	    //	  }

*/
  
  return false; }

DEFINE_OTS_INTERFACE(ROCStoppingTargetMonitorInterface)
