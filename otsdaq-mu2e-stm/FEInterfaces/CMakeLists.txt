    
include(otsdaq::FEInterface) 

cet_build_plugin(ROCStoppingTargetMonitorInterface otsdaq::FEInterface LIBRARIES REG otsdaq_mu2e::ROCCore
)

install_headers()
install_source()
