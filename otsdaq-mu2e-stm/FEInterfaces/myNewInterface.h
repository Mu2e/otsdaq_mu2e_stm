#ifndef _ots_myNewInterface_h_
#define _ots_myNewInterface_h_

#include "otsdaq-components/FEInterfaces/FEOtsUDPTemplateInterface.h"

#include <string>

namespace ots
{
class myNewInterface : public FEOtsUDPTemplateInterface
{
  public:
	//myNewInterface     (unsigned int name=0, std::string daqHardwareType="daqHardwareType",	std::string firmwareType="firmwareType", const FEInterfaceConfigurationBase* configuration=0);
	myNewInterface (const std::string& interfaceUID, const ConfigurationTree& theXDAQContextConfigTree, const std::string& interfaceConfigurationPath);
	virtual ~myNewInterface (void);

	void configure (void);
	void halt (void);
	void pause (void);
	void resume (void);
	void start (std::string runNumber) override;
	void stop (void);
	bool running (void);

	int  universalRead (char* address, char* readValue) override;
	void universalWrite (char* address, char* writeValue) override;

	//void configureFEW     (void);
	//void configureDetector	(const DACStream& theDACStream);

	void testFunction (frontEndMacroInArgs_t argsIn, frontEndMacroOutArgs_t argsOut);

  private:
};
}

#endif
