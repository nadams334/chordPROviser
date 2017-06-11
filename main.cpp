// chordPROviser
// Author: Nathan Adams
// 2016-2017

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

#include "MidiFile.h"



using namespace std;



int errorStatus;

// Song info
int beatsPerMinute = 0;
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

map<string, string> chordMap;

// Command line args
vector<string> commandLineArgs;

bool displayLogs;
bool brightMode;
bool indicateRoot;
bool debugMode;

const string inputFileOption = "-i";
const string outputFileOption = "-o";
const string logFileOption = "-l";
const string brightnessOption = "-b";
const string indicateRootOption = "-r";
const string debugOption = "-d";


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

string getRoot(string chordName)
{
	if (chordName[0] != 'A' || chordName[0] != 'B' || chordName[0] != 'C' || chordName[0] != 'D' || chordName[0] != 'E' || chordName[0] != 'F' || chordName[0] != 'G')
	{
		cout << "No root specified for chord '" << chordName << "'" << endl;
	}
	if (chordName[1] == '#' || chordName[1] == 'b')
	{
		return chordName.substr(0, 2);
	}
	return chordName.substr(0, 1);
}

string getChordType(string chordName)
{
	int indexOfFirstCharOfChordType = getRoot(chordName).size();
	return chordName.substr(indexOfFirstCharOfChordType, string::npos);
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
	cout << "Bright mode (default 0):" << brightMode << endl;
	cout << "Indicate root (default 0):" << indicateRoot << endl;
	
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
	// Stuff all chord names in the chord progression vector
	// For each chord:
	//	Get the associated scale (append root of chord at beginning if not specified)
	//	else add "empty" to scaleProgression vector
	
	bool foundChordsSection = false;
	
	for (int i = 0; i < lines.size(); i++)
	{
		string line = lines[i];
		
		if (foundChordsSection)
		{
			// parse chords...
			// delimit by whitespace into word array
			// if see '|' then skip to next word
			// if see '_' then split into chord and scale
			// chord name ("CM7") goes into chord progression vector
			// scale name ("C'ionian") goes into scale progression vector (make sure to append root if not present)
			// if no '_' then add "empty" to scaleProgression vector to keep indecies consistent
			// if see '/' or '.' then repeat last chord: chordProgression.push_back(chordProgression.back())
			//                                           scaleProgression.push_back(scaleProgression.back())
		}
		
		else if (endsWith(line, "BPM"))
		{
			stringstream ss(line);
			string temp;
			ss >> temp >> beatsPerMinute;
		}
		
		else if (line.compare("Chords:") == 0)
		{
			foundChordsSection = true;
		}
	}
		
	if (beatsPerMinute == 0)
	{
		cerr << "ERROR: No tempo found in input file: " << inputFilename << endl;
		errorStatus = 2;
		exit(errorStatus);
	}
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
	vector<string> lines = getLines(filename);
	for (int i = 0; i < lines.size(); i++)
	{
		string line = lines[i];
		if (line.size() <= 0)
			return; // finished parsing
			
		string chordType = "";
		string noteString = "";
		
		bool foundTab = false;
		for (int j = 0; j < line.length(); j++) 
		{
			char c = line[j];
			if (c == '\t') 
			{
				foundTab = true;
			}
			else if (foundTab)
			{
				noteString += c;
			}
			else
			{
				chordType += c;
			}
		}
		
		chordMap.insert(pair<string, string>(chordType, noteString));
	}
}

void loadConfig() 
{
	loadChordMaps(CHORD_LIST_FILENAME);
}

string normalizeBrightness(string chord)
{
	bool foundOne = false;
	bool foundTwo = false;
	
	for (int i = 0; i < chord.size(); i++)
	{
		if (chord[i] == '1') foundOne = true;
		else if (chord[i] == '2') foundTwo = true;
	}
	
	bool shouldNormalize = foundOne && !foundTwo;
	
	if (shouldNormalize)
	{
		for (int i = 0; i < chord.size(); i++)
		{
			if (chord[i] == '1') chord[i] = '2';
		}
	}
}

string combineChords(string chord1, string chord2)
{
	string combinedChord = "";
	
	for (int i = 0; i < chord1.length(); i++)
	{
		combinedChord += chord1[i] + chord2[i] - '0';
	}
	
	if (brightMode)
		combinedChord = normalizeBrightness(combinedChord);
	
	return combinedChord;
}

