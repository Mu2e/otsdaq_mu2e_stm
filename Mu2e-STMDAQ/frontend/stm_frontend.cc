// STM Frontend Header
#include "Mu2e-STMDAQ/frontend/stm_frontend.hh"

// STM frontend constructor
STMfrontend::STMfrontend() :
  xml_config_path(EnvVars::expand("${STM_XML}")),
  cfg(Config::getInstance(xml_config_path)),
  cpu(cpu_utils::getInstance(cfg,CpuRole::Standalone)),
  logger(std::make_shared<AsyncLogger>(cfg,cpu)),
  signal(std::make_shared<SignalHandler>(logger,cpu)),
  stm(std::make_shared<STMdata>(cfg,logger)){

  // Give cpu logger
  cpu->set_logger(logger);

  // If we are on the hardware control server
  if (stm->master_config.host == stm->fw_config.ctrl_srvr){
    // Initialise hardware manager
    hw = std::make_shared<HardwareManager>(logger,stm);
  }

  core_checkpoint = cpu->checkpoint();

  // Instance of operation manager
  if (!stop::should_stop()) om = std::make_shared<OperationManager>(cfg,logger,stm,signal);
  
  // If any operations have been selected...
  if (om && om->class_num() > 0){ 
    
    // Create buffer pool
    if (!stop::should_stop()) pool = std::make_shared<BufferPool>(cpu,logger,stm,om);
    
  }
}

// Start STMDAQ
void STMfrontend::start_stmdaq(){

  // Instance of thread manager
  if (!stop::should_stop()) {
    // If we reset the om, make a new one
    if (!om) om = std::make_shared<OperationManager>(cfg, logger, stm, signal);
    // If we previously cleared the pool, make a new one
    if (!pool) pool = std::make_shared<BufferPool>(cpu,logger,stm,om);
    // Thread manager
    tm = std::make_shared<ThreadManager>(cpu,logger,stm,signal,pool,om,hw);
  }

  return;

}

// Close threads
void STMfrontend::close_threads(){

  // Close thread manager
  if (tm) tm.reset();
  if (pool) pool.reset();
  if (om) om.reset();
  if (cpu) cpu->reset_to(core_checkpoint);

  return;
}

// Timed thread shutdown for error recovery
void STMfrontend::shutdown_threads(std::chrono::seconds timeout){
  if (tm) {
    tm->shutdown(timeout);
    tm.reset();
  }
  if (pool) pool.reset();
  if (om) om.reset();
  if (cpu) cpu->reset_to(core_checkpoint);
}

// Function so ots can call readout reset
void STMfrontend::run_reset_readout(){

  // If hw manager reset readout
  if (hw) hw->reset_readout();

  return;
}

// Wait until stop signal is called
void STMfrontend::wait(){
  
  while (!stop::should_stop()) {}

  return;

}

int STMfrontend::return_channel(){

  int channel = 1;
  if (stm->master_config.host == stm->fw_config.ctrl_srvr) channel = 0;

  return channel;
}

