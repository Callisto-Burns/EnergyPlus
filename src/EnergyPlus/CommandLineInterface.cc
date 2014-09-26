//Standard C++ library
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libproc.h>
#include <unistd.h>

// CLI Headers
#include <ezOptionParser.hpp>
//#include <io.cpccFileSystemMini.h>

// Project headers
#include <CommandLineInterface.hh>
#include <DisplayRoutines.hh>
#include <DataStringGlobals.hh>
#include <EnergyPlus.hh>
#include <InputProcessor.hh>
#include <OutputProcessor.hh>
#include <OutputReportTabular.hh>
#include <OutputReports.hh>
#include <SimulationManager.hh>
#include <SolarShading.hh>
#include <UtilityRoutines.hh>

namespace EnergyPlus{

namespace CommandLineInterface{

	using namespace DataStringGlobals;
	using namespace InputProcessor;
	using namespace SimulationManager;
	using namespace OutputReportTabular;
	using namespace OutputProcessor;
	using namespace SolarShading;
	using namespace ez;

	std::string outputAuditFileName;
	std::string outputBndFileName;
	std::string outputDxfFileName;
	std::string outputEioFileName;
	std::string outputEndFileName;
	std::string outputErrFileName;
	std::string outputEsoFileName;
	std::string outputMtdFileName;
	std::string outputMddFileName;
	std::string outputMtrFileName;
	std::string outputRddFileName;
	std::string outputShdFileName;
	std::string outputTblCsvFileName;
	std::string outputTblHtmFileName;
	std::string outputTblTabFileName;
	std::string outputTblTxtFileName;
	std::string outputTblXmlFileName;
	std::string inputIdfFileName;
	std::string inputIddFileName;
	std::string inputWeatherFileName;
	std::string outputAdsFileName;
	std::string outputDfsFileName;
	std::string outputDelightFileName;
	std::string outputMapTabFileName;
	std::string outputMapCsvFileName;
	std::string outputMapTxtFileName;
	std::string outputEddFileName;
	std::string outputIperrFileName;
	std::string outputDbgFileName;
	std::string outputSlnFileName;
	std::string outputSciFileName;
	std::string outputWrlFileName;
	std::string outputZszCsvFileName;
	std::string outputZszTabFileName;
	std::string outputZszTxtFileName;
	std::string outputSszCsvFileName;
	std::string outputSszTabFileName;
	std::string outputSszTxtFileName;
	std::string outputScreenCsvFileName;
	std::string EnergyPlusIniFileName;
	std::string inStatFileName;
	std::string TarcogIterationsFileName;
	std::string eplusADSFileName;

	std::string
	returnFileName( std::string const& filename )
	{
	    return {std::find_if(filename.rbegin(), filename.rend(), [](char c) { return c == pathChar; }).base(),
	    	  	filename.end()-4};
	}

	std::string
	returnDirPathName( std::string const& filename )
	{
		std::string::const_reverse_iterator pivot = std::find( filename.rbegin(), filename.rend(), pathChar );
			    return pivot == filename.rend()
			        ? filename
			        : std::string( filename.begin(), pivot.base() - 1 );
	}

