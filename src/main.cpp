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

#include "Controller.h"
#include "Exception.h"
#include "Model.h"
#include "TextParser.h"

#define TRM_CONTROL_MODEL_CONFIG_FILE "/monet.xml"



void
showUsage()
{
	std::cout << "Usage: gnuspeech_x [-v] -c config_dir [-i input_file.txt] -p trm_param_file -o output_file.wav text" << std::endl;
	std::cout << "         -v : verbose" << std::endl;
}

int
main(int argc, char* argv[])
{
	if (argc < 2) {
		showUsage();
		return 1;
	}

	bool verbose = false;
	const char* configDirPath = nullptr;
	const char* inputFile = nullptr;
	const char* outputFile = nullptr;
	const char* trmParamFile = nullptr;
	std::ostringstream inputTextStream;

	int i = 1;
	while (i < argc) {
		if (strcmp(argv[i], "-v") == 0) {
			++i;
			verbose = true;
		} else if (strcmp(argv[i], "-c") == 0) {
			++i;
			if (i == argc) {
				showUsage();
				return 1;
			}
			configDirPath = argv[i];
			++i;
		} else if (strcmp(argv[i], "-i") == 0) {
			++i;
			if (i == argc) {
				showUsage();
				return 1;
			}
			inputFile = argv[i];
			++i;
		} else if (strcmp(argv[i], "-p") == 0) {
			++i;
			if (i == argc) {
				showUsage();
				return 1;
			}
			trmParamFile = argv[i];
			++i;
		} else if (strcmp(argv[i], "-o") == 0) {
			++i;
			if (i == argc) {
				showUsage();
				return 1;
			}
			outputFile = argv[i];
			++i;
		} else {
			for ( ; i < argc; ++i) {
				inputTextStream << argv[i] << ' ';
			}
		}
	}

	if (configDirPath == nullptr || trmParamFile == nullptr || outputFile == nullptr) {
		showUsage();
		return 1;
	}

	if (inputFile != nullptr) {
		std::ifstream in(inputFile);
		if (!in) {
			std::cerr << "Could not open the file " << inputFile << '.' << std::endl;
			return 1;
		}
		std::string line;
		while (std::getline(in, line)) {
			inputTextStream << line << ' ';
		}
	}
	std::string inputText = inputTextStream.str();
	if (inputText.empty()) {
		std::cerr << "Empty input text." << std::endl;
		return 1;
	}
	if (verbose) {
		std::cout << "inputText=[" << inputText << ']' << std::endl;
	}

	try {
		std::unique_ptr<TRMControlModel::Model> trmControlModel(new TRMControlModel::Model());
		trmControlModel->load(configDirPath, TRM_CONTROL_MODEL_CONFIG_FILE);
		if (verbose) {
			trmControlModel->printInfo();
		}

		std::unique_ptr<TextParser> textParser(new TextParser(configDirPath));

		std::unique_ptr<TRMControlModel::Controller> trmController(new TRMControlModel::Controller(configDirPath, *trmControlModel));

		std::string phoneticString = textParser->parseText(inputText.c_str());
		if (verbose) {
			std::cout << "Phonetic string: [" << phoneticString << ']' << std::endl;
		}

		trmController->synthesizePhoneticString(phoneticString.c_str(), trmParamFile, outputFile);

	} catch (std::exception& e) {
		std::cerr << "Caught an exception: " << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Caught an unknown exception." << std::endl;
		return 1;
	}

	return 0;
}
