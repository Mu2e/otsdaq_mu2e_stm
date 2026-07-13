#ifndef STMTCPRECEIVER_HH
#define STMTCPRECEIVER_HH

// STMTCPReceiver.hh
// Author George Sweetmore

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
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <boost/lockfree/spsc_queue.hpp>

// =====================================================================================
// ARTDAQ includes
// =====================================================================================
#include "artdaq/DAQdata/Globals.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core-mu2e/Overlays/STMFragment.hh"
#include "fhiclcpp/fwd.h"
#include "fhiclcpp/ParameterSet.h"

// =====================================================================================
// Trace includes
// =====================================================================================
#include "trace.h"
#define TRACE_NAME "STMTCPReceiver"

// =====================================================================================
// Helper headers
// =====================================================================================
#include "sys_utils.hh"   // Sys utils ( e.g. pin_thread_to_least_busy_core(std::thread&, const char*) )
#include "tcp_utils.hh"   // General utils ( e.g. variable definitions )
// Include cpu_utils from STMDAQ
#include "Mu2e-STMDAQ/config/config.hh"
#include "Mu2e-STMDAQ/utils/cpu_utils.hh"
#include "Mu2e-STMDAQ/utils/EnvVars.hh"

// =====================================================================================
// Generator
// =====================================================================================

namespace mu2e {

  class STMTCPReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit STMTCPReceiver(fhicl::ParameterSet const& ps);
    ~STMTCPReceiver() override;

    FragmentType GetFragmentType() { return fragment_type_; }

  private:
    // ----------------- artDAQ -----------------
    bool getNext_(artdaq::FragmentPtrs& output) override;
    void start() override;
    void stopNoMutex() override {}
    void stop() override;

    FragmentType fragment_type_{FragmentType::STM};  // Type of fragment (see FragmentType.hh)
    std::shared_ptr<cpu_utils> cpu_;
    
    // ----------------- Config -----------------
    int chan_;                               // Data channel (0=HPGE, 1=LaBr3)
    std::string host_;                       // I.P. host of TCP socket
    int port_;                               // Port of TCP socket
    int rcvbuf_bytes_;                       // Socket receive buffer bytes
    int recv_buffer_size_;                   // Size of the temporary TCP receive buffer
    uint16_t event_anchor_word_;             // Anchor word in Event header
    uint64_t start_fragment_id_;             // Base fragment id
    uint64_t raw_stream_id_;                 // RAW fragment id
    uint64_t zs_stream_id_;                  // ZS fragment id
    uint64_t ph_stream_id_;                  // PH fragment id
    uint64_t container_stream_id_;           // Container fragment id
    int      debug_level_;                   // Debug verbosity
    bool     pin_threads_;                   // Pin thread to least busy cores
    int      recv_core_;                     // Pin receiver thread to specific core
    int      parser_core_;                   // Pin receiver thread to specific core
    int      builder_core_;                  // Pin receiver thread to specific core
    size_t   events_per_container_;          // Number of events per container
    size_t   offspill_events_per_container_; // Number of off-spill events per container
    bool     use_spill_condition_;           // Condition for batching on spill flags
    uint64_t eos_stream_id_;		     // End of subrun marker fragment id
    uint64_t rollover_subrun_interval_;	     // EWTs per subrun
    uint64_t subrun_number_{0};		     // Subrun number tracker
    
    // ----------------- Runtime -----------------
    std::atomic<size_t> event_count_{0}; // Number of events processed
    std::atomic<size_t> raw_frag_count_{0}; // Number of raw frags produced
    std::atomic<size_t> zs_frag_count_{0}; // Number of ZS frags produced
    std::atomic<size_t> ph_frag_count_{0}; // Number of PH frags produced
    std::atomic<size_t> batch_count_{0}; // Number of event batches produced
    std::atomic<uint64_t> total_bytes_received_{0}; // Total bytes received over TCP

    // Throughput
    using clock = std::chrono::steady_clock;

    std::chrono::time_point<clock> run_start_time_;

    std::chrono::nanoseconds receiver_active_{0};
    std::chrono::nanoseconds parser_active_{0};
    std::chrono::nanoseconds builder_active_{0};

    std::atomic<size_t> receiver_loops_{0};
    std::atomic<size_t> parser_loops_{0};
    std::atomic<size_t> builder_loops_{0};
    
