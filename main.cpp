
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>

// MidiFile
#include "MidiFile.h"
#include "Options.h"

using namespace std;

int errorStatus;
bool displayLogs;

// Song info
int beatsPerMinute;
vector<string> chordProgression;

// File I/O
MidiFile* midiFile;
Options* options;

enum IOtype { Input, Output };
enum InputFileType { MMA, TXT, Other };
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
		cout << message << endl;
	}
}

bool endsWith(const string& a, const string& b) {
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}

vector<string> getLines(string filename)
{
	vector<string> lines = {};
	ifstream inputStream(filename);
	
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
			log("Unsupported input file type for filename: " + filename + " (InputFileType::" + to_string(inputFileType) + ")");
			errorStatus = 2;
			break;
	}

	cout << "Input file: " << inputFilename << endl << endl;

	for (int i = 0; i < lines.size(); i++)
	{
		cout << lines[i] << endl;
	}

	cout << endl << "Output file: " << outputFilename << endl;

	cout << endl << "Options(" + to_string(options->getArgCount()) + "): " << endl;

	for (int i = 0; i < options->getArgCount(); i++)
	{
		cout << options->getArg(i) << endl;
	}

}

void setInputFileType(string filename)
{
	if (endsWith(filename, ".txt")) inputFileType = TXT;
	else if (endsWith(filename, ".mma")) inputFileType = MMA;
	else
	{
		inputFileType = Other;
		log("Unrecognized input file type for input file: " + filename);
		errorStatus = 1;
	}
}

bool setIOFile(int argNumber, IOtype ioType)
{
	string arg = options->getArg(argNumber);
	string type;
	string optionName;

	if (argNumber >= options->getArgCount())
	{
		log("Please supply an " + type + " filename after the " + optionName + " option.");
		errorStatus = 1;
		return false;
	}
	
	switch (ioType)
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
			log("Unrecoginized IO type: " + ioType);
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
	return setIOFile(argNumber, Input);
}

void processOption(int argNumber)
{
	string arg = options->getArg(argNumber);

	if (arg.compare(inputFileOption) == 0)
		setInputFile(argNumber+1);
	else if (arg.compare(outputFileOption) == 0)
			setOutputFile(argNumber+1);
	else if (arg.compare(logFileOption) == 0)
			displayLogs = false;
	else
	{
		log("Unrecoginized command line argument #" + to_string(argNumber) + ": " + arg);
		errorStatus = 1;
	}
}

void initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	displayLogs = true;
	
	// Process command line options
	options = new Options();
	options->process(argc, argv);
	
	for (int i = 0; i < options->getArgCount(); i++)
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