	int
	ProcessArgs(int argc, const char * argv[])
	{
		ezOptionParser opt;

		opt.overview = VerString;
		opt.example = "EnergyPlus -i InputFile.idf -e Energy+.idd -w WeatherFile.epw -o";

		opt.syntax = "EnergyPlus [options]";

		opt.add("", 0, 0, 0, "Display this message", "-h", "--help");

		opt.add("", 0, 0, 0, "Print version information", "-v", "--version");

		opt.add("in.idf", 0, 1, 0, "Input Definition File (IDF) file path (default \".\\in.idf\")", "-i", "--idf");

		opt.add("in.epw", 0, 1, 0, "EnergyPlus Weather (EPW) file path (default \".\\in.epw\")", "-w", "--weather");

		opt.add("Energy+.idd", 0, 1, 0, "Input Data Dictionary (IDD) file path (default \".\\Energy+.idd\")", "-e", "--idd");

		opt.add("", 0, 0, 0, "Rename output files to using the IDF and EPW file names", "-o", "--output");

		// Parse arguments
		opt.parse(argc, argv);

		// print arguments parsed (useful for debugging)

		/*std::string pretty;
		opt.prettyPrint(pretty);
		std::cout << pretty << std::endl;*/

		std::string usage, idfFileNameOnly, idfDirPathName;
		std::string weatherFileNameOnly, weatherDirPathName;
		opt.getUsage(usage);

		std::string outputFilePrefix;

		opt.get("-i")->getString(inputIdfFileName);

		opt.get("-w")->getString(inputWeatherFileName);

		opt.get("-e")->getString(inputIddFileName);

		idfFileNameOnly = returnFileName(inputIdfFileName);
		idfDirPathName = returnDirPathName(inputIdfFileName);

		weatherFileNameOnly = returnFileName(inputWeatherFileName);
		weatherDirPathName = returnDirPathName(inputWeatherFileName);

	//	std::cout<<"\n Name of the file = "<<idfFileNameOnly<<'\t'<<" and directory path name = "<<idfDirPathName<<std::endl;
	//  std::cout<<"\n Name of the file = "<<weatherFileNameOnly<<'\t'<<" and directory path name = "<<weatherDirPathName<<std::endl;

	    if (opt.isSet("-o")) outputFilePrefix = idfFileNameOnly + "_" + weatherFileNameOnly + "_";

	    else outputFilePrefix = "eplus";

		outputAuditFileName = outputFilePrefix + "out.audit";
		outputBndFileName = outputFilePrefix + "out.bnd";
		outputDxfFileName = outputFilePrefix + "out.dxf";
		outputEioFileName = outputFilePrefix + "out.eio";
		outputEndFileName = outputFilePrefix + "out.end";
		outputErrFileName = outputFilePrefix + "out.err";
		outputEsoFileName = outputFilePrefix + "out.eso";
		outputMtdFileName = outputFilePrefix + "out.mtd";
		outputMddFileName = outputFilePrefix + "out.mdd";
		outputMtrFileName = outputFilePrefix + "out.mtr";
		outputRddFileName = outputFilePrefix + "out.rdd";
		outputShdFileName = outputFilePrefix + "out.shd";
		outputTblCsvFileName = outputFilePrefix + "tbl.csv";
		outputTblHtmFileName = outputFilePrefix + "tbl.htm";
		outputTblTabFileName = outputFilePrefix + "tbl.tab";
		outputTblTxtFileName = outputFilePrefix + "tbl.txt";
		outputTblXmlFileName = outputFilePrefix + "tbl.xml";
		outputAdsFileName = outputFilePrefix + "ADS.out";
		outputDfsFileName = outputFilePrefix + "out.dfs";
		outputDelightFileName = outputFilePrefix + "out.delightdfdmp";
		outputMapTabFileName = outputFilePrefix + "map.tab";
		outputMapCsvFileName = outputFilePrefix + "map.csv";
		outputMapTxtFileName = outputFilePrefix + "map.txt";
		outputEddFileName = outputFilePrefix + "out.edd";
		outputIperrFileName = outputFilePrefix + "out.iperr";
		outputDbgFileName = outputFilePrefix + "out.dbg";
		outputSlnFileName = outputFilePrefix + "out.sln";
		outputSciFileName = outputFilePrefix + "out.sci";
		outputWrlFileName = outputFilePrefix + "out.wrl";
		outputZszCsvFileName = outputFilePrefix + "zsz.csv";
		outputZszTabFileName = outputFilePrefix + "zsz.tab";
		outputZszTxtFileName = outputFilePrefix + "zsz.txt";
		outputSszCsvFileName = outputFilePrefix + "ssz.csv";
		outputSszTabFileName = outputFilePrefix + "ssz.tab";
		outputSszTxtFileName = outputFilePrefix + "ssz.txt";
		outputScreenCsvFileName = outputFilePrefix + "screen.csv";
		EnergyPlusIniFileName = "Energy+.ini";
		inStatFileName = "in.stat";
		TarcogIterationsFileName = "TarcogIterations.dbg";
		eplusADSFileName = idfDirPathName+"eplusADS.inp";

		// Handle bad options
		std::vector<std::string> badOptions;
		if(!opt.gotExpected(badOptions)) {
			for(int i=0; i < badOptions.size(); ++i) {
					ShowFatalError("ERROR: Unexpected number of arguments for option " + badOptions[i] + "\n");
			}
			DisplayString(usage);
			exit(EXIT_FAILURE);
		}

		if(opt.firstArgs.size() > 1 || opt.unknownArgs.size() > 0 || opt.lastArgs.size() > 0){
			for(int i=1; i < opt.firstArgs.size(); ++i) {
				std::string arg(opt.firstArgs[i]->c_str());
				ShowFatalError("ERROR: Invalid option first arg: " + arg + "\n");
			}
			for(int i=0; i < opt.unknownArgs.size(); ++i) {
				std::string arg(opt.unknownArgs[i]->c_str());
				ShowFatalError("ERROR: Invalid option unknown arg: " + arg + "\n");
			}
			for(int i=0; i < opt.lastArgs.size(); ++i) {
				std::string arg(opt.lastArgs[i]->c_str());
				ShowFatalError("ERROR: Invalid option last arg: " + arg + "\n");
			}
			DisplayString(usage);
			exit(EXIT_FAILURE);
		}

		if(!opt.gotRequired(badOptions)) {
			for(int i=0; i < badOptions.size(); ++i) {
				ShowFatalError("ERROR: Missing required option " + badOptions[i] + "\n");
			}
			DisplayString(usage);
			exit(EXIT_FAILURE);
		}

		// Process standard arguments
		if (opt.isSet("-h")) {
			DisplayString(usage);
			exit(EXIT_SUCCESS);
		}

		if (opt.isSet("-v")) {
			DisplayString(VerString);
			exit(EXIT_SUCCESS);
		}


		return 0;
	}
  } //namespace options
} //EnergyPlus namespace


