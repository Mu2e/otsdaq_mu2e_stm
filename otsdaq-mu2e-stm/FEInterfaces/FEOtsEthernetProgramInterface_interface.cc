#include "otsdaq-demo/FEInterfaces/FEOtsEthernetProgramInterface.h"
#include "otsdaq-core/MessageFacility/MessageFacility.h"
#include "otsdaq-core/Macros/CoutMacros.h"
#include "otsdaq-core/Macros/InterfacePluginMacros.h"
#include <iostream>
#include <set>
#include <dirent.h> /*DIR and dirent*/

using namespace ots;

#undef 	__MF_SUBJECT__
#define __MF_SUBJECT__ "FE-FEOtsEthernetProgramInterface"


#define PROGRAM_FILE_PATH	std::string(getenv("OTS_FIRMWARE_PROGRAM_FILE_PATH")) + "/"


/////////////////////////////////=======================================
//ADDRESS SPACE
//	http://otsdaq.fnal.gov/docs/oei_address_space.html
															//field size (bits)
#define UDP_CORE_BLOCK_ADDRESS 		((uint64_t)(0x2) << 32)
#define FLASH_COMMAND_BASE			0
// 0 -- START_FLASH_COMMAND_TRIGGER
// 1 -- FLASH_BASE_ADDRESS
// 2 -- FLASH_DATA_SIZE
// 3 -- FLASH_WRITE_MODE
#define FLASH_WRITE_DATA			4
#define FLASH_WRITE_STATUS			9

//end ADDRESS SPACE
/////////////////////////////////=======================================


//========================================================================================================================
FEOtsEthernetProgramInterface::FEOtsEthernetProgramInterface(const std::string& interfaceUID, const ConfigurationTree& theXDAQContextConfigTree, const std::string& interfaceConfigurationPath)
: Socket               (
		theXDAQContextConfigTree.getNode(interfaceConfigurationPath).getNode("HostIPAddress").getValue<std::string>()
		, theXDAQContextConfigTree.getNode(interfaceConfigurationPath).getNode("HostPort").getValue<unsigned int>())
, FEOtsUDPTemplateInterface(interfaceUID,theXDAQContextConfigTree,interfaceConfigurationPath)
{
	//register FE Macro Functions
	registerFEMacroFunction("getListOfProgramFiles" /*feMacroName*/,
			static_cast<FEVInterface::frontEndMacroFunction_t>(&FEOtsEthernetProgramInterface::getListOfProgramFiles) /*feMacroFunction*/,
			std::vector<std::string>{} /*namesOfInputArgs*/,
			std::vector<std::string>{"listOfProgramFiles"}  /*namesOfOutputArgs*/,
			10 /*requiredUserPermissions*/);

	//for testing FE Macro Functions
	std::vector<frontEndMacroInArg_t> argsIn;
	//	argsIn.push_back(frontEndMacroInArg_t("arg1","val1"));
	//	argsIn.push_back(frontEndMacroInArg_t("arg2","val2"));
	//	argsIn.push_back(frontEndMacroInArg_t("arg3","val3"));


	__COUT__ << std::endl;
	__COUT__ << std::endl;
	__COUT__ << __COUT_HDR_P__ << "# of args = " << argsIn.size() << std::endl;
	for(auto &argIn:argsIn)
		__COUT__ << argIn.first << ": " << argIn.second << std::endl;


	std::vector<std::string> returnStrings;
	std::vector<frontEndMacroOutArg_t> argsOut;

	std::string outputArgs = "listOfProgramFiles";//"oarg1,oarg2,";
	{
		std::istringstream inputStream(outputArgs);
		std::string argName;
		while (getline(inputStream, argName, ','))
		{
			__COUT__ << "argName " << argName << std::endl;

			returnStrings.push_back(std::string("test"));
			argsOut.push_back(FEVInterface::frontEndMacroOutArg_t(
					argName,
					returnStrings[returnStrings.size()-1]));
			//
			//			__COUT__ << argsOut[argsOut.size()-1].first << std::endl;
			//			__COUT__ << argsOut[argsOut.size()-1].second << std::endl;
		}
	}

	auto mapOfFEMacroIt = mapOfFEMacroFunctions_.find("getListOfProgramFiles");
	if(mapOfFEMacroIt != mapOfFEMacroFunctions_.end())
	{
		(this->*(mapOfFEMacroIt->second.macroFunction_))(argsIn,argsOut);
		__COUT__ << "Made it " << std::endl;
		for(auto &arg:argsOut)
			__COUT__ << arg.first << ": " << arg.second << std::endl;
	}
}