string shiftStringRight(string str, int offset)
{
	rotate(str.begin(), str.begin()+(str.size()-offset), str.end());
}

string transposeChord(string chordType, string root)
{
	if (root.compare("C") == 0)
	{
		return shiftStringRight(chordType, 0);
	}	
	else if (root.compare("C#") == 0 || root.compare("Db") == 0)
	{
		return shiftStringRight(chordType, 1);
	}	
	else if (root.compare("D") == 0)
	{
		return shiftStringRight(chordType, 2);
	}	
	else if (root.compare("D#") == 0 || root.compare("Eb") == 0)
	{
		return shiftStringRight(chordType, 3);
	}
	else if (root.compare("E") == 0)
	{
		return shiftStringRight(chordType, 4);
	}
	else if (root.compare("F") == 0)
	{
		return shiftStringRight(chordType, 5);
	}
	else if (root.compare("F#") == 0 || root.compare("Gb") == 0)
	{
		return shiftStringRight(chordType, 6);
	}
	else if (root.compare("G") == 0)
	{
		return shiftStringRight(chordType, 7);
	}
	else if (root.compare("G#") == 0 || root.compare("Ab") == 0)
	{
		return shiftStringRight(chordType, 8);
	}
	else if (root.compare("A") == 0)
	{
		return shiftStringRight(chordType, 9);
	}
	else if (root.compare("A#") == 0 || root.compare("Bb") == 0)
	{
		return shiftStringRight(chordType, 10);
	}
	else if (root.compare("B") == 0)
	{
		return shiftStringRight(chordType, 11);
	}
	else
	{
		stringstream ss;
		ss << "Unrecoginized root note: " << root;
		log(ss.str());
		errorStatus = 3;
	}
}

string generateChord(int index)
{
	return transposeChord(getChordType(chordProgression[index]), getRoot(chordProgression[index]));
}

string generateScale(int index)
{
	return transposeChord(getChordType(scaleProgression[index]), getRoot(scaleProgression[index]));
}

vector<string> generateNoteProgression() 
{
	for (int i = 0; i < chordProgression.size(); i++)
	{
		string notesInChord = generateChord(i);
		string notesInScale = generateScale(i);
		noteProgression.push_back(combineChords(notesInChord, notesInScale));
	}
}

void initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	brightMode = true;
	indicateRoot = false;
	displayLogs = true;
	debugMode = false;
	
	inputFilename = "";
	outputFilename = "";
	
	// Process command line options
	parseArgs(argc, argv);
	
	for (int i = 1; i < getArgCount(); i++)
	{
		if (processOption(i))
			i++;
	}
	
	if (inputFilename.size() == 0)
	{
		cerr << "Please specify an input file. (-i)" << endl;
		errorStatus = 1;
		exit(errorStatus);
	}
	
	if (outputFilename.size() == 0)
	{
		cerr << "Please specify an output file. (-o)" << endl;
		errorStatus = 1;
		exit(errorStatus);
	}

	loadConfig();
	
	loadInput(inputFilename);
	
}

int main(int argc, char** argv) 
{	
	initialize(argc, argv);
	
	cout << "Chord Types: " << endl;
	for (map<string, string>::const_iterator it = chordMap.begin(); it != chordMap.end(); it++)
	{
		cout << it->first << "   :   " << it->second << endl;
	}
	cout << endl;
	
	cout << "BPM: " << beatsPerMinute << endl;
	cout << endl;
	
	cout << "Chord Progression: " << endl;
	for (int i = 0; i < chordProgression.size(); i++)
		cout << "[" << i << "]: " << chordProgression[i] << endl;
		
	cout << endl;
	
	cout << "Scale Progression: " << endl;
	for (int i = 0; i < scaleProgression.size(); i++)
		cout << "[" << i << "]: " << scaleProgression[i] << endl;
		
	cout << endl;

	vector<string> noteProgression = generateNoteProgression();
	
	cout << "Note Progression: " << endl;
	for (int i = 0; i < noteProgression.size(); i++)
		cout << "[" << i << "]: " << noteProgression[i] << endl;

	return errorStatus;
}
