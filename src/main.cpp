/***************************************************************************
 *  Copyright 2014 Marcelo Y. Matuda                                       *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Controller.h"
#include "Exception.h"
#include "global.h"
#include "Log.h"
#include "Model.h"
#include "Tube.h"
#include "en/phonetic_string_parser/PhoneticStringParser.h"
#include "en/text_parser/TextParser.h"
#include "TRMControlModelConfiguration.h"

void
showUsage(const char* programName)
{
    std::cout << "\nGnuspeechSA " << PROGRAM_VERSION << "\n\n";
    std::cout << "Usage:\n\n";
    std::cout << programName << " --version\n";
    std::cout << "        Shows the program version.\n\n";
    std::cout << programName << " [-v] -c config_dir -p trm_param_file.txt -o output_file.wav \"Hello world.\"\n";
    std::cout << "        Synthesizes text from the command line.\n";
    std::cout << "        -v : verbose\n\n";
    std::cout << programName << " [-v] -c config_dir -i input_text.txt -p trm_param_file_prefix -o output_file_prefix\n";
    std::cout << "        Synthesizes text from a file, output to file with prefix specified.\n";
    std::cout << "        -v : verbose\n\n";
    std::cout << programName << " [-v] -t trm_param_input_file.txt -o output_file.wav\n";
    std::cout << "        Synthesizes audio from input trm parameters.\n";
    std::cout << "        -v : verbose\n" << std::endl;
}

int
main(int argc, char* argv[])
{
    if (argc < 2) {
        showUsage(argv[0]);
        return 1;
    }

    const char* configDirPath = nullptr;
    const char* inputFile = nullptr;
    const char* trmInputFile = nullptr;
    const char* outputFilePrefix = nullptr;
    const char* trmParamFilePrefix = nullptr;
    std::ostringstream inputTextStream;

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            ++i;
            GS::Log::debugEnabled = true;
        } else if (strcmp(argv[i], "-c") == 0) {
            ++i;
            if (i == argc) {
                showUsage(argv[0]);
                return 1;
            }
            configDirPath = argv[i];
            ++i;
        } else if (strcmp(argv[i], "-i") == 0) {
            ++i;
            if (i == argc) {
                showUsage(argv[0]);
                return 1;
            }
            inputFile = argv[i];
            ++i;
        } else if(strcmp(argv[i], "-t") == 0) {
            ++i;
            if (i == argc){
                showUsage(argv[0]);
                return 1;
            }
            trmInputFile = argv[i];
            ++i;
        } else if (strcmp(argv[i], "-p") == 0) {
            ++i;
            if (i == argc) {
                showUsage(argv[0]);
                return 1;
            }
            trmParamFilePrefix = argv[i];
            ++i;
        } else if (strcmp(argv[i], "-o") == 0) {
            ++i;
            if (i == argc) {
                showUsage(argv[0]);
                return 1;
            }
            outputFilePrefix = argv[i];
            ++i;
        } else if (strcmp(argv[i], "--version") == 0) {
            ++i;
            showUsage(argv[0]);
            return 0;
        } else {
            for ( ; i < argc; ++i) {
                inputTextStream << argv[i] << ' ';
            }
        }
    }
    if (trmInputFile != nullptr && outputFilePrefix != nullptr){
        // If given trm parameters, then only this part of code will be run
        // std::fstream trmParamStream(trmInputFile, std::ios_base::in| std::ios_base::binary | std::ios_base::trunc);
        std::ifstream trmParamStream(trmInputFile, std::ios_base::in | std::ios_base::binary);
	    if (!trmParamStream) {
		    THROW_EXCEPTION(GS::IOException, "Could not open the file " << trmInputFile << '.');
	    }

	    GS::TRM::Tube trm;
        const char* outputFile = outputFilePrefix; // to be refactored
	    trm.synthesizeToFile(trmParamStream, outputFile);
        return 0;
    }
    if (configDirPath == nullptr || trmParamFilePrefix == nullptr || outputFilePrefix == nullptr) {
        showUsage(argv[0]);
        return 1;
    }
    
    std::vector<std::string> inputTexts;
    if (inputFile != nullptr) {
        std::ifstream in(inputFile, std::ios_base::in | std::ios_base::binary);
        if (!in) {
            std::cerr << "Could not open the file " << inputFile << '.' << std::endl;
            return 1;
        }
        std::string line;
        while (std::getline(in, line)) {
            //inputTextStream << line;
            inputTexts.push_back(line);
            //inputTextStream.clear(std::istream::eofbit | std::istream::failbit);
        }
    }else{
        inputTexts.push_back(inputTextStream.str());
    }
    
    if (inputTexts.empty()) {
        std::cerr << "Empty input text." << std::endl;
        return 1;
    }
    if (GS::Log::debugEnabled) {
        std::cout << "inputText number = " << inputTexts.size() << std::endl;
    }

    try {
        std::unique_ptr<GS::TRMControlModel::Model> trmControlModel(new GS::TRMControlModel::Model());
        trmControlModel->load(configDirPath, TRM_CONTROL_MODEL_CONFIG_FILE);
        if (GS::Log::debugEnabled) {
            trmControlModel->printInfo();
        }

        std::unique_ptr<GS::TRMControlModel::Controller> trmController(new GS::TRMControlModel::Controller(configDirPath, *trmControlModel));
        const GS::TRMControlModel::Configuration& trmControlConfig = trmController->trmControlModelConfiguration();

        std::unique_ptr<GS::En::TextParser> textParser(new GS::En::TextParser(configDirPath,
                                            trmControlConfig.dictionary1File,
                                            trmControlConfig.dictionary2File,
                                            trmControlConfig.dictionary3File));
        std::unique_ptr<GS::En::PhoneticStringParser> phoneticStringParser(new GS::En::PhoneticStringParser(configDirPath, *trmController));
        
        for(int i = 0; i < inputTexts.size(); ++i){
            std::string inputText = inputTexts[i];
            std::string phoneticString = textParser->parseText(inputText.c_str());
            if (GS::Log::debugEnabled) {
                std::cout << "Phonetic string: [" << phoneticString << ']' << std::endl;
            }
            std::string trmParamFile = trmParamFilePrefix + std::to_string(i + 1) + ".txt";
            std::string outputFile = outputFilePrefix + std::to_string(i + 1) + ".wav";
            std::string postureFile = trmParamFilePrefix + std::to_string(i + 1) + "_p.txt";
            trmController->synthesizePhoneticString(*phoneticStringParser, phoneticString.c_str(), trmParamFile.c_str(), postureFile.c_str(), outputFile.c_str());
        }
    } catch (std::exception& e) {
        std::cerr << "Caught an exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Caught an unknown exception." << std::endl;
        return 1;
    }

    return 0;
}
