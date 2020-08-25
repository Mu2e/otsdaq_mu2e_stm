link_directories($ENV{XDAQ_LIB})

cet_make_exec(DemoInterfaceTestMain
  LIBRARIES
		ConfigurationInterface
		DAQHardware
		FECore
		ConfigurationDataFormats
        	WorkLoopManager
		NetworkUtilities
		otsdaq-components_FEInterfaces_FEOtsUDPTemplateInterface_interface
		 ${XDAQ_BASIC_LIB_LIST}
		 ${XDAQ_XERCES-C}
)

install_headers()
install_source()
