// STMTCPReceiver_generator.cc
// Author - George Sweetmore

// =====================================================================================
// CPP Includes
// =====================================================================================
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <tuple>
#include <chrono>
#include <poll.h>
#include <boost/lockfree/spsc_queue.hpp>

// =====================================================================================
// ARTDAQ includes
// =====================================================================================
#include "artdaq/DAQdata/Globals.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/ContainerFragmentLoader.hh"
#include "artdaq-core/Data/MetadataFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core-mu2e/Overlays/STMFragment.hh"
#include "fhiclcpp/fwd.h"
#include "fhiclcpp/ParameterSet.h"

// =====================================================================================
// Header includes
// =====================================================================================
#include "STMTCPReceiver.hh"

//////////////////////////////// - STMTCPReceiver - ////////////////////////////////////

namespace mu2e {

  // =====================================================================================
  // Constructor & Destructor
  // =====================================================================================
  STMTCPReceiver::STMTCPReceiver(fhicl::ParameterSet const& ps)
    : artdaq::CommandableFragmentGenerator(ps)
    , chan_(ps.get<int>("channelNumber", 0)) // Data channel (0=HPGE, 1=LaBr3)
    , host_(ps.get<std::string>("ip_address", "127.0.0.2")) // I.P. host of TCP socket
    , port_(ps.get<int>("port", 10025)) // Port of TCP socket
    , rcvbuf_bytes_(ps.get<int>("rcvbuf_bytes", 314572800)) // Buffer bytes (Default: 300*1024*1024)
    , recv_buffer_size_(ps.get<int>("recv_buffer_size", 10485760)) // Size of the temporary TCP receive buffer (Default: 10*1024*1024)
    , event_anchor_word_(ps.get<uint16_t>("event_anchor_word", 51966)) // Anchor word in Event header (0xCAFE)
    , start_fragment_id_(ps.get<uint64_t>("start_fragment_id", 100)) // Base fragment id
    , raw_stream_id_(ps.get<uint64_t>("raw_stream_id", 0)) // RAW fragment id
    , zs_stream_id_(ps.get<uint64_t>("zs_stream_id", 1)) // ZS fragment id
    , ph_stream_id_(ps.get<uint64_t>("ph_stream_id", 2)) // PH fragment id
    , container_stream_id_(ps.get<uint64_t>("container_stream_id", 3)) // Container fragment id
    , debug_level_(ps.get<int>("debug_level", 0)) // Set debug levels
    , pin_threads_(ps.get<bool>("pin_threads", true)) // Pin thread to least busy cores
    , recv_core_(ps.get<int>("recv_core", 40)) // Pin receive thread to own core
    , parser_core_(ps.get<int>("parser_core", 41)) // Pin parser thread to own core
    , builder_core_(ps.get<int>("builder_core", 42)) // Pin builder thread to own core
    , events_per_container_(ps.get<size_t>("events_per_container", 500)) // Default number of events per container in getNext()
    , offspill_events_per_container_(ps.get<size_t>("offspill_events_per_container", 100)) // Number of off-spill events per container in getNext()
    , use_spill_condition_(ps.get<bool>("use_spill_condition", false)) // Condition for batching on spill flags
    , eos_stream_id_(ps.get<uint64_t>("eos_stream_id", 4)) // End-of-subrun marker fragment id
    , rollover_subrun_interval_(ps.get<uint64_t>("rollover_subrun_interval", 0)) // EWTs per subrun (0 = disabled)
  {
    // Log configuration
    TLOG_DEBUG(1) << "[STM_BR] STMTCPReceiver: Channel = " << chan_ << " | Host = " << host_ << " | Port = " << port_
                  << " rcvbuf=" << rcvbuf_bytes_;

    // Create lockfree SPSC queues
    batcher_to_builder_queue_ = std::make_unique<BatcherToBuildQ>();
    builder_to_getNext_queue_ = std::make_unique<BuilderToGNQ>();

    // Create ring buffer
    ring_ = std::make_unique<EventRingBuffer>(1024ULL * 1024 * 1024); // 1GB

    // Get cpu_utils for thread-pinning
    std::string xml_config_path = EnvVars::expand("${STM_XML}");
    ::Config& cfg = ::Config::getInstance(xml_config_path);
    cpu_ = cpu_utils::getInstance(cfg, CpuRole::ArtDAQ);
    
  }