//========================================================================================================================
FEOtsEthernetProgramInterface::~FEOtsEthernetProgramInterface(void)
{}

//========================================================================================================================
void FEOtsEthernetProgramInterface::configure(void)
{
	FEOtsUDPTemplateInterface::configure();
//	__COUT__ << "configure" << std::endl;
//	__COUT__ << "Clearing receive socket buffer: " << OtsUDPHardware::clearReadSocket() << " packets cleared." << std::endl;
//
//	std::string sendBuffer;
//	std::string recvBuffer;
//
//	__COUT__ << "Setting Destination IP: " <<
//			theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("StreamToIPAddress").getValue<std::string>()
//			<< std::endl;
//	__COUT__ << "And Destination Port: " <<
//			theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("StreamToPort").getValue<unsigned int>()
//			<< std::endl;
//
//	sendBuffer.resize(0);
//	OtsUDPFirmwareCore::setupBurstDestination(sendBuffer,
//			theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("StreamToIPAddress").getValue<std::string>(),
//			theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("StreamToPort").getValue<uint64_t>()
//	);
//	OtsUDPHardware::write(sendBuffer);
//
//	//
//	//
//	__COUT__ << "Reading back burst dest MAC/IP/Port: "  << std::endl;
//	sendBuffer.resize(0);
//	OtsUDPFirmwareCore::readBurstDestinationMAC(sendBuffer);
//	OtsUDPHardware::read(sendBuffer,recvBuffer);
//	sendBuffer.resize(0);
//	OtsUDPFirmwareCore::readBurstDestinationIP(sendBuffer);
//	OtsUDPHardware::read(sendBuffer,recvBuffer);
//	sendBuffer.resize(0);
//	OtsUDPFirmwareCore::readBurstDestinationPort(sendBuffer);
//	OtsUDPHardware::read(sendBuffer,recvBuffer);
//
//
//	sendBuffer.resize(0);
//	OtsUDPFirmwareCore::read(sendBuffer,0x5);
//	OtsUDPHardware::read(sendBuffer,recvBuffer);
//
//	//Run Configure Sequence Commands
//	runSequenceOfCommands("LinkToConfigureSequence");
//
	__COUT__ << "Done with configuring."  << std::endl;
}

//========================================================================================================================
//void FEOtsEthernetProgramInterface::configureDetector(const DACStream& theDACStream)
//{
//	__COUT__ << "\tconfigureDetector" << std::endl;
//}

