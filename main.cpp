// chordPROviser
// Author: Nathan Adams
// 2016

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

// MidiFile
#include "MidiFile.h"

using namespace std;

int errorStatus;
bool displayLogs;

// Song info
int beatsPerMinute;
vector<string> chordProgression;

// File I/O
MidiFile* midiFile;

enum IOtype { Input, Output };
enum InputFileType { MMA, TXT, Other };
string inputFilename;
string outputFilename;
InputFileType inputFileType;

// Command line args
vector<string> commandLineArgs;

string inputFileOption = "-i";
string outputFileOption = "-o";
string logFileOption = "-l";

void log(string message)
{
	if (displayLogs)
	{
		cout << message << endl;
	}
}

bool endsWith(const string& a, const string& b) 
{
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}

void parseArgs(int argc, char** argv)
{
	for (int i = 0; i < argc; i++)
	{
		string arg(argv[i]);
		commandLineArgs.push_back(arg);
	}
}

int getArgCount()
{
	return commandLineArgs.size();
}

string getArg(int index)
{
	if (index < 0 || index >= getArgCount())
	{
		stringstream ss;
		ss << "Attempting to access out-of-bounds command line argument #: " << index;
		log(ss.str());
		errorStatus = 2;
		return "";
	}
	
	return commandLineArgs.at(index);
}

string getTypeName(IOtype ioType)
{
	switch (ioType)
	{
		case Input:
			return "input";
		case Output:
			return "output";
		default:
			return "";
	}
}

vector<string> getLines(string filename)
{
	vector<string> lines;
	ifstream inputStream(filename.c_str());
	
	for (string line; getline(inputStream, line);) {
        lines.push_back(line);
    }
    
    return lines;
}

void loadCPSfile(vector<string> lines)
{
	
}

void loadMMAfile(vector<string> lines)
{
	
}

void loadInput(string filename)
{
	vector<string> lines = getLines(filename);
	
	switch (inputFileType)
	{
		case TXT:
			loadCPSfile(lines);
			break;
		case MMA:
			loadMMAfile(lines);
			break;
		default:
			stringstream ss;
			ss << "Unsupported input file type for filename: " << filename << " (InputFileType::" << inputFileType << ")";
			log(ss.str());
			errorStatus = 2;
			break;
	}

	cout << "Input file: " << inputFilename << endl << endl;

	for (int i = 0; i < lines.size(); i++)
	{
		cout << lines[i] << endl;
	}

	cout << endl << "Output file: " << outputFilename << endl;

	cout << endl << "Options(" << getArgCount() << "): " << endl;

	for (int i = 0; i < getArgCount(); i++)
	{
		cout << getArg(i) << endl;
	}

}

void setInputFileType(string filename)
{
	if (endsWith(filename, ".txt")) inputFileType = TXT;
	else if (endsWith(filename, ".mma")) inputFileType = MMA;
	else
	{
		inputFileType = Other;
		stringstream ss;
		ss << "Unrecognized input file type for input file: " << filename;
		log(ss.str());
		errorStatus = 1;
	}
}

bool setIOFile(int argNumber, IOtype ioType)
{
	string arg = getArg(argNumber);
	string optionName;

	if (argNumber >= getArgCount())
	{
		stringstream ss;
		ss << "Please supply an " << getTypeName(ioType) << " filename after the " << optionName << " option.";
		log(ss.str());
		errorStatus = 1;
		return false;
	}
	
	switch (ioType)
	{
		case Input:
			setInputFileType(arg);
			optionName = inputFileOption;
			inputFilename = arg;
			break;
		case Output:
			optionName = outputFileOption;
			outputFilename = arg;
			break;
		default:
			stringstream ss;
			ss << "Unrecoginized IO type: " << ioType;
			log(ss.str());
			errorStatus = -1;
			return false;
			break;
	}

	return true;
}

bool setInputFile(int argNumber)
{
	return setIOFile(argNumber, Input);
}

bool setOutputFile(int argNumber)
{
	return setIOFile(argNumber, Output);
}

bool processOption(int argNumber)
{
	string arg = getArg(argNumber);

	if (arg.compare(inputFileOption) == 0)
	{
		setInputFile(argNumber+1);
		return true;
	}	
	else if (arg.compare(outputFileOption) == 0)
	{
		setOutputFile(argNumber+1);
		return true;
	}
	else if (arg.compare(logFileOption) == 0)
	{
		displayLogs = false;
	}	
	else
	{
		stringstream ss;
		ss << "Unrecoginized command line argument #" << argNumber << ": " << arg;
		log(ss.str());
	}
	
	return false;
}

void initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	displayLogs = true;
	
	// Process command line options
	parseArgs(argc, argv);
	
	for (int i = 1; i < getArgCount(); i++)
	{
		if (processOption(i))
			i++;
	}
	
	loadInput(inputFilename);
	
}

int main(int argc, char** argv) 
{
	initialize(argc, argv);



	return errorStatus;
}