  STMTCPReceiver::~STMTCPReceiver() {
    // Signal end of datastream
    receiver_done_.store(true);
    stop_requested_.store(true);
    batcher_cv_.notify_all();
    builder_cv_.notify_all();

    // Wake up blocking calls
    if (listen_fd_ >= 0) ::shutdown(listen_fd_, SHUT_RDWR);
    if (client_fd_ >= 0) ::shutdown(client_fd_, SHUT_RDWR);

    // Close all threads and socket
    if (receiver_thread_.joinable()) receiver_thread_.join();
    if (parser_thread_.joinable()) parser_thread_.join();
    if (builder_thread_.joinable()) builder_thread_.join();
    if (listen_fd_ >= 0) ::close(listen_fd_);
    if (client_fd_ >= 0) ::close(client_fd_);
  }

  // =====================================================================================
  // Start function
  // =====================================================================================
  void STMTCPReceiver::start() {
    // NOTE:
    // start() MUST NOT block in ART. We ONLY create the listening socket here.
    // accept() and recv() happen in the receiver thread.
    
    TLOG(TLVL_INFO) << "[STM_BR][START] Starting STMTCPReceiver, opening listening socket... host="
                    << host_ << " port=" << port_;

    // Start global timer
    run_start_time_ = clock::now();

    // Setup socket
    listen_fd_ = setup_tcp_socket(host_, port_, rcvbuf_bytes_);
    if (listen_fd_ < 0) {
      TLOG(TLVL_ERROR) << "Failed to set up TCP listening socket";
      throw cet::exception("STMTCPReceiver") << "Failed to set up TCP listening socket";
    }

    receiver_done_.store(false);
    stop_requested_.store(false);

    // Start receiver thread (handles accept + recv)
    receiver_thread_ = std::thread(&STMTCPReceiver::receiverThread_, this);
    // Start parser thread
    parser_thread_ = std::thread(&STMTCPReceiver::parserThread_, this);
    // Start batcher thread
    builder_thread_ = std::thread(&STMTCPReceiver::builderThread_, this);
  }

