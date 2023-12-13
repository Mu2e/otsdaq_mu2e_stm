#ifndef _otsdaq_mu2e_stm_ROCStoppingTargetMonitorInterface_h_
#define _otsdaq_mu2e_stm__ROCStoppingTargetMonitorInterface_h_

#include <sstream>
#include <string>
#include "dtcInterfaceLib/DTC.h"
#include "otsdaq-mu2e/ROCCore/ROCCoreVInterface.h"

namespace ots
{
class ROCStoppingTargetMonitorInterface : public ROCCoreVInterface
{
	// clang-format off
public:
	ROCStoppingTargetMonitorInterface(
	  const std::string&							rocUID,
	  const ConfigurationTree&						theXDAQContextConfigTree,
	  const std::string&							interfaceConfigurationPath);

	~ROCStoppingTargetMonitorInterface(void);

  	// state machine
  	//----------------
  	void 									configure				(void) override;
  	void 									halt					(void) override;
  	void 									pause					(void) override;
  	void 									resume					(void) override;
  	void 									start					(std::string runNumber) override;
  	void 									stop					(void) override;
  	bool 									running					(void) override;

  	// write and read to registers
  	virtual void 							writeEmulatorRegister	(uint16_t address, uint16_t data_to_write) override;
  	virtual uint16_t						readEmulatorRegister	(uint16_t address) override;

  	virtual void 							readEmulatorBlock		(std::vector<uint16_t>& data, uint16_t address, uint16_t numberOfReads, bool incrementAddress) override { }


  	// specific ROC functions
  	// virtual int  							readInjectedPulseTimestamp			(void) override;
  	// virtual void 							writeDelay				(uint16_t delay) override;  	// 5ns steps
  	// virtual int  							readDelay				(void) override;            	// 5ns steps

  	// virtual int  							readDTCLinkLossCounter	(void) override;
  	// virtual void 							resetDTCLinkLossCounter	(void) override;

  

  private:

  unsigned int STMParameter_1_;
  bool STMParameter_2_;
  std::string STMParameter_3_;
  unsigned int number_of_good_events_;
  unsigned int number_of_bad_events_;
  unsigned int number_of_empty_events_;
//  std::ofstream datafile_;
  unsigned int event_number_;


	// clang-format on
};
}  // namespace ots

#endif
