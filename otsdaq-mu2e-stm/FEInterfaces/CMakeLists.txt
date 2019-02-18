include_directories(${MPI_INCLUDE_PATH})
include_directories(${CMAKE_BINARY_DIR}/artdaq)
    
simple_plugin(ROCStoppingTargetMonitorInterface "interface"
        FECore
	DTCInterface
)
    
simple_plugin(ROCStoppingTargetMonitorEmulator "interface"
	#otsdaq-mu2e_FEInterfaces_ROCCoreInterface_interface
	otsdaq-mu2e_FEInterfaces_ROCCoreVEmulator_interface
 )

install_headers()
install_source()