  // =====================================================================================
  // Receiver Thread: Accepts TCP connection and reads data with idle timeout
  // =====================================================================================
  void mu2e::STMTCPReceiver::receiverThread_()
  {
    // Pin thread to core
    if(pin_threads_) cpu_->get_next_core("[STM_BR][RECEIVER]");
    
    TLOG(TLVL_INFO) << "[STM_BR][RECEIVER] Receiver thread started";

    std::vector<uint8_t> byte_buffer(recv_buffer_size_);

    struct pollfd listen_pfd{};
    listen_pfd.fd = listen_fd_;
    listen_pfd.events = POLLIN;

    while (!stop_requested_.load(std::memory_order_acquire)){

      // ======================================
      // Wait for TCP client connection
      // ======================================

      TLOG(tcpDebugLevel())
        << "[STM_BR][RECEIVER] Waiting for TCP client connection...";

      while (!stop_requested_.load(std::memory_order_acquire)) {

        int ret = poll(&listen_pfd, 1, 10);

        if (stop_requested_.load(std::memory_order_acquire))
          break;
        
        if (ret < 0) {
          if (errno == EINTR) continue;
          perror("poll(listen_fd)");
          return;
        }

        if (ret == 0)
          continue;

        if (listen_pfd.revents & POLLIN) {

          client_fd_ = accept(listen_fd_, nullptr, nullptr);

          if (client_fd_ >= 0)
            break;

          // Accept can occasionally fail after poll
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            continue;

          if (errno == EINTR)
            continue;

          perror("accept");
          continue;
        }
      }

      if (stop_requested_.load(std::memory_order_acquire))
        break;

      TLOG(TLVL_INFO)
        << "[STM_BR][RECEIVER] Client connected (fd=" << client_fd_ << ")";

      // ======================================
      // Configure client socket
      // ======================================

      int flags = fcntl(client_fd_, F_GETFL, 0);
      fcntl(client_fd_, F_SETFL, flags | O_NONBLOCK);

      size_t rmem_max = get_rmem_max();

      if (rmem_max > 0) {

        int bufsize = static_cast<int>(rmem_max);

        if (setsockopt(client_fd_,
                       SOL_SOCKET,
                       SO_RCVBUF,
                       &bufsize,
                       sizeof(bufsize)) < 0) {
          perror("setsockopt(SO_RCVBUF)");
        }

        socklen_t len = sizeof(bufsize);

        getsockopt(client_fd_,
                   SOL_SOCKET,
                   SO_RCVBUF,
                   &bufsize,
                   &len);

        TLOG(TLVL_INFO)
          << "[STM_BR][RECEIVER] SO_RCVBUF set to "
          << bufsize << " bytes";
      }

      // ======================================
      // Data receive loop
      // ======================================

      struct pollfd client_pfd{};
      client_pfd.fd = client_fd_;
      client_pfd.events = POLLIN;

      bool received_first_packet = false;
      bool connection_alive = true;

      while (connection_alive &&
             !stop_requested_.load(std::memory_order_acquire)) {

        int ret = poll(&client_pfd, 1, 10);

        // Check stop
        if (stop_requested_.load(std::memory_order_acquire))
          break;

        if (ret < 0) {
          if (errno == EINTR) continue;
          perror("poll(client_fd)");
          break;
        }

        if (ret == 0)
          continue;

        if (client_pfd.revents & POLLIN) {

          auto work_start = clock::now();

          for (;;) {

            ssize_t n = recv(client_fd_,
                             byte_buffer.data(),
                             byte_buffer.size(),
                             0);

            if (n > 0) {

              if (!received_first_packet) {
                received_first_packet = true;
                TLOG(tcpDebugLevel())
                  << "[STM_BR][RECEIVER] First packet received";
              }

              total_bytes_received_.fetch_add(n, std::memory_order_relaxed);

              // ======================================
              // Write into ring buffer
              // ======================================

              size_t remaining = n;
              size_t copied = 0;

              while (remaining > 0) {

                if (stop_requested_.load(std::memory_order_acquire)){
                  connection_alive = false;
                  break;
                }

                size_t writable = ring_->writable();

                // prevent overwrite of in-flight events
                if (writable < remaining) {
                  std::this_thread::yield();
                  continue;
                }

                size_t contiguous =
                  ring_->contiguousWritable();

                size_t to_copy =
                  std::min(remaining, contiguous);

                uint8_t* dst = ring_->writePtr();

                std::memcpy(dst,
                            byte_buffer.data() + copied,
                            to_copy);

                ring_->advanceWrite(to_copy);

                copied += to_copy;
                remaining -= to_copy;
              }

              continue;
            }

            if (n == 0) {
              TLOG(TLVL_WARNING)
                << "[STM_BR][RECEIVER] Sender disconnected";
              connection_alive = false;
              break;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
              break;

            perror("recv");
            connection_alive = false;
            break;
          }

          receiver_active_ +=
            (clock::now() - work_start);

          receiver_loops_.fetch_add(1, std::memory_order_relaxed);
        }

        if (client_pfd.revents &
            (POLLHUP | POLLERR | POLLNVAL)) {

          TLOG(TLVL_WARNING)
            << "[STM_BR][RECEIVER] Socket hangup/error";

          connection_alive = false;
        }
      }

      // ======================================
      // Cleanup connection
      // ======================================

      if (client_fd_ >= 0) {
        shutdown(client_fd_, SHUT_RDWR);
        close(client_fd_);
        client_fd_ = -1;
      }

      if (!stop_requested_.load(std::memory_order_acquire)) {
        TLOG(TLVL_INFO)
          << "[STM_BR][RECEIVER] Waiting for new sender connection";
      }
    }

    receiver_done_.store(true, std::memory_order_release);
    TLOG(TLVL_INFO)
      << "[STM_BR][RECEIVER] Receiver thread exiting";
  }
  
  // =====================================================================================
  // Parser Thread: Assembles events across chunk boundaries puts into batches
  // =====================================================================================
  void mu2e::STMTCPReceiver::parserThread_()
  {

    // Pin thread to core
    if(pin_threads_) cpu_->get_next_core("[STM_BR][PARSER]");

    TLOG(TLVL_INFO) << "[STM_BR][PARSER] Parser+Batcher thread started";

    auto make_new_batch = [&] {
      EventBatch b;
      b.events.reserve(events_per_container_);
      b.container_frag_id = start_fragment_id_ + container_stream_id_;
      b.container_seq_id  = 0;
      return b;
    };

    EventBatch batch = make_new_batch();
    uint16_t batch_spill_flag = 0;

    uint16_t header_words[EVENT_HEADER_WORDS];

    size_t expected_event_bytes = 0;

    while (true)
      {
        size_t available = ring_->readable();

        if (receiver_done_.load(std::memory_order_acquire) &&
            available == 0)
          break;

        // =================================================
        // Waiting for header
        // =================================================
        if (expected_event_bytes == 0)
          {
            if (available < EVENT_HEADER_BYTES) {

              if (receiver_done_.load(std::memory_order_acquire)) {
                TLOG(TLVL_WARNING)
                  << "[STM_BR][PARSER] Incomplete event at shutdown ("
                  << available << "/" << expected_event_bytes
                  << " bytes)";
                break;
              }
              std::this_thread::yield();
              continue;
            }

            const uint8_t* ptr = ring_->readPtr();
            size_t contiguous  = ring_->contiguousReadable();

            const uint16_t* hdr;

            if (contiguous >= EVENT_HEADER_BYTES) {
              hdr = reinterpret_cast<const uint16_t*>(ptr);
            }
            else {

              for (size_t i = 0; i < EVENT_HEADER_WORDS; ++i) {

                const uint8_t* p;

                size_t byte_off = i * 2;

                if (byte_off < contiguous)
                  p = ptr + byte_off;
                else
                  p = ring_->begin() + (byte_off - contiguous);

                header_words[i] =
                  uint16_t(p[0]) |
                  (uint16_t(p[1]) << 8);
              }

              hdr = header_words;
            }

            // Anchor validation
            if (hdr[anchor_start] != event_anchor_word_) {

              const uint16_t* p =
                reinterpret_cast<const uint16_t*>(ptr);

              size_t words = contiguous / 2;

              size_t i = 1;
              for (; i < words; ++i) {
                if (p[i] == event_anchor_word_)
                  break;
              }

              if (i == words)
                ring_->advanceRead(contiguous - 2);
              else
                ring_->advanceRead(i * 2);

              continue;
            }

            if (hdr[anchor_end] != event_anchor_word_) {
              ring_->advanceRead(2);
              continue;
            }

            uint16_t ps = hdr[PRESCALE];

            bool raw_prescaled = (ps >> 15) & 1;
            bool zs_prescaled  = (ps >> 7)  & 1;

            uint16_t raw_len = hdr[RAW_LEN];
            uint16_t zs_regs = hdr[ZS_REGIONS];
            uint16_t zs_len  = hdr[ZS_LEN] + zs_regs * 2;
            uint16_t ph_num  = hdr[PH_NUM];
            uint16_t ph_len  = ph_num * 2;

            if (raw_prescaled)
              raw_len = 0;

            if (zs_prescaled) {
              zs_regs = 0;
              zs_len  = 0;
            }

            expected_event_bytes =
              EVENT_HEADER_BYTES +
              size_t(raw_len + zs_len + ph_len) *
              sizeof(uint16_t);

            continue;
          }

        // =================================================
        // Waiting for full event
        // =================================================
        if (available < expected_event_bytes) {

          if (receiver_done_.load(std::memory_order_acquire)) {
            TLOG(TLVL_WARNING)
              << "[STM_BR][PARSER] Incomplete event at shutdown ("
              << available << "/" << expected_event_bytes
              << " bytes)";
            break;
          }

          std::this_thread::yield();
          continue;
        }

        // =================================================
        // Build EventView (ZERO COPY)
        // =================================================

        const uint8_t* ptr = ring_->readPtr();
        size_t contiguous  = ring_->contiguousReadable();

        EventView evt;

        bool wrapped = contiguous < expected_event_bytes;

        if (!wrapped) {

          evt.event_rb_head_ptr   = ptr;
          evt.event_rb_head_bytes = expected_event_bytes;

          evt.event_rb_wrap_ptr   = nullptr;
          evt.event_rb_wrap_bytes = 0;

        }
        else {

          size_t first  = contiguous;
          size_t second = expected_event_bytes - first;

          evt.event_rb_head_ptr   = ptr;
          evt.event_rb_head_bytes = first;

          evt.event_rb_wrap_ptr   = ring_->begin();
          evt.event_rb_wrap_bytes = second;
        }

        evt.size_bytes = expected_event_bytes;

        // =================================================
        // Header extraction
        // =================================================

        for (size_t i = 0; i < EVENT_HEADER_WORDS; ++i) {

          const uint8_t* p =
            resolveEventPointer(evt, i * 2);

          header_words[i] =
            uint16_t(p[0]) |
            (uint16_t(p[1]) << 8);
        }

        const uint16_t* hdr = header_words;

        evt.header = hdr;

        evt.event_num =
          int64_t(hdr[EWT_0]) |
          (int64_t(hdr[EWT_1]) << 16) |
          (int64_t(hdr[EWT_2]) << 32);

	//metricMan->sendMetric("Last event window tag", evt.event_num, "status", 3, artdaq::MetricMode::LastPoint);
	
        evt.spill_flag = hdr[EM_2_DRTDC] & 0x1;

        // =================================================
        // Dataset pointer resolution
        // =================================================

        if (!computeDatasetView(evt)) {

          TLOG(TLVL_ERROR)
            << "[STM_BR][PARSER] computeDatasetView failed";

          ring_->advanceRead(2);
          expected_event_bytes = 0;
          continue;
        }

        event_count_.fetch_add(1, std::memory_order_relaxed);

        // =================================================
        // Batching logic
        // =================================================

        if (batch.events.empty()) {

          batch.container_seq_id = evt.event_num;
          batch_spill_flag = evt.spill_flag;
        }
        else if (use_spill_condition_ &&
                 evt.spill_flag != batch_spill_flag) {

          EventBatch flushed = std::move(batch);
          batch = make_new_batch();

          while (!batcher_to_builder_queue_->push(std::move(flushed)))
            std::this_thread::yield();

          batcher_cv_.notify_one();

          batch.container_seq_id = evt.event_num;
          batch_spill_flag = evt.spill_flag;
        }

        batch.events.emplace_back(evt);

        const size_t target_batch_size =
          (batch_spill_flag == 0)
          ? offspill_events_per_container_
          : events_per_container_;

        if (batch.events.size() >= target_batch_size) {

          EventBatch full_batch = std::move(batch);
          batch = make_new_batch();

          while (!batcher_to_builder_queue_->push(std::move(full_batch)))
            std::this_thread::yield();

          batcher_cv_.notify_one();
        }

        ring_->advanceRead(expected_event_bytes);

	metricMan->sendMetric("Expected event bytes",expected_event_bytes,"status",3,artdaq::MetricMode::LastPoint);

        expected_event_bytes = 0;
      }

    // =================================================
    // Final flush
    // =================================================

    if (!batch.events.empty()) {

      EventBatch final_batch = std::move(batch);

      TLOG(TLVL_INFO) << "[STM_BR][PARSER] Flushing last batch";

      while (!batcher_to_builder_queue_->push(std::move(final_batch)))
        std::this_thread::yield();
    }

    batch.events.clear();

    TLOG(TLVL_INFO) << "[STM_BR][PARSER] Setting batcher done";

    batcher_done_.store(true, std::memory_order_release);
    batcher_cv_.notify_all();

    TLOG(TLVL_INFO)
      << "[STM_BR][PARSER] Parser+Batcher exiting";
  }
  
  // =====================================================================================
  // Builder Thread: Assembles event batchers into fragment containers
  // =====================================================================================
  void mu2e::STMTCPReceiver::builderThread_()
  {

    // Pin thread to core
    if(pin_threads_) cpu_->get_next_core("[STM_BR][BUILDER]");

    TLOG(TLVL_INFO) << "[STM_BR][BUILDER] Builder thread started";

    EventBatch batch;
    size_t debug_evt_counter = 0;
    const size_t debug_print_every = events_per_container_*5;

    // End of subrun fragment ID
    const uint64_t eos_frag_id = start_fragment_id_ + eos_stream_id_;

    while (true){

      if (!batcher_to_builder_queue_->pop(batch)) {

        if (batcher_done_.load(std::memory_order_acquire) &&
            batcher_to_builder_queue_->empty()) {

          TLOG(TLVL_INFO) << "[STM_BR][BUILDER] Parser finished and queue drained";
          break;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1));
        continue;
      }

      auto work_start = clock::now();

      // ------------------------------------------------
      // Create container fragment
      // ------------------------------------------------

      auto* container_frag = new artdaq::Fragment();

      container_frag->setSequenceID(batch.container_seq_id);
      container_frag->setFragmentID(batch.container_frag_id);

      //artdaq::ContainerFragmentLoader loader(*container_frag,
      //                                       FragmentType::STM);
      auto loader = std::make_unique<artdaq::ContainerFragmentLoader>(
                      *container_frag, FragmentType::STM);

      bool need_seq_id_for_new_container = false;

      // ------------------------------------------------
      // Collect fragments for this batch
      // ------------------------------------------------

      artdaq::Fragments batch_frags;
      batch_frags.reserve(batch.events.size() * 3);

      // Lambda functino to flush container
      auto flush_container = [&] {
	// Don't send empty container
	if (batch_frags.empty()) {
          delete container_frag;
          return;
        }
        loader->addFragments(batch_frags);
        while (!builder_to_getNext_queue_->push(container_frag)) {
          std::this_thread::yield();
        }
        batch_frags.clear();
      };

      for (const auto& e : batch.events)
        {
	  // If subrun transition in middle of container
	  if (need_seq_id_for_new_container) {
            container_frag->setSequenceID(e.event_num);
            need_seq_id_for_new_container = false;
          }

          ++debug_evt_counter;
          if ( (debug_level_ > 0) && ((debug_evt_counter % debug_print_every) == 0) ) {
            TLOG(TLVL_INFO)
              << "[STM_BR][BUILDER DEBUG] seq=" << batch.container_seq_id
              << " events= " << batch.events.size()
              << " raw=" << e.raw.size
              << " zs="  << e.zs.size
              << " ph="  << e.ph.size;
          }

          const uint64_t seq = e.event_num;

          auto process =
            [&](const DatasetView& ds,
                uint64_t stream_id,
                std::atomic<size_t>& counter)
            {
              const uint64_t frag_id =
                start_fragment_id_ + stream_id;

              std::unique_ptr<artdaq::Fragment> frag;

              if (ds.size == 0){
                frag = makeFragment_(frag_id, seq, nullptr, 0);
              }
              else{
                const int16_t* ptr = ds.ptr;
                if (!ds.wrap_ptr){
                  frag = makeFragment_(frag_id, seq, ptr, ds.size);
                }
                else{
                  frag = makeFragmentWrapped_(frag_id, seq, ds);
                }
              }

              if (frag){
                batch_frags.emplace_back(*frag);
                counter.fetch_add(1, std::memory_order_relaxed);
              }
            };

          process(e.raw, raw_stream_id_, raw_frag_count_);
          process(e.zs,  zs_stream_id_,  zs_frag_count_);
          process(e.ph,  ph_stream_id_,  ph_frag_count_);

	  // Sub-run transition: just in software for now
	  const bool sw_subrun_trigger = (rollover_subrun_interval_ > 0) && (e.event_num % rollover_subrun_interval_ == 0);
	  //if (hw_subrun_trigger || sw_subrun_trigger)
	  if (sw_subrun_trigger && e.event_num > 0)
	  {
	    const auto next_subrun = ++subrun_number_;
	    if (debug_level_ > 0) {
	      TLOG(TLVL_INFO) << "Subrun transition "
				      //"(hw=" << hw_subrun_trigger
				      << "(sw=" << sw_subrun_trigger
				      << ") at EWT=" << e.event_num
				      << " -> subrun " << next_subrun;
	    }

	    metricMan->sendMetric("SubrunNumber", next_subrun, "subrun", 1,
                                  artdaq::MetricMode::LastPoint | artdaq::MetricMode::Persist);

	    // Send container to getNext as is
	    flush_container();

	    // Make and push EOS fragment
            std::unique_ptr<artdaq::Fragment> eos_frag =
              artdaq::MetadataFragment::CreateEndOfSubrunFragment(my_rank, seq, next_subrun,eos_frag_id);

            if (eos_frag) {
	      artdaq::Fragment* eos_raw = eos_frag.release();
	      while (!builder_to_getNext_queue_->push(eos_raw)) {
		std::this_thread::yield();
	      }
            }

	    // Start a new container for the rest of this batch
            container_frag = new artdaq::Fragment();
            container_frag->setFragmentID(batch.container_frag_id);
            loader = std::make_unique<artdaq::ContainerFragmentLoader>(
                       *container_frag, FragmentType::STM);
	    need_seq_id_for_new_container = true;
	  }

      }

      // Send to getNext
      flush_container();

      builder_active_ += (clock::now() - work_start);
      builder_loops_.fetch_add(1, std::memory_order_relaxed);
    }

    builder_done_.store(true, std::memory_order_release);
    builder_cv_.notify_all();

    TLOG(TLVL_INFO) << "[STM_BR][BUILDER] Builder exiting";
  }

