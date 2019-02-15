#ifndef _ots_ROCStoppingTargetMonitorEmulator_h_
#define _ots_ROCStoppingTargetMonitorEmulator_h_

#include "otsdaq-mu2e/FEInterfaces/ROCCoreVEmulator.h"


namespace ots
{


class ROCStoppingTargetMonitorEmulator : public ROCCoreVEmulator
{

public:
	ROCStoppingTargetMonitorEmulator (const std::string& rocUID,
			const ConfigurationTree& theXDAQContextConfigTree,
			const std::string& interfaceConfigurationPath);

	~ROCStoppingTargetMonitorEmulator(void);

	void 					writeRegister		(unsigned address, unsigned data_to_write) override;
	int 					readRegister		(unsigned address) override;

	bool 					emulatorWorkLoop	(void) override;

	enum{
		ADDRESS_FIRMWARE_VERSION = 5,
		ADDRESS_MYREGISTER = 0x65,
	};

	//	temperature--
	class Thermometer
	{
	private:
		double mnoiseTemp;
	public:
		void noiseTemp(double intemp)
		{
			mnoiseTemp = (double) intemp + 0.05*(intemp*((double) rand() / (RAND_MAX))-0.5);
			return;
		}
		double GetBoardTempC()
		{
			return mnoiseTemp;
		}
	};


	Thermometer 	temp1_;
	double 			inputTemp_;

};

}

#endif