    // getNext() wakeup control
    std::mutex batcher_cv_mutex_;
    std::condition_variable batcher_cv_;
    std::mutex builder_cv_mutex_;
    std::condition_variable builder_cv_;
    std::atomic<bool> receiver_done_{false};
    std::atomic<bool> batcher_done_{false};  
    std::atomic<bool> builder_done_{false};
    std::atomic<bool> stop_requested_{false};
    
    // TCP sockets
    int listen_fd_{-1}; // Listening TCP socket fd (created in start())
    int client_fd_{-1}; // Accepted client connection fd

    // TCP debug helper
    int tcpDebugLevel() const { return debug_level_ + 5; }
    
    // Queue capacity
    static constexpr size_t DEFAULT_BATCH_QCAP = 10000;
    static constexpr size_t DEFAULT_FRAG_QCAP = 10000;

    // Batcher -> Builder (batch events per container)
    using BatcherToBuildQ = boost::lockfree::spsc_queue<EventBatch, boost::lockfree::capacity<DEFAULT_BATCH_QCAP>>;
    // Builder -> getNext (sent fragment container)
    using BuilderToGNQ = boost::lockfree::spsc_queue<artdaq::Fragment*, boost::lockfree::capacity<DEFAULT_FRAG_QCAP>>;

    std::unique_ptr<BatcherToBuildQ> batcher_to_builder_queue_; 
    std::unique_ptr<BuilderToGNQ> builder_to_getNext_queue_;

    // Ring Buffer
    std::unique_ptr<EventRingBuffer> ring_;
    
    // ----------------- Threads -----------------
    std::thread receiver_thread_;
    std::thread parser_thread_;
    std::thread builder_thread_;

    // Thread functions
    void receiverThread_();
    void parserThread_();
    void builderThread_();

    // ----------------- Helpers -----------------

    // Resolves RB pointers to event data
    inline const uint8_t* resolveEventPointer(const EventView& e,
                                              size_t offset)
    {
      if (offset < e.event_rb_head_bytes)
        return e.event_rb_head_ptr + offset;
      
      return e.event_rb_wrap_ptr + (offset - e.event_rb_head_bytes);
    }

    // --- Get RMEM Max ---
    size_t get_rmem_max()
    {
      std::ifstream rmem("/proc/sys/net/core/rmem_max");
      size_t val = 0;

      if (!(rmem >> val)) {
        TLOG(TLVL_ERROR)
          << "[STM_BR][RECEIVER] Failed to read /proc/sys/net/core/rmem_max";
        return 0;
      }

      return val;
    }