  // =====================================================================================
  // getNext_: Send container of fragments to EB
  // =====================================================================================
  bool mu2e::STMTCPReceiver::getNext_(artdaq::FragmentPtrs& frags)
  {
    artdaq::Fragment* raw_ptr = nullptr;

    while (true) {

      if (builder_to_getNext_queue_->pop(raw_ptr)) {

        // DEBUG PRINT
        if(debug_level_ > 0){
          TLOG(TLVL_INFO) << "\n[GETNEXT] Fragment received "
                          << "FragID=" << raw_ptr->fragmentID()
                          << " SeqID=" << raw_ptr->sequenceID()
                          << " Type="  << raw_ptr->type()
                          << " Size(bytes)=" << raw_ptr->dataSizeBytes();

          if (raw_ptr->type() == artdaq::Fragment::ContainerFragmentType) {

            artdaq::ContainerFragment cont(*raw_ptr);

            TLOG(TLVL_INFO) << "[GETNEXT] --> Container with "
                            << cont.block_count()
                            << " inner fragments";
          } // end container check
        } // end Debug
        
        frags.emplace_back(std::unique_ptr<artdaq::Fragment>(raw_ptr));
        return true;
      }
      
      // ARTDAQ requested stop
      if (should_stop()) {
        if (!stop_requested_.load(std::memory_order_acquire)) {
          TLOG(TLVL_INFO) << "[STM_BR][GETNEXT] Stop requested by ARTDAQ";
          stop_requested_.store(true, std::memory_order_release);
        }
      }
      
      if (builder_done_.load(std::memory_order_acquire)){
        TLOG(TLVL_INFO) << "[STM_BR][GETNEXT] Builder done, exiting";
        return false;
      }
      std::this_thread::yield();
    }
    return true;
  }
  
