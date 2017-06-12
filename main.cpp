// chordPROviser
// Author: Nathan Adams
// 2016-2017

#include <string>
#include <stdio.h>
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

const int TPQ = 48;

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
bool indicateBass;
bool debugMode;

const string inputFileOption = "-i";
const string outputFileOption = "-o";
const string logFileOption = "-l";
const string brightnessOption = "-b";
const string indicateBassOption = "-r";
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

int getMicrosecondsPerBeat(int beatsPerMinute)
{
	return 60000000/beatsPerMinute; // 60,000,000 microseconds per minute / x beats per minute = 60,000,000/x microseconds per beat
}

string getRoot(string chordName)
{
	string root;
	
	if (chordName[0] != 'A' && chordName[0] != 'B' && chordName[0] != 'C' && chordName[0] != 'D' && chordName[0] != 'E' && chordName[0] != 'F' && chordName[0] != 'G')
	{
		root = "";
	}
	else if (chordName[1] == '#' || chordName[1] == 'b')
	{
		root = chordName.substr(0, 2); // grab first two characters to get accidental
	}
	else
	{
		root = chordName.substr(0, 1); // just grab first character, no accidental
	}
	//cout << "The root of '" << chordName << "' is '" << root << "'" << endl;
	return root;
}

string getBass(string chordName)
{
	string bass;
	
	int indexOfSlash = chordName.find("/");
	
	if (indexOfSlash != string::npos)
	{
		bass = chordName.substr(indexOfSlash+1);
	}
	else
	{
		bass = getRoot(chordName);
	}
	//cout << "The bass of '" << chordName << "' is '" << bass << "'" << endl;
	return bass;
}

string getChordType(string chordName)
{
	int indexOfFirstCharOfChordType = getRoot(chordName).size();
	string chordType = chordName.substr(indexOfFirstCharOfChordType, string::npos);
	if (chordType.size() == 0) chordType = "M"; // blank chord type implies major
	return chordType;
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
	cout << "Indicate bass (default 0):" << indicateBass << endl;
	
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

vector<string> split(string str, char delim)
{
	vector<string> words;
	string currentWord = "";
	
	for (int i = 0; i < str.size(); i++)
	{
		char c = str[i];
		
		if (c == delim)
		{
			if (currentWord.size() == 0)
			{
				// do nothing
			}
			else // finished building current word
			{
				words.push_back(currentWord);
				currentWord = "";
			}
		}
		
		else
		{
			currentWord += c;
		}
	}
	
	return words;
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
			vector<string> words = split(line, ' ');
			
			for (int i = 0; i < words.size(); i++)
			{
				string word = words[i];
				
				string chord = "";
				string scale = "empty";
				
				if (word.compare("|") == 0)
				{
					// skip
					continue;
				}
				else if (word.compare(".") == 0 || word.compare("/") == 0)
				{
					// repeat
					chord = chordProgression.back();
					scale = scaleProgression.back();
				}
				else if (word.find("_") != string::npos)
				{
					// everything before '_' is chord, everything after '_' is scale
					int indexOfUnderscore = word.find("_");
					
					chord = word.substr(0, indexOfUnderscore);
					scale = word.substr(indexOfUnderscore+1);
					
					if (getRoot(scale).size() == 0)
					{
						//cout << "Appending root note '" << getRoot(chord) << "' from chord '" << chord << "' to scale '" << scale << "'" << endl;
						scale = getRoot(chord) + scale; // append root of chord to scale name
					}
				}
				else
				{
					chord = word;
				}
				
				chordProgression.push_back(chord);
				scaleProgression.push_back(scale);
				
			}
			
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
	else if (arg.compare(indicateBassOption) == 0)
	{
		toggle(indicateBass);
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
	string normalizedChord = "";
	
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
			if (chord[i] == '1') normalizedChord += '2';
			else normalizedChord += '0';
		}
		
		return normalizedChord;
	}
	
	else
	{
		return chord;
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

string copyString(string str)
{
	string copiedString = "";
	
	for (int i = 0; i < str.size(); i++)
	{
		copiedString += str[i];
	}
	
	return copiedString;
}

string shiftStringRight(string str, int offset)
{
	string rotatedStr = copyString(str);
	
	rotate(rotatedStr.rbegin(), rotatedStr.rbegin()+offset, rotatedStr.rend());
	//cout << "Original string: '" << str << "' Rotated string: '" << rotatedStr << "' with offset of " << offset << endl;
	return rotatedStr;
}

string transposeChord(string chordType, string root)
{
	string chordNoteString = chordMap[chordType];
	
	if (root.compare("C") == 0)
	{
		return shiftStringRight(chordNoteString, 0);
	}	
	else if (root.compare("C#") == 0 || root.compare("Db") == 0)
	{
		return shiftStringRight(chordNoteString, 1);
	}	
	else if (root.compare("D") == 0)
	{
		return shiftStringRight(chordNoteString, 2);
	}	
	else if (root.compare("D#") == 0 || root.compare("Eb") == 0)
	{
		return shiftStringRight(chordNoteString, 3);
	}
	else if (root.compare("E") == 0)
	{
		return shiftStringRight(chordNoteString, 4);
	}
	else if (root.compare("F") == 0)
	{
		return shiftStringRight(chordNoteString, 5);
	}
	else if (root.compare("F#") == 0 || root.compare("Gb") == 0)
	{
		return shiftStringRight(chordNoteString, 6);
	}
	else if (root.compare("G") == 0)
	{
		return shiftStringRight(chordNoteString, 7);
	}
	else if (root.compare("G#") == 0 || root.compare("Ab") == 0)
	{
		return shiftStringRight(chordNoteString, 8);
	}
	else if (root.compare("A") == 0)
	{
		return shiftStringRight(chordNoteString, 9);
	}
	else if (root.compare("A#") == 0 || root.compare("Bb") == 0)
	{
		return shiftStringRight(chordNoteString, 10);
	}
	else if (root.compare("B") == 0)
	{
		return shiftStringRight(chordNoteString, 11);
	}
	else
	{
		stringstream ss;
		ss << "Unrecoginized root note: '" << root << "' for chord type '" << chordType << "'";
		log(ss.str());
		errorStatus = 3;
		return "000000000000";
	}
}

string generateChord(int index)
{
	return transposeChord(getChordType(chordProgression[index]), getRoot(chordProgression[index]));
}

string generateScale(int index)
{
	if (scaleProgression[index].compare("empty") == 0) return "000000000000";
	return transposeChord(getChordType(scaleProgression[index]), getRoot(scaleProgression[index]));
}

void generateNoteProgression() 
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
	brightMode = false;
	indicateBass = false;
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

	generateNoteProgression();
	
	cout << "Note Progression: " << endl;
	for (int i = 0; i < noteProgression.size(); i++)
		cout << "[" << i << "]: " << noteProgression[i] << endl;

	return errorStatus;
}
