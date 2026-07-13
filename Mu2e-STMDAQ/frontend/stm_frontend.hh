#ifndef STM_FRONTEND_hh_
#define STM_FRONTEND_hh_

// Thread Manager header                                                 
#include "Mu2e-STMDAQ/processing/thread_manager.hh"

// Frontend class
class STMfrontend {

private:

  // DAQ timer
  Timer t;

  // Load the config xml
  const std::string xml_config_path;

  // Config class
  Config& cfg;
  
  // Store reference to CPU utils instance
  //const std::shared_ptr<cpu_utils> cpu;
  std::shared_ptr<cpu_utils> cpu;
  
  // Async Logger
  //const std::shared_ptr<AsyncLogger> logger;
  std::shared_ptr<AsyncLogger> logger;

  // Signal Handler
  //const std::shared_ptr<SignalHandler> signal;
  std::shared_ptr<SignalHandler> signal;
  
  // STM data info
  //const std::shared_ptr<STMdata> stm;  
  std::shared_ptr<STMdata> stm;  
  
  // Hardware manager
  std::shared_ptr<HardwareManager> hw;

  // Persistent cores between run start/stops
  size_t core_checkpoint;
  
  // Operation Manager
  std::shared_ptr<OperationManager> om;

  // Buffer pool
  std::shared_ptr<BufferPool> pool;

  // Thread Manager
  std::shared_ptr<ThreadManager> tm;
    
public:
  
  // STM frontend constructor
  STMfrontend();

  // Destructor
  ~STMfrontend() {
    //std::cout << "STMfrontend destructor called.\n";    
    logger->log("STMfrontend destructor called.",1);    

    tm.reset();
    pool.reset();
    om.reset();
    hw.reset();
    stm.reset();
    signal.reset();
    cpu->clear_logger();
    logger.reset();
    cpu->reset_to(0);
  }
  
  // Start STM DAQ
  void start_stmdaq();
  
  // Wait until stop signal is called
  void wait();

  void start_DQM();

  void close_threads();

  // Timed thread shutdown for error recovery — detaches stuck threads
  void shutdown_threads(std::chrono::seconds timeout);

  void run_reset_readout();

  //Get 0 or 1 to know what channel we are on
  int return_channel();

};

#endif 



