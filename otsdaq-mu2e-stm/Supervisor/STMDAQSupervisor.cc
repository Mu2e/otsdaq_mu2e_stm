#include "otsdaq-mu2e-stm/Supervisor/STMDAQSupervisor.h"
#include "Mu2e-STMDAQ/frontend/stm_frontend.hh"

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(STMDAQSupervisor)

//==============================================================================
// Constructor
STMDAQSupervisor::STMDAQSupervisor(xdaq::ApplicationStub* s)
: CoreSupervisorBase(s)
{
  __SUP_COUT__ << "Constructor." << __E__;

  //clean up any leftover DQMS that didn't close properly 
  std::string commandResponse = StringMacros::exec("pgrep -f app.py | xargs kill -9");

  __SUP_COUT__ << "Constructed." << __E__;
}

//==============================================================================
// Destructor
STMDAQSupervisor::~STMDAQSupervisor(void)
{
  __SUP_COUT__ << "Destroying..." << __E__;
  __SUP_COUT__ << "Destructed." << __E__;
}

void STMDAQSupervisor::stopDAQ()
{
    __SUP_COUT__ << "Triggering stop..." << __E__;

    // Trigger stop for all threads
    stop::trigger_user_stop();

    stmFE_.reset();

    __SUP_COUT__ << "DAQ stopped cleanly." << __E__;
}

//==============================================================================
/// transitionConfiguring
void STMDAQSupervisor::transitionConfiguring(toolbox::Event::Reference e)
{
  __SUP_COUT__ << "transitionConfiguring" << __E__;

  CoreSupervisorBase::configureInit();

  failureReported_ = false;
  stop::reset_stops();

  if (configured_) {
    Config::getInstance().reinit();
  }
  configured_ = true;

  stmFE_ = std::make_shared<STMfrontend>();

  // Get the channel for DQM log file labelling
  int currentChannel = stmFE_->return_channel();
  //HPGe DQM start
  if (currentChannel == 0){
    std::string commandResponse = StringMacros::exec("nohup python $STM_DQM > $STM_DQM_LOG/dqmout_HPGe.log 2>&1 &");
  }
  //LaBr DQM start
  else{
    std::string commandResponse = StringMacros::exec("nohup python $STM_DQM > $STM_DQM_LOG/dqmout_LaBr.log 2>&1 &");
  }

  CoreSupervisorBase::transitionConfiguring(e);
}


//==============================================================================
/// transitionStarting
void STMDAQSupervisor::transitionStarting(toolbox::Event::Reference e)
{
  __SUP_COUT__ << "transitionStarting" << __E__;

  CoreSupervisorBase::transitionStarting(e);

  stop::reset_stops();
  // Start STMDAQ
  stmFE_->start_stmdaq();

}

//==============================================================================
/// transitionResuming
void STMDAQSupervisor::transitionResuming(toolbox::Event::Reference e)
{
  __SUP_COUT__ << "transitionResuming" << __E__;
  CoreSupervisorBase::transitionResuming(e);
}

//==============================================================================
/// transitionHalting
void STMDAQSupervisor::transitionHalting(toolbox::Event::Reference e)
{
    __SUP_COUT__ << "transitionHalting" << __E__;

    if (stop::should_critical_stop() && stmFE_) {
      __SUP_COUT__ << "Critical stop — deferring cleanup to background" << __E__;
      stop::trigger_user_stop();
      auto fe = std::move(stmFE_);
      std::thread([fe]() mutable {
        fe->shutdown_threads(std::chrono::seconds(30));
        fe.reset();
      }).detach();
    } else {
      stopDAQ();
    }

    std::string commandResponse = StringMacros::exec("pgrep -f app.py | xargs kill -9");

    CoreSupervisorBase::transitionHalting(e);
}

void STMDAQSupervisor::transitionStopping(toolbox::Event::Reference e)
{
    __SUP_COUT__ << "transitionStopping" << std::endl;

    stop::trigger_user_stop();

    // Close thread manager but leave FE
    if (stmFE_) {
      stmFE_->close_threads();
      stmFE_->run_reset_readout();
    }

    stop::reset_stops();

    CoreSupervisorBase::transitionStopping(e);
}

std::vector<SupervisorInfo::SubappInfo> STMDAQSupervisor::getSubappInfo(void)
{
  SupervisorInfo::SubappInfo info;

  info.name   = "STMDAQ";
  info.detail = ""; 
  info.lastStatusTime = time(0);
  info.progress       = 100;
  info.url        = "";
  info.class_name = "STMDAQ ";

  if (stop::should_critical_stop()){
    info.status = RunControlStateMachine::HALTED_STATE_NAME;
    if (!failureReported_) {
      failureReported_ = true;
      __SUP_SS__ << "STMDAQ critical stop detected. Please go to halted -> configured to restart." << __E__;
      __SUP_COUT_ERR__ << ss.str();
      theStateMachine_.setErrorMessage(ss.str());
      sendAsyncExceptionToGateway(ss.str(), false, false);
    }
  }
  
  else info.status = theStateMachine_.getCurrentStateName();

  std::vector<SupervisorInfo::SubappInfo> output;
  output.push_back(info);

  return output;
}