////========================================================================================================================
//void FEOtsEthernetProgramInterface::halt(void)
//{
//	__COUT__ << "\tHalt" << std::endl;
//	stop();
//}
//
////========================================================================================================================
//void FEOtsEthernetProgramInterface::pause(void)
//{
//	__COUT__ << "\tPause" << std::endl;
//	stop();
//}
//
////========================================================================================================================
//void FEOtsEthernetProgramInterface::resume(void)
//{
//	__COUT__ << "\tResume" << std::endl;
//	start("");
//}
//
////========================================================================================================================
//void FEOtsEthernetProgramInterface::start(std::string )//runNumber)
//{
//	__COUT__ << "\tStart" << std::endl;
//
//
//	//Run Start Sequence Commands
//	runSequenceOfCommands("LinkToStartSequence");
//
//	OtsUDPHardware::write(OtsUDPFirmwareCore::startBurst());
//}
//
////========================================================================================================================
//void FEOtsEthernetProgramInterface::stop(void)
//{
//	__COUT__ << "\tStop" << std::endl;
//
//	//Run Stop Sequence Commands
//
//	runSequenceOfCommands("LinkToStopSequence");
//
//	OtsUDPHardware::write(OtsUDPFirmwareCore::stopBurst());
//}
//
////========================================================================================================================
//bool FEOtsEthernetProgramInterface::running(void)
//{
//	__COUT__ << "\running" << std::endl;
//
//	//		//example!
//	//		//play with array of 8 LEDs at address 0x1003
//
//	//
//	//	bool flashLEDsWhileRunning = false;
//	//	if(flashLEDsWhileRunning)
//	//	{
//	//		std::string sendBuffer;
//	//		int state = -1;
//	//		while(WorkLoop::continueWorkLoop_)
//	//		{
//	//			//while running
//	//			//play with the LEDs at address 0x1003
//	//
//	//			++state;
//	//			if(state < 8)
//	//			{
//	//				sendBuffer.resize(0);
//	//				OtsUDPFirmwareCore::writeAdvanced(sendBuffer, 0x1003,1<<state);
//	//				OtsUDPHardware::write(sendBuffer);
//	//			}
//	//			else if(state%2 == 1 && state < 11)
//	//			{
//	//				sendBuffer.resize(0);
//	//				OtsUDPFirmwareCore::writeAdvanced(sendBuffer, 0x1003, 0xFF);
//	//				OtsUDPHardware::write(sendBuffer);
//	//			}
//	//			else if(state%2 == 0 && state < 11)
//	//			{
//	//				sendBuffer.resize(0);
//	//				OtsUDPFirmwareCore::writeAdvanced(sendBuffer, 0x1003,0);
//	//				OtsUDPHardware::write(sendBuffer);
//	//			}
//	//			else
//	//				state = -1;
//	//
//	//			sleep(1);
//	//		}
//	//	}
//
//	return false;
//}

//========================================================================================================================
//getListOfProgramFiles
//	0 args in
//	1 args out
//		listOfProgramFiles = comma-separated list of programmable file names
//
// Note: path is from environment variable OTS_FIRMWARE_PROGRAM_FILE_PATH
void FEOtsEthernetProgramInterface::getListOfProgramFiles(frontEndMacroInArgs_t argsIn,
		frontEndMacroOutArgs_t argsOut)
{
	std::string dirpath = PROGRAM_FILE_PATH;
	DIR *pDIR;
	struct dirent *entry;
	bool isDir;
	std::set<std::string> listOfProgramFiles;

	if( (pDIR = opendir(dirpath.c_str())) )
	{
		//add every program file to return set
		//	valid program files are *.bin

		while((entry = readdir(pDIR)))
		{
			//__COUT__ << int(entry->d_type) << " " << entry->d_name << "\n" << std::endl;
			if( entry->d_name[0] != '.' && (entry->d_type == 0 || //0 == UNKNOWN (which can happen - seen in SL7 VM)
					entry->d_type == 4 || entry->d_type == 8))
			{
				__COUT__ << int(entry->d_type) << " " << entry->d_name <<
						" " << std::string(entry->d_name).find(".bin") << " " <<
						strlen(entry->d_name)-4 << "\n" << std::endl;

				isDir = false;

				if(entry->d_type == 0)
				{
					//unknown type .. determine if directory
					DIR *pTmpDIR = opendir((dirpath + entry->d_name).c_str());
					if(pTmpDIR)
					{
						isDir = true;
						closedir(pTmpDIR);
					}
					//else //assume file
				}
				else if(entry->d_type == 4)
					isDir = true;

				if(!isDir && //isBinFile
					std::string(entry->d_name).find(".bin") == strlen(entry->d_name)-4)
					listOfProgramFiles.insert(entry->d_name);
			}
		}

		closedir(pDIR);
	}
	else
	{
		__COUT__ << "Failed to access directory contents!" << std::endl;
	}

	auto &returnString = FEVInterface::getFEMacroOutputArgument(argsOut,"listOfProgramFiles");
	returnString = "";
	for(const auto &name:listOfProgramFiles)
	{
		if(returnString.size()) returnString += ",";
		__COUT__ << "name " << name << std::endl;
		returnString += name;
	}
}

