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

#include "MidiFile.h"



using namespace std;



int errorStatus;

// Song info
int beatsPerMinute;
int beatsPerBar;
vector<string> chordProgression;
vector<string> scaleProgression;
vector<string> noteProgression;

// File I/O
MidiFile* midiFile;

enum IOtype { Input, Output };
enum InputFileType { MMA, TXT, Other };

string inputFilename;
string outputFilename;

InputFileType inputFileType;

// Config data
const string CHORD_LIST_FILENAME = "config/chords.cfg";
const string COLOR_FILENAME = "config/colors.cfg";

map<string, string> chordScaleNotes;
map<string, string> chordDefaultScales;

int[] color1;
int[] color2;
int[] mixed_color;

// Command line args
vector<string> commandLineArgs;

bool displayLogs;
bool displayScales;
bool indicateRoot;
bool brightMode;
bool debugMode;

const string inputFileOption = "-i";
const string outputFileOption = "-o";
const string logFileOption = "-l";
const string scalesFileOption = "-s";
const string indicateRootOption = "-r";
const string brightnessOption = "-b";
const string debugOption = "-d";

void debug()
{
	if (!debugMode)
		return;

	cout << endl << "Args(" << getArgCount() << "): " << endl;

	for (int i = 0; i < getArgCount(); i++)
	{
		cout << getArg(i) << endl;
	}

	cout << endl << "Options: " << endl;
	cout << "Logs enabled (default 1): " << displayLogs << endl;
	cout << "Bright mode (default 1):" << brightMode << endl;
	cout << "Scale mode (default 0): " << displayScales << endl;
}

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

string getChordType(string chordName)
{
	// Grab everything after the first character or 2
}

string getRoot(string chordName)
{
	// Grab the first character or 2
}

void toggle(bool& booleanValue)
{
	if (booleanValue)
		booleanValue = false;
	else
		booleanValue = true;
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
	// Stuff all chord names in the chord progression vector (append root C if no root specified)
	// For each chord:
	//	Get the associated scale (either specified or looked up in chordDefaultScales if none specified. append root of chord at beginning if not specified)
	//	If displayScales: add scale to chord progression
	//	else add "empty"
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
		toggle(displayLogs);
	}	
	else if (arg.compare(scalesFileOption) == 0)
	{
		toggle(displayScales);
	}	
	else if (arg.compare(debugOption) == 0)
	{
		toggle(debugMode);
	}
	else if (arg.compare(brightnessOption) == 0)
	{
		toggle(brightMode);
	}
	else if (arg.compare(indicateRootOption) == 0)
	{
		toggle(indicateRoot);
	}
	else
	{
		stringstream ss;
		ss << "Unrecoginized command line argument #" << argNumber << ": " << arg;
		log(ss.str());
	}
	
	return false;
	
}

void loadChordMaps(string filename)
{
	
}

void loadColorConfig(string filename)
{
	color1 = { 255, 0, 0 };
	color2 = { 0, 255, 0 };
	mixed_color = { 255, 0, 255 };
}

void loadConfig() 
{
	loadChordMaps(CHORD_LIST_FILENAME);
	loadColorConfig(COLOR_FILENAME);
}

void normalizeBrightness()
{
	if (!brightMode)
		return;
		
	// if all 1's and 0's, change 1's to 2's
}

string combineChords(string chord1, string chord2)
{
	string combinedChord = "";
	
	for (int i = 0; i < chord1.length(); i++)
	{
		combinedChord += chord1[i] + chord2[i] - '0';
	}
	
	normalizeBrightness();
	
	return combinedChord;
}

string shiftStringRight(string str, int offset)
{
	
}

string transposeChord(string chordType, string root)
{
	if (root.compare("C") == 0)
	{
		return shiftStringRight(chord, 0);
	}	
	else if (root.compare("C#") == 0 || root.compare("Db") == 0)
	{
		return shiftStringRight(chord, 1);
	}	
	else if (root.compare("D") == 0)
	{
		return shiftStringRight(chord, 2);
	}	
	else if (root.compare("D#") == 0 || root.compare("Eb") == 0)
	{
		return shiftStringRight(chord, 3);
	}
	else if (root.compare("E") == 0)
	{
		return shiftStringRight(chord, 4);
	}
	else if (root.compare("F") == 0)
	{
		return shiftStringRight(chord, 5);
	}
	else if (root.compare("F#") == 0 || root.compare("Gb") == 0)
	{
		return shiftStringRight(chord, 6);
	}
	else if (root.compare("G") == 0)
	{
		return shiftStringRight(chord, 7);
	}
	else if (root.compare("G#") == 0 || root.compare("Ab") == 0)
	{
		return shiftStringRight(chord, 8);
	}
	else if (root.compare("A") == 0)
	{
		return shiftStringRight(chord, 9);
	}
	else if (root.compare("A#") == 0 || root.compare("Bb") == 0)
	{
		return shiftStringRight(chord, 10);
	}
	else if (root.compare("B") == 0)
	{
		return shiftStringRight(chord, 11);
	}
	else
	{
		stringstream ss;
		ss << "Unrecoginized root not: " << root;
		log(ss.str());
		errorStatus = 3;
	}
}

string generateChord(int index)
{
	return transposeChord(getChordType(chordProgression[i]), getRoot(chordProgression[i]);
}

string generateScale(int index)
{
	return transposeChord(getChordType(scaleProgression[i]), getRoot(scaleProgression[i]);
}

vector<string> generateNoteProgression() 
{
	for (int i = 0; i < chordProgression.size(); i++)
	{
		string notesInChord = generateChord(i);
		string notesInScale = generateScale(i);
		noteProgression.push_back(combineChordAndScale(notesInChord, notesInScale);
	}
}

void initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	displayLogs = true;
	displayScales = false;
	indicateRoot = false;
	brightMode = true;
	debugMode = false;
	
	// Process command line options
	parseArgs(argc, argv);
	
	for (int i = 1; i < getArgCount(); i++)
	{
		if (processOption(i))
			i++;
	}

	loadConfig();
	
	loadInput(inputFilename);
	
}

int main(int argc, char** argv) 
{
	initialize(argc, argv);

	generateNoteProgression();

	return errorStatus;
}