    bool computeDatasetView(EventView& evt)
    {
      const uint16_t* hdr_words = evt.header;

      if (!hdr_words) {
        TLOG(TLVL_ERROR) << "[STM_BR][PARSER] Null event header pointer";
        return false;
      }

      if (hdr_words[anchor_start] != event_anchor_word_ ||
          hdr_words[anchor_end]   != event_anchor_word_) {

        TLOG(TLVL_ERROR)
          << "[STM_BR][PARSER] Anchor mismatch: start=0x"
          << std::hex << hdr_words[anchor_start]
          << " end=0x" << hdr_words[anchor_end]
          << " expected=0x" << event_anchor_word_;
        return false;
      }

      // ------------------------------------------------
      // Decode prescale flags
      // ------------------------------------------------

      uint16_t ps = hdr_words[PRESCALE];

      bool raw_prescaled = (ps >> 15) & 0x1;
      bool zs_prescaled  = (ps >> 7)  & 0x1;

      // ------------------------------------------------
      // Read header lengths
      // ------------------------------------------------

      uint16_t raw_len = hdr_words[RAW_LEN];
      uint16_t zs_regs = hdr_words[ZS_REGIONS];
      uint16_t zs_len  = hdr_words[ZS_LEN] + zs_regs * 2;
      uint16_t ph_num  = hdr_words[PH_NUM];
      uint16_t ph_len  = ph_num * 2;

      // ------------------------------------------------
      // Apply prescale overrides
      // ------------------------------------------------

      if (raw_prescaled)
        raw_len = 0;

      if (zs_prescaled) {
        zs_regs = 0;
        zs_len  = 0;
      }

      // ------------------------------------------------
      // Sanity checks
      // ------------------------------------------------

      if (raw_len > MAX_RAW_WORDS ||
          zs_len  > MAX_ZS_WORDS  ||
          ph_len  > MAX_PH_WORDS) {

        TLOG(TLVL_ERROR)
          << "[STM_BR][PARSER] Invalid dataset sizes: RAW=" << raw_len
          << " ZS=" << zs_len
          << " PH=" << ph_len;

        return false;
      }

      // ------------------------------------------------
      // Compute dataset offsets (same logic as original)
      // ------------------------------------------------

      size_t offset_words = 0;

      // RAW includes header
      size_t raw_offset_bytes = offset_words * 2;
      size_t raw_words_total  = raw_len + EVENT_HEADER_WORDS;
      offset_words += raw_words_total;

      size_t zs_offset_bytes = offset_words * 2;

      offset_words += zs_len;

      size_t ph_offset_bytes = offset_words * 2;

      // ------------------------------------------------
      // Helper lambda to resolve dataset pointer(s)
      // ------------------------------------------------

      auto resolveDataset = [&](DatasetView& ds,
                                size_t offset_bytes,
                                size_t size_words)
      {
        size_t size_bytes = size_words * sizeof(int16_t);

        const uint8_t* start_ptr =
          resolveEventPointer(evt, offset_bytes);

        size_t head_available =
          evt.event_rb_head_bytes > offset_bytes
          ? evt.event_rb_head_bytes - offset_bytes
          : 0;

        if (size_bytes <= head_available) {

          // Dataset fully in head region
          ds.ptr = reinterpret_cast<const int16_t*>(start_ptr);
          ds.size = size_words;
          ds.wrap_ptr = nullptr;
          ds.wrap_words = 0;
        }
        else {

          // Dataset crosses ring wrap
          size_t first_bytes = head_available;
          size_t second_bytes = size_bytes - first_bytes;

          ds.ptr = reinterpret_cast<const int16_t*>(start_ptr);
          ds.size = size_words;

          ds.wrap_ptr =
            reinterpret_cast<const int16_t*>(evt.event_rb_wrap_ptr);

          ds.wrap_words = second_bytes / sizeof(int16_t);
        }
      };

      // ------------------------------------------------
      // Resolve datasets
      // ------------------------------------------------

      resolveDataset(evt.raw, raw_offset_bytes, raw_words_total);
      resolveDataset(evt.zs,  zs_offset_bytes,  zs_len);
      resolveDataset(evt.ph,  ph_offset_bytes,  ph_len);

      return true;
    }
  
    // Fragment creation helper
    std::unique_ptr<artdaq::Fragment> makeFragment_(uint64_t fragment_id,
                                                    uint64_t sequence_id,
                                                    const int16_t* data,
                                                    size_t n_words)
    {
      const size_t n_bytes = n_words * sizeof(int16_t);
      auto frag = artdaq::Fragment::FragmentBytes(n_bytes);
      if (!frag) {
        TLOG(TLVL_ERROR) << "[STM_BR][makeFragment_] allocation failed";
        return nullptr;
      }
      
      frag->setUserType(FragmentType::STM);
      frag->setSequenceID(sequence_id);
      frag->setFragmentID(fragment_id);
      
      if (n_bytes > 0) {
        if (!data || !frag->dataBeginBytes()) {
          TLOG(TLVL_ERROR) << "[STM_BR][makeFragment_] null data pointer";
          return nullptr;
        }
        std::memcpy(frag->dataBeginBytes(), data, n_bytes);
      }
      
      return frag;
    }

    std::unique_ptr<artdaq::Fragment>
    makeFragmentWrapped_(uint64_t fragment_id,
                         uint64_t sequence_id,
                         const DatasetView& ds)
    {
      size_t total_words = ds.size;
      size_t total_bytes = total_words * sizeof(int16_t);

      auto frag = artdaq::Fragment::FragmentBytes(total_bytes);
      if (!frag) {
        TLOG(TLVL_ERROR) << "[STM_BR][makeFragmentWrapped_] allocation failed";
        return nullptr;
      }

      frag->setUserType(FragmentType::STM);
      frag->setSequenceID(sequence_id);
      frag->setFragmentID(fragment_id);

      uint8_t* dst = frag->dataBeginBytes();

      size_t first_words = total_words - ds.wrap_words;

      std::memcpy(dst,
                  ds.ptr,
                  first_words * sizeof(int16_t));

      std::memcpy(dst + first_words * sizeof(int16_t),
                  ds.wrap_ptr,
                  ds.wrap_words * sizeof(int16_t));

      return frag;
    }

    // -------------------------------------------
    
  };
} // namespace mu2e

#endif // STMTCPRECEIVER_HH
