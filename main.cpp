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

const int numChannels = 5;
const int ODD_CHORD_CHANNEL = 1;
const int EVEN_CHORD_CHANNEL = 2;
const int MIXED_CHORD_CHANNEL = 3;
const int BASS_NOTE_CHANNEL = 4;

vector<string> noteProgressionByChannel[numChannels];
vector<int> chordChanges; // a list of every beat (zero-based) where a chord changes occurs

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

bool loopMode;
bool brightMode;
bool indicateBass;
bool debugMode;

const string inputFileOption = "-i";
const string outputFileOption = "-o";
const string loopModeOption = "-l";
const string brightModeOption = "-b";
const string indicateBassOption = "-r";
const string debugOption = "-d";


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
	string chordType = chordName.substr(indexOfFirstCharOfChordType);
	while (chordType.find("/") != string::npos) chordType = chordType.substr(0, chordType.size() -1);
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
		cerr << ss.str() << endl;
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
	cout << "Loop mode enabled (default 1): " << loopMode << endl;
	cout << "Bright mode (default 0):" << brightMode << endl;
	cout << "Indicate bass (default 1):" << indicateBass << endl;
	
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
	
	for (string line; getline(inputStream, line);) 
	{
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
					
					if (getRoot(chord).size() == 0)
					{
						chord = "C" + chord; // default to root of C if none specified
					}
					
					if (getRoot(scale).size() == 0)
					{
						//cout << "Appending root note '" << getRoot(chord) << "' from chord '" << chord << "' to scale '" << scale << "'" << endl;
						scale = getRoot(chord) + scale; // append root of chord to scale name
					}
					
					// need to append bass note to scale if specified for chord but not for scale
					if (scale.find("/") == string::npos && chord.find("/") != string::npos)
					{
						scale += "/" + getBass(chord);
					}
				}
				else
				{
					chord = word;
					if (getRoot(chord).size() == 0)
					{
						chord = "C" + chord; // default to root of C if none specified
					}
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
			cerr << ss.str() << endl;
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
		cerr << ss.str() << endl;
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
		cerr << ss.str() << endl;
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
			cerr << ss.str() << endl;
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
	else if (arg.compare(loopModeOption) == 0)
	{
		toggle(loopMode);
	}	
	else if (arg.compare(debugOption) == 0)
	{
		toggle(debugMode);
	}
	else if (arg.compare(brightModeOption) == 0)
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
		cerr << ss.str() << endl;
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

int getNoteIndex(string note)
{
	if (note.compare("C") == 0)
	{
		return 0;
	}	
	else if (note.compare("C#") == 0 || note.compare("Db") == 0)
	{
		return 1;
	}	
	else if (note.compare("D") == 0)
	{
		return 2;
	}	
	else if (note.compare("D#") == 0 || note.compare("Eb") == 0)
	{
		return 3;
	}
	else if (note.compare("E") == 0)
	{
		return 4;
	}
	else if (note.compare("F") == 0)
	{
		return 5;
	}
	else if (note.compare("F#") == 0 || note.compare("Gb") == 0)
	{
		return 6;
	}
	else if (note.compare("G") == 0)
	{
		return 7;
	}
	else if (note.compare("G#") == 0 || note.compare("Ab") == 0)
	{
		return 8;
	}
	else if (note.compare("A") == 0)
	{
		return 9;
	}
	else if (note.compare("A#") == 0 || note.compare("Bb") == 0)
	{
		return 10;
	}
	else if (note.compare("B") == 0)
	{
		return 11;
	}
	else
	{
		stringstream ss;
		ss << "Unrecoginized note: '" << note << "'";
		cerr << ss.str() << endl;
		errorStatus = 3;
		return -1;
	}
}

string getChordNoteString(string chordType)
{
	int numZeroes = count(chordType.begin(), chordType.end(), '0');
	int numOnes = count(chordType.begin(), chordType.end(), '1');
	int numTwos = count(chordType.begin(), chordType.end(), '2');
	
	if (chordType.size() == 12 && ((numZeroes + numOnes + numTwos) == 12))
	{
		// chord is already in note-string format
		return chordType;
	}
	
	return chordMap[chordType]; // do lookup translation to get note-string format
}

string transposeChord(string chordType, string root, string bass)
{
	string chordNoteString = getChordNoteString(chordType);
	
	int rootIndex = getNoteIndex(root);
	int bassIndex = getNoteIndex(bass);
	
	if (rootIndex >= 0)
	{
		// shift chord based on specified root
		chordNoteString = shiftStringRight(chordNoteString, rootIndex);
	}
	
	if (bassIndex >= 0)
	{
		// add bass note if not present in chord
		if (chordNoteString[bassIndex] == '0') chordNoteString[bassIndex] = '1';
	}
	
	return chordNoteString;
}

string generateChord(int index)
{
	return transposeChord(getChordType(chordProgression[index]), getRoot(chordProgression[index]), getBass(chordProgression[index]));
}

string generateScale(int index)
{
	if (scaleProgression[index].compare("empty") == 0) return "000000000000";
	return transposeChord(getChordType(scaleProgression[index]), getRoot(scaleProgression[index]), getBass(scaleProgression[index]));
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

void separateNotesOfChordChange(int indexOfFirstChord, int indexOfSecondChord, bool oddToEven)
{
		string firstChord = noteProgression[indexOfFirstChord];
		string secondChord = noteProgression[indexOfSecondChord];
		
		string notesByChannel[numChannels];
		for (int i = 1; i < numChannels; i++)
		{
			notesByChannel[i] = "000000000000";
		}
		
		for (int i = 0; i < firstChord.size(); i++)
		{
			if (firstChord[i] > '0') // note is active
			{
				if (secondChord[i] > '0')
				{
					// chord change shares this note
					notesByChannel[MIXED_CHORD_CHANNEL][i] = firstChord[i];
				}
				else // this note is only in first chord
				{
					if (oddToEven)
					{
						notesByChannel[ODD_CHORD_CHANNEL][i] = firstChord[i];
					}
					else // even to odd
					{
						notesByChannel[EVEN_CHORD_CHANNEL][i] = firstChord[i];
					}
				}
			}
		}
		
		if (indicateBass)
		{
			int indexOfBassNote = getNoteIndex(getBass(chordProgression[indexOfFirstChord]));
			notesByChannel[BASS_NOTE_CHANNEL][indexOfBassNote] = firstChord[indexOfBassNote];
		}
		
		// fill in all bars of the first chord
		for (int beat = indexOfFirstChord; beat != indexOfSecondChord && beat < noteProgression.size(); beat++)
		{
			for (int channel = 1; channel < numChannels; channel++)
			{
				noteProgressionByChannel[channel][beat] = notesByChannel[channel];
			}
		}
}

void separateNoteProgressionByChannel()
{
	// Compare each chord to the chord that comes next
	// Move shared notes to channel 3
	// Move private notes to channel 1/2 (alternating each chord change)
	// eg. CM7_'ionian to Cm7_'aeolian
	// 201021020102 -> [000020000102] [] [201001020000]
	
	// initialize noteProgressionByChannel
	for (int channel = 1; channel < numChannels; channel++)
	{
		for (int beat = 0; beat < noteProgression.size(); beat++)
			noteProgressionByChannel[channel].push_back("000000000000");
	}
	
	bool isOddToEvenChordChange = true; // keep track of odd/even parity for each chord change
	
	for (int indexOfCurrentChord = 0; indexOfCurrentChord < noteProgression.size();)
	{
		// look ahead until we find next chord change (first chord that is different from the current)
		
		int indexOfNextChord;
		for (indexOfNextChord = indexOfCurrentChord + 1; indexOfNextChord < noteProgression.size() && noteProgression[indexOfCurrentChord].compare(noteProgression[indexOfNextChord]) == 0; indexOfNextChord++);
		
		if (indexOfNextChord >= noteProgression.size()) // we're currently completing the last chord
		{
			if (indexOfCurrentChord == 0) // there are no chord changes
			{
				// Put all notes on same channel
				for (int i = 0; i < noteProgression.size(); i++)
				{
					noteProgressionByChannel[ODD_CHORD_CHANNEL][i] = noteProgression[i];
					noteProgressionByChannel[EVEN_CHORD_CHANNEL][i] = "000000000000";
					noteProgressionByChannel[MIXED_CHORD_CHANNEL][i] = "000000000000";
					noteProgressionByChannel[BASS_NOTE_CHANNEL][i] = "000000000000";
					if (indicateBass)
					{
						int indexOfBassNote = getNoteIndex(getBass(chordProgression[i]));
						noteProgressionByChannel[BASS_NOTE_CHANNEL][i][indexOfBassNote] = noteProgression[i][indexOfBassNote];
					}
				}
			}
			else // there are chord changes
			{
				if (loopMode)
				{
					// need to transistion to a chord and use different colors
					
					// if first and last chord same
					if (noteProgression[0].compare(noteProgression[noteProgression.size()-1]) == 0)
					{
						indexOfNextChord = chordChanges[0];
					}
					else // first and last chord different
					{
						// transistion to first chord
						indexOfNextChord = 0;
						chordChanges.push_back(0); // indicate a chord change to first chord
					}
					
					separateNotesOfChordChange(indexOfCurrentChord, indexOfNextChord, isOddToEvenChordChange);
				}
				else
				{
					// use same color
					for (int i = indexOfCurrentChord; i < noteProgression.size(); i++)
					{
						if (isOddToEvenChordChange)
						{
							noteProgressionByChannel[ODD_CHORD_CHANNEL][i] = noteProgression[i];
							noteProgressionByChannel[EVEN_CHORD_CHANNEL][i] = "000000000000";
						}
						else // even to odd
						{
							noteProgressionByChannel[EVEN_CHORD_CHANNEL][i] = noteProgression[i];
							noteProgressionByChannel[ODD_CHORD_CHANNEL][i] = "000000000000";
						}
						noteProgressionByChannel[MIXED_CHORD_CHANNEL][i] = "000000000000";
						noteProgressionByChannel[BASS_NOTE_CHANNEL][i] = "000000000000";
						if (indicateBass)
						{
							int indexOfBassNote = getNoteIndex(getBass(chordProgression[i]));
							noteProgressionByChannel[BASS_NOTE_CHANNEL][i][indexOfBassNote] = noteProgression[i][indexOfBassNote];
						}
					}
				}
			}
			break; // finished separating notes into channels for each chord
		} 
		
		// create transition for non-final chord change
		
		chordChanges.push_back(indexOfNextChord);
		
		separateNotesOfChordChange(indexOfCurrentChord, indexOfNextChord, isOddToEvenChordChange);
		
		toggle(isOddToEvenChordChange);
		indexOfCurrentChord = indexOfNextChord; // point to next chord
	}
}

void createMidiFile()
{
	// Create MIDI file
	// Add tempo midi event
	// Add note ons and note offs for every octave based on chord changes to the appropriate channel
	// Make sure to use blinking lead-in
	// Write MIDI file
}

void initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	brightMode = false;
	indicateBass = true;
	loopMode = true;
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
	
	if (chordProgression.size() == 0)
	{
		cerr << "No chords found in input file. Exiting..." << endl;
		errorStatus = 2;
		exit(errorStatus);
	}	
}

int main(int argc, char** argv) 
{	
	initialize(argc, argv);
	
	if (debugMode)
	{
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
	}

	generateNoteProgression();
	
	if (debugMode)
	{
		cout << "Note Progression: " << endl;
		for (int i = 0; i < noteProgression.size(); i++)
			cout << "[" << i << "]: " << noteProgression[i] << endl;	
		cout << endl;
	}
		
	separateNoteProgressionByChannel();
	
	if (debugMode)
	{
		cout << "Note Progression by Channel: " << endl;
		for (int i = 1; i < numChannels; i++)
		{
			for (int j = 0; j < noteProgressionByChannel[i].size(); j++)
			{
				cout << "[" << i << "][" << j << "]: " << noteProgressionByChannel[i][j] << endl;
			}
		}
		cout << endl;
		
		cout << "Chord changes: " << endl;
		for (int i = 0; i < chordChanges.size(); i++)
			cout << "[" << i << "]: " << "Beat #" << chordChanges[i] << endl;
		cout << endl;
	}
	
	createMidiFile();

	return errorStatus;
}
