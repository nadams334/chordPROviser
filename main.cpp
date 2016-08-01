
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
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
Options* options;

enum IOtype = { Input, Output };
enum InputFileType = { MMA, TXT };
string inputFilename;
string outputFilename;
InputFileType inputFileType;

// Command line args
string inputFileOption = "-i";
string outputFileOption = "-o";
string logFileOption = "-l";

void log(string message)
{
	if (displayLogs)
	{
		cerr << message << endl;
	}
}

bool EndsWith(const string& a, const string& b) {
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}

vector<string> getLines(string filename)
{
	vector<string> lines = new vector<string>();
	// ifstream inputStream = filename
	
	for (string line; getline(inputStream, line);) {
        lines.insert(line);
    }
    
    return lines;
}

void loadCPSfile(string lines)
{
	
}

void loadMMAfile(string lines)
{
	
}

void loadInput(string filename, InputFileType fileType)
{
	vector<string> lines = getLines(filename);
	
	switch (fileType)
	{
		case InputFileType.TXT:
			loadCPSfile(lines);
			break;
		case InputFileType.MMA:
			loadMMAfile(lines);
			break;
		default:
			log("Unsupported input file type for filename: %s (InputFileType::%s)", filename, fileType);
			break;
	}

}

void setInputFileType(string filename)
{
	if (filename.endsWith(".txt")) inputFileType = InputFileType.TXT;
	else if (filename.endsWith(".mma")) inputFileType = InputFileType.MMA;
	else
	{
		inputFileType = InputFileType.Other;
		log("Unrecognized input file type for input file: %s", filename);
	}
}

bool setIOFile(int argNumber, IOtype ioType)
{
	if (argNumber >= options.getArgCount())
	{
		log("Please supply an %s filename after the '%s' option.", type, optionName);
		return false;
	}
	
	string arg = options.getArg(argNumber);
	string type;
	string optionName;
	
	switch (type)
	{
		case Input:
			setInputFileType(arg);
			type = "input";
			optionName = inputFileOption;
			inputFilename = arg;
			break;
		case Output:
			type = "output";
			optionName = outputFileOption;
			outputFilename = arg;
			break;
		default:
			log("Unrecoginized IO type: %s", ioType);
			return false;
			break;
	}
}

bool setInputFile(int argNumber)
{
	return setIOFile(argNumber, IOtype.Input);
}

bool setOutputFile(int argNumber)
{
	return setIOFile(argNumber, IOtype.Input);
}

void processOption(int argNumber)
{
	string arg = options.getArg(argNumber);
	switch (arg)
	{
		case inputFileOption:
			setInputFile(argNumber+1);
			break;
		case outputFileOption:
			setOutputFile(argNumber+1);
			break;
		case logFileOption:
			displayLogs = false;
			break;
		default:
			log("Unrecoginized command line argument #%d: %s", argNumber, option);
			break;
	}
}

int initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	displayLogs = true;
	
	// Process command line options
	options = new Options();
	options->process(argc, argv);
	
	for (int i = 0; i < options.getArgCount(); i++)
	{
		processOption(i);
	}
	
	loadInput(inputFilename);
	
}

int main(int argc, char** argv) 
{
	initialize(argc, argv);
	
	
	
	return errorStatus;
}
