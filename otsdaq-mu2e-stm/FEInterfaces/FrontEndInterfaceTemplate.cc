#include "otsdaq/DetectorWriter/FrontEndInterfaceTemplate.h"

#include "otsdaq/DAQHardware/FrontEndFirmwareTemplate.h"
#include "otsdaq/DAQHardware/FrontEndHardwareTemplate.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FrontEndInterfaceTemplate"

//========================================================================================================================
FrontEndInterfaceTemplate::FrontEndInterfaceTemplate (std::string name)
    : FrontEndVirtualInterface (name)
{
	theFrontEndHardware_  = new FrontEndHardwareTemplate ();
	theFrontEndFirmware_  = new FrontEndFirmwareTemplate ();
	universalAddressSize_ = 4;
	universalDataSize_    = 4;

	__COUT__ << "FrontEndInterfaceTemplate instantiated with name: " << name << std::endl;
}

//========================================================================================================================
FrontEndInterfaceTemplate::~FrontEndInterfaceTemplate (void)
{
	delete theFrontEndHardware_;
	delete theFrontEndFirmware_;
}

//========================================================================================================================
void FrontEndInterfaceTemplate::configure (void)
{
}

//========================================================================================================================
void FrontEndInterfaceTemplate::halt (void)
{
}

//========================================================================================================================
void FrontEndInterfaceTemplate::pause (void)
{
}

//========================================================================================================================
void FrontEndInterfaceTemplate::resume (void)
{
}

//========================================================================================================================
void FrontEndInterfaceTemplate::start (std::string)  //runNumber)
{
}

//========================================================================================================================
void FrontEndInterfaceTemplate::stop (void)
{
}

//========================================================================================================================
bool FrontEndInterfaceTemplate::running (void)
{
	while (WorkLoop::continueWorkLoop_)
	{
		break;  //exit workloop
	}
	return false;
}

//========================================================================================================================
//universalRead
//	Must implement this function for Macro Maker to work with this interface.
//	When Macro Maker calls:
//		- address will be a [universalAddressSize_] byte long char array
//		- returnValue will be a [universalDataSize_] byte long char array
//		- expects return value of 0 on success and negative numbers on failure
int FrontEndInterfaceTemplate::universalRead (char* address, char* returnValue)
{
	//TODO - implement the read for this interface
	return -1;  //failed
}

//========================================================================================================================
//universalWrite
//	Must implement this function for Macro Maker to work with this interface.
//	When Macro Maker calls:
//		- address will be a [universalAddressSize_] byte long char array
//		- writeValue will be a [universalDataSize_] byte long char array
void FrontEndInterfaceTemplate::universalWrite (char* address, char* writeValue)
{
	//TODO - implement the write for this interface
}

DEFINE_OTS_INTERFACE (FrontEndInterfaceTemplate)
