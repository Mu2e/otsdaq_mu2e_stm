#include "otsdaq-mu2e-stm/FEInterfaces/ROCStoppingTargetMonitorInterface.h"

#include "otsdaq-core/Macros/InterfacePluginMacros.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FE-ROCStoppingTargetMonitorInterface"

//=========================================================================================
ROCStoppingTargetMonitorInterface::ROCStoppingTargetMonitorInterface(
    const std::string&       rocUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       theConfigurationPath)
    : ROCCoreVInterface(rocUID, theXDAQContextConfigTree, theConfigurationPath)
{
	INIT_MF("ROCStoppingTargetMonitorInterface");

	__MCOUT_INFO__("ROCStoppingTargetMonitorInterface instantiated with link: "
	               << linkID_ << " and EventWindowDelayOffset = " << delay_ << __E__);

	ConfigurationTree rocTypeLink =
	    Configurable::getSelfNode().getNode("ROCTypeLinkTable");

	STMParameter_1_ = rocTypeLink.getNode("NumberParam1").getValue<int>();

	STMParameter_2_ = rocTypeLink.getNode("TrueFalseParam2").getValue<bool>();

	// std::string STMParameter_3 =
	// rocTypeLink.getNode("STMMustBeUniqueParam1").getValue<std::string>();

	__FE_COUTV__(STMParameter_1_);
	__FE_COUTV__(STMParameter_2_);
	//    __FE_COUTV__(STMParameter_3);
}

//==========================================================================================
ROCStoppingTargetMonitorInterface::~ROCStoppingTargetMonitorInterface(void)
{
	// NOTE:: be careful not to call __FE_COUT__ decoration because it uses the
	// tree and it may already be destructed partially
	__COUT__ << FEVInterface::interfaceUID_ << " Destructor" << __E__;
}

//==================================================================================================
void ROCStoppingTargetMonitorInterface::writeROCRegister(uint16_t address,
                                                         uint16_t data_to_write)
{
	__FE_COUT__ << "Calling write ROC register: link number " << std::dec << linkID_
	            << ", address = " << address << ", write data = " << data_to_write
	            << __E__;

	bool requestAck = false;

	thisDTC_->WriteROCRegister(linkID_, address, data_to_write, requestAck);

	return;
}

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readROCRegister(uint16_t address)
{
	__FE_COUT__ << "Calling read ROC register: link number " << std::dec << linkID_
	            << ", address = " << address << __E__;

	int read_data = 0;

	try
	{
		read_data = thisDTC_->ReadROCRegister(linkID_, address, 1);
	}
	catch(...)
	{
		__COUT__ << "DTC failed DCS read" << __E__;
		read_data = -999;
	}

	return read_data;
}

//============================================================================================
void ROCStoppingTargetMonitorInterface::writeEmulatorRegister(uint16_t address,
                                                              uint16_t data_to_write)
{
	__FE_COUT__ << "Calling write ROC Emulator register: link number " << std::dec
	            << linkID_ << ", address = " << address
	            << ", write data = " << data_to_write << __E__;

	return;
}

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readEmulatorRegister(uint16_t address)
{
	__FE_COUT__ << "Calling read ROC Emulator register: link number " << std::dec
	            << linkID_ << ", address = " << address << __E__;

	return -1;
}

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readTimestamp() { return this->readRegister(12); }

//==================================================================================================
void ROCStoppingTargetMonitorInterface::writeDelay(uint16_t delay)
{
	this->writeRegister(21, delay);
	return;
}

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readDelay() { return this->readRegister(7); }

//==================================================================================================
int ROCStoppingTargetMonitorInterface::readDTCLinkLossCounter()
{
	return this->readRegister(8);
}

//==================================================================================================
void ROCStoppingTargetMonitorInterface::resetDTCLinkLossCounter()
{
	this->writeRegister(24, 0x1);
	return;
}

//==================================================================================================
void ROCStoppingTargetMonitorInterface::configure(void) try
{
	__MCOUT_INFO__(".... STM parameter 1 = " << STMParameter_1_);
	__MCOUT_INFO__(".... STM parameter 2 = " << STMParameter_2_);

	this->writeRegister(0, 1);
	__MCOUT_INFO__("... STM ROC Register 0, Write 1, Read " << this->readRegister(0)
	                                                        << __E__);

	this->writeRegister(1, 2);
	__MCOUT_INFO__("... STM ROC Register 1, Write 2, Read " << this->readRegister(1)
	                                                        << __E__);

	this->writeRegister(2, 3);
	__MCOUT_INFO__("... STM ROC Register 2, Write 3, Read " << this->readRegister(2)
	                                                        << __E__);

	this->writeRegister(3, 4);
	__MCOUT_INFO__("... STM ROC Register 3, Write 4, Read " << this->readRegister(3)
	                                                        << __E__);

	// __MCOUT_INFO__("......... Clear DCS FIFOs" << __E__);
	// this->writeRegister(0,1);
	// this->writeRegister(0,0);

	// setup needToResetAlignment using rising edge of register 22
	// (i.e., force synchronization of ROC clock with 40MHz system clock)
	//__MCOUT_INFO__("......... setup to synchronize ROC clock with 40 MHz clock
	// edge" << __E__);  this->writeRegister(22,0);  this->writeRegister(22,1);

	// this->writeDelay(delay_);
	// usleep(100);

	//__MCOUT_INFO__ ("........." << " Set delay = " << delay_ << ", readback = "
	//<< this->readDelay() << " ... ");

	//__FE_COUT__ << "Debugging ROC-DCS" << __E__;

	// unsigned int val;

	// read 6 should read back 0x12fc
	// for (int i = 0; i<1; i++)
	//{
	//	val = this->readRegister(6);
	//
	//	//__MCOUT_INFO__(i << " read register 6 = " << val << __E__);
	//	if(val != 4860)
	//	{
	//		__FE_SS__ << "Bad read not 4860! val = " << val << __E__;
	//		__FE_SS_THROW__;
	//	}
	//
	//	val = this->readDelay();
	//	//__MCOUT_INFO__(i << " read register 7 = " << val << __E__);
	//	if(val != delay_)
	//	{
	//		__FE_SS__ << "Bad read not " << delay_ << "! val = " << val <<
	//__E__;
	//		__FE_SS_THROW__;
	//	}
	//}

	// if(0) //random intense check
	//{
	//	highRateCheck();
	//}

	//__MCOUT_INFO__ ("......... reset DTC link loss counter ... ");
	// resetDTCLinkLossCounter();
}
catch(const std::runtime_error& e)
{
	__FE_MOUT__ << "Error caught: " << e.what() << __E__;
	throw;
}
catch(...)
{
	__FE_SS__ << "Unknown error caught. Check printouts!" << __E__;
	__FE_MOUT__ << ss.str();
	__FE_SS_THROW__;
}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::halt(void) {}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::pause(void) {}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::resume(void) {}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::start(std::string)  // runNumber)
{
}

//========================================================================================================================
void ROCStoppingTargetMonitorInterface::stop(void) {}

//========================================================================================================================
bool ROCStoppingTargetMonitorInterface::running(void) { return false; }

DEFINE_OTS_INTERFACE(ROCStoppingTargetMonitorInterface)
