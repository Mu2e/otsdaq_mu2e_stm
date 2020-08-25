include_directories(${MPI_INCLUDE_PATH})
include_directories(${CMAKE_BINARY_DIR}/artdaq)
    
simple_plugin(ROCStoppingTargetMonitorInterface "interface"
     ROCCore
     ConfigurationInterface
     TableCore
     Macros
     FECore
     Configurable
     DTCInterface
     WorkLoopManager
     artdaq-core_Utilities
     ${XDAQ_XERCES-C}
     ${MF_MESSAGELOGGER}
     pthread
)

install_headers()
install_source()