//========================================================================================================================
//loadProgramFile
//	1 args in
//		programFile = filename of programmable file
//	1 args out
//		listOfProgramFiles = comma-separated list of programmable file names
void FEOtsEthernetProgramInterface::loadProgramFile(frontEndMacroInArgs_t argsIn, frontEndMacroOutArgs_t argsOut)
{
	//Steps:
	//	- open file
	//

	std::string file = "";
	//remove special characters
	{
		bool prevWasDot = false;
		std::string sourceStr = getFEMacroInputArgument(argsIn,"programFile");
		for(const auto& c : sourceStr)
		{
			if(c == '.')
			{
				if(!prevWasDot && //previous source char was not dot
						file.size() && //there are other chars in filename
						file[file.size()-1] != '.') //the prev char in filename is not a '.'
						file += c;
				//else skip (prevent multiple dots)

				prevWasDot = true;
			}
			else if((c >= 'a' && c <= 'z') ||
					(c >= 'A' && c <= 'Z') ||
					(c >= '0' && c <= '9') ||
					c == '_' || c == '-')
				file += c;
			//else skip (non-alphanumeric - _ .)
		}
	}
	std::string path = PROGRAM_FILE_PATH + file;

	__COUT__ << "Programmable file path: " << path << std::endl;

	FILE *bitstream = fopen(path.c_str(), "rb");
	if (!bitstream) {
		__SS__ << "Failed to read bitsream";
		__COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}

	//keep as placeholder for potentially handling mcs files (in addition to bin files)
	/*if (path.Find(".mcs") != -1) {
		//if the path has .mcs in it assume it is an mcs file and generate a bin file
		FILE * binfile = fopen("generated_from_mcs.bin", "w");
		char line[100];
		char lineoutput[16];
		fgets(line, 100, bitstream);//skip first line
		int i;
		bool has_d;
		//char dstr[100];
		while (fgets(line, 100, bitstream)) {
			i = 9;
			has_d = 0;
			while (i < 41) {
				lineoutput[(i - 9) / 2] = hextoint(line[i]) * 16 + hextoint(line[i + 1]);
				if (hextoint(line[i + 1]) == 13) has_d = 1;
				i += 2;
			}
			if (has_d) {
				LineOut(line);
				break;
			}
			fwrite(lineoutput, sizeof(char), 16, binfile);
		}
		fclose(bitstream);
		fclose(binfile);
		//update the bitsream variable for the rest of the function to the .bin file
		bitstream = fopen("generated_from_mcs.bin", "r");
	}*/


	fseek(bitstream, 0, SEEK_END);
	uint64_t bSize = ftell(bitstream)/8;//divide by 8 to get number of qwds
	rewind(bitstream);

	__COUT__ << "Programmable file size [quad-words]: " << bSize << std::endl;

	std::string sendBuffer;
	std::string recvBuffer;
	uint64_t 	recvQuadWord;

	//send reset command to Ethernet block which will reset flash block
	OtsUDPFirmwareCore::softEthernetReset(sendBuffer);
	OtsUDPHardware::write(sendBuffer);
	OtsUDPFirmwareCore::clearEthernetReset(sendBuffer);
	OtsUDPHardware::write(sendBuffer);

	//sleep to allow reset to complete
	//sleep(2); //seconds

	//setup address and flash write mode
	{
		std::vector<uint64_t> dataVec = {
				0, //first (ie at addr 0) quadwrod all 0 because it is the start-command trigger
				0, //write address(second qwd)
				bSize, //size
				3 //mode is erase then write
		};

		sendBuffer.resize(0);
		OtsUDPFirmwareCore::writeAdvanced(sendBuffer,
				UDP_CORE_BLOCK_ADDRESS /*block*/ | FLASH_COMMAND_BASE /*addr*/,
				dataVec /*data*/);
		OtsUDPHardware::write(sendBuffer);
	}



	//now actually send bitstream data
	const size_t page_size = 512;
	const size_t MAX_TCP_SIZE = 5000;

	char page_data[page_size];
	unsigned char next_page_recv_buff[MAX_TCP_SIZE];
	int next_page_recv_len;
	int page = 0;

	//int num_page_ready_returns = 0;
	//int bytesoff = 0;
	//int thisbytesoff, returnaddr;
	size_t result;
	while (1)
	{
		if (page % 5120 == 0) //debug/status print out
		{
			char str[100];
			sprintf(str, "Place: %lX  Page: %d", ftell(bitstream), page);
			//sprintf(str, "Place: %X  Page: %d  Returns: %d", ftell(bitstream),page,num_page_ready_returns);
			__COUT__ << str << std::endl;;
		}

		//if (page == 512) break;

		//get block of data from file
		result = fread(page_data, 1, page_size, bitstream);
		if (result != page_size) //if block of data is not full page size, handle it.
		{
			if (result == 0)
			{
				__COUT__ << "Done" << std::endl;
				break;
			}
			else //0-fill
			{
				for (unsigned int i = result; i < page_size; i++)
					page_data[i] = 0;
			}
		}

		//at this point have block of data
		//check if ready to send

		if (page > 1) //if 3rd page and beyond, handle differently
		{
			//after writing first two pages, now waits to write next page
			//	until gets response from FPGA indicating to send next page

			//check status
			sendBuffer.resize(0);
			OtsUDPFirmwareCore::read(sendBuffer,
					UDP_CORE_BLOCK_ADDRESS /*block*/ | FLASH_WRITE_STATUS /*addr*/);

			try
			{
				OtsUDPHardware::read(sendBuffer,recvQuadWord);
			}
			catch(...)
			{
				//close file before re-throwing
				std::fclose(bitstream);
				throw;
			}

			if (recvQuadWord != 0x01)
			{
				//no reply or incorrect reply
				if (recvQuadWord == 3)
					__COUT_ERR__ << "No active command" << std::endl;
				else
					__COUT_ERR__ << "Error in writing multiple pages" << std::endl;

				break;
			}
			//		else {
			//			num_page_ready_returns++;
			//			returnaddr = 0 | (int)next_page_recv_buff[21]<<16 | (int)next_page_recv_buff[20]<<8 | (int)next_page_recv_buff[19];
			//			thisbytesoff = ftell(bitstream) - returnaddr;
			//			if (thisbytesoff != bytesoff) {
			//				sprintf(str, "New offset: %X", thisbytesoff);
			//				__COUT__ << str << std::endl;
			//				bytesoff = thisbytesoff;
			//			}
			//		}

			//__COUT__ << "Got next page response" << std::endl;
		}

		//ready, so send data
		sendBuffer.resize(0);
		OtsUDPFirmwareCore::writeAdvanced(sendBuffer,
				UDP_CORE_BLOCK_ADDRESS /*block*/ | FLASH_WRITE_DATA /*addr*/,
				page_data /*data*/,
				page_size/8 /*size in quadwords*/,
				OtsUDPFirmwareCore::FIFO_ADDRESS_CMD_TYPE /*type option*/);
		OtsUDPHardware::write(sendBuffer);



//		int counter = 0;
//		while (counter < page_size) {
//			fprintf(outfile, "%8.8X  ", ftell(bitstream)-0x200+counter);
//			while (1) {
//				fprintf(outfile, "%2.2hhX ", page_data[counter]);
//				counter++;
//				if((counter%16)==0){
//					break;
//				}
//			}
//			fprintf(outfile, "\n");
//		}



		//send go command after first page written
		if(page == 0)
		{
			sendBuffer.resize(0);
			OtsUDPFirmwareCore::writeAdvanced(sendBuffer,
					UDP_CORE_BLOCK_ADDRESS /*block*/ | FLASH_COMMAND_BASE /*addr*/,
					1 /*data*/);
			OtsUDPHardware::write(sendBuffer);

			__COUT__ << "Sending \"GO\" command..." << std::endl;
		}

		page++;
	}

//	fclose(outfile);
	std::fclose(bitstream);
}

DEFINE_OTS_INTERFACE(FEOtsEthernetProgramInterface)