  // =====================================================================================
  // Stop function
  // =====================================================================================
  void mu2e::STMTCPReceiver::stop()
  {
    TLOG(TLVL_INFO) << "[STM_BR][STOP] STMTCPReceiver stop() called";

    // Signal threads to exit
    receiver_done_.store(true);
    stop_requested_.store(true);

    // Wake up getNext_() if it is waiting
    batcher_cv_.notify_all();
    builder_cv_.notify_all();
    
    // Close sockets to unblock poll()/recv()/accept()
    if (client_fd_ >= 0) {
      ::shutdown(client_fd_, SHUT_RDWR);
      ::close(client_fd_);
      client_fd_ = -1;
    }

    if (listen_fd_ >= 0) {
      ::shutdown(listen_fd_, SHUT_RDWR);
      ::close(listen_fd_);
      listen_fd_ = -1;
    }

    // Join threads safely
    if (receiver_thread_.joinable()) receiver_thread_.join();
    TLOG(TLVL_INFO) << "[STM_BR][STOP] Receiver thread joined";
    if (parser_thread_.joinable()) parser_thread_.join();
    TLOG(TLVL_INFO) << "[STM_BR][STOP] Parser thread joined";
    if (builder_thread_.joinable()) builder_thread_.join();
    TLOG(TLVL_INFO) << "[STM_BR][STOP] Builder thread joined";

    // Print summary
    TLOG(TLVL_INFO)
      << "[STM_BR][STOP] Summary: events=" << event_count_.load()
      << " bytes_received=" << total_bytes_received_.load()
      << " | produced RAW/ZS/PH frags= " << raw_frag_count_.load() << " / "
      << zs_frag_count_.load() << " / " << ph_frag_count_.load();

    // Print throughput
    auto total_runtime = clock::now() - run_start_time_;

    double runtime_sec =
      std::chrono::duration<double>(total_runtime).count();

    double receiver_sec =
      std::chrono::duration<double>(receiver_active_).count();

    double parser_sec =
      std::chrono::duration<double>(parser_active_).count();

    double builder_sec =
      std::chrono::duration<double>(builder_active_).count();

    TLOG(TLVL_INFO)
      << "\n========= [STM_BR][STOP] PERFORMANCE REPORT =========\n"
      << "Total runtime: " << runtime_sec << " s\n"
      << "Events: " << event_count_.load() << "\n"
      << "Throughput: "
      << (event_count_.load() / runtime_sec) << " events/sec\n\n"
      << "Receiver active: " << receiver_sec
      << " s (" << (receiver_sec/runtime_sec*100.0) << "%)\n"
      << "Parser active:   " << parser_sec
      << " s (" << (parser_sec/runtime_sec*100.0) << "%)\n"
      << "Builder active:  " << builder_sec
      << " s (" << (builder_sec/runtime_sec*100.0) << "%)\n"
      << "Avg parse time per event: "
      << (parser_sec / event_count_.load()) * 1e6
      << " us\n"
      << "Avg build time per event: "
      << (builder_sec / event_count_.load()) * 1e6
      << " us\n"
      << "====================================================";

  }
  
} // namespace mu2e

  // =====================================================================================
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(mu2e::STMTCPReceiver)
