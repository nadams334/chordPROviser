// chordPROviser
// Author: Nathan Adams
// 2016-2018

#include <string>
#include <stdio.h>
#include <stdint.h>
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
bool unrecognizedChordTypes;

// Song info
int beatsPerMinute = 0;
int numBeats = 0;

vector<string> chordProgression;
vector<string> scaleProgression;
vector<string> noteProgression;

const int UPDATE_CHANNEL_MESSAGE_CODE = 30;
const int UPDATE_ALL_MESSAGE_CODE = 31;
const int STARTING_OCTAVE = 3;
const int ENDING_OCTAVE = 8;
const int NUM_CHANNELS = 4;
const int ODD_CHORD_CHANNEL = 1 - 1;
const int EVEN_CHORD_CHANNEL = 2 - 1;
const int MIXED_CHORD_CHANNEL = 3 - 1;
const int BASS_NOTE_CHANNEL = 4 - 1;
const int STARTING_CHANNEL = 11 - 1;

vector<string> noteProgressionByChannel[NUM_CHANNELS];
vector<int> chordChanges; // a list of every beat (zero-based) where a chord changes occurs

const int TICKS_PER_QUARTER_NOTE = 384;

// File I/O
MidiFile midiOutputFile;

enum IOtype { Input, Output };
enum InputFileType { MMA, TXT };

string chordScaleMappingFilename;
string inputFilename;
string outputFilename;

InputFileType inputFileType;

const string BINASC_DIRECTORY = "binasc/";

// Config data
const string DEFAULT_CHORD_SCALE_MAPPING_FILENAME = "config/chord-scale.cfg";

const string CHORD_LIST_FILENAME = "config/chords.cfg";
const string SCALE_LIST_FILENAME = "config/scales.cfg";

map<string, vector<string>> chordScaleMap; // M7 : [ 'ionian , 'lydian , 'mixolydian , ... ]

map<string, string> chordMap;
map<string, string> scaleMap;

map<string, string> reverseChordMap;
map<string, string> reverseScaleMap;

const int dimNoteVelocity = 8;
const int brightNoteVelocity = 80;

// Command line args
vector<string> commandLineArgs;

bool loopMode;
bool brightMode;
bool indicateBass;
bool debugMode;
bool ignoreScales;

bool realtime;

const string REALTIME_OPTION = "-t";

const string INPUT_FILE_OPTION = "-i";
const string OUTPUT_FILE_OPTION = "-o";
const string LOOP_MODE_OPTION = "-l";
const string BRIGHT_MODE_OPTION = "-b";
const string INDICATE_BASS_OPTION = "-r";
const string DEBUG_OPTION = "-d";
const string CHORDS_ONLY_OPTION = "-c";

const string EMPTY_NOTE_STRING = "000000000000";


string copyString(string str)
{
	string copiedString = "";
	
	for (int i = 0; i < str.size(); i++)
	{
		copiedString += str[i];
	}
	
	return copiedString;
}

string boolToText(bool b)
{
	if (b) return "Enabled";
	else return "Disabled";
}

bool endsWith(const string& a, const string& b) 
{
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}

uint32_t getMicrosecondsPerBeat(int beatsPerMinute)
{
	// 60,000,000 microseconds per minute / x beats per minute = 60,000,000/x microseconds per beat
	unsigned long microsecondsPerMinute = 60000000;
	uint32_t microsecondsPerBeat = microsecondsPerMinute/beatsPerMinute;
	
	if (debugMode)
	{
		cout << "Converted " << beatsPerMinute << " BPM to " << microsecondsPerBeat << " microseconds per quarter note." << endl;
		cout << endl;
	}
	
	return microsecondsPerBeat;
}

bool isValidNoteString(string str)
{
	int numZeroes = count(str.begin(), str.end(), '0');
	int numOnes = count(str.begin(), str.end(), '1');
	int numTwos = count(str.begin(), str.end(), '2');
	
	if (str.size() == 12 && ((numZeroes + numOnes + numTwos) == 12)) return true;
	else return false;
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
		bass = getRoot(chordName.substr(indexOfSlash+1));
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
		cerr << "Attempting to access out-of-bounds command line argument #: " << index << endl;
		cerr << "Exiting..." << endl;
		errorStatus = 2;
		exit(errorStatus);
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
	
	for (string line; getline(inputStream, line);) 
	{
		line.erase(remove(line.begin(), line.end(), '\r'), line.end()); // get rid of windows carriage returns
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

string getChordScaleMappingString()
{
	string chordScaleMappingString = "";

	map<string,vector<string>>::iterator it;
	for (it = chordScaleMap.begin(); it != chordScaleMap.end(); it++)
	{
		string chord = it->first;
		vector<string> scales = it->second;

		string scalesString = "[ ";
		for (int i = 0; i < scales.size() - 1; i++)
		{
			scalesString += scales[i];
			scalesString += " , ";
		}
		scalesString += scales[scales.size()-1] + " ]";

		chordScaleMappingString += chord + " : " + scalesString + "\n";
	}

	return chordScaleMappingString;
}

void writeChordScaleMapping(string filename)
{
	ofstream chordScaleMappingFile;
	chordScaleMappingFile.open(filename);

	chordScaleMappingFile << getChordScaleMappingString();

	chordScaleMappingFile.close();
}

void generateChordScaleMapping(string filename)
{
	// do stuff to populate chordScaleMap

	writeChordScaleMapping(filename);
}

void loadChordScaleMapping(string filename)
{
	// Read file to chordScaleMap

	vector<string> lines = getLines(filename);
	
	for (int i = 0; i < lines.size(); i++)
	{
		string line = lines[i];
		
		string chord;
		vector<string> scales;

		vector<string> words = split(line, ' ');

		chord = words[0];

		for (int i = 0; i < words.size(); i++)
		{
			string word = words[i];

			if (word.compare(":") == 0) continue;
			else if (word.compare("[") == 0) continue;
			else if (word.compare("]") == 0) continue;
			else if (word.compare(",") == 0) continue;

			else scales.push_back(word);
		}

		chordScaleMap.insert(pair<string, vector<string>>(chord, scales));
	}
}

void loadCPSfile(string filename)
{
	// Stuff all chord names in the chord progression vector
	// For each chord:
	//	Get the associated scale (append root of chord at beginning if not specified)
	//	else add "empty" to scaleProgression vector

	vector<string> lines = getLines(filename);
	
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
		cerr << "Exiting..." << endl;
		errorStatus = 2;
		exit(errorStatus);
	}
}

void loadMMAfile(string filename)
{
	cerr << "ERROR: MMA files not yet supported." << endl;
	cerr << "Exiting..." << endl;
	errorStatus = 1;
	exit(errorStatus);
}

void loadInput(string filename)
{	
	switch (inputFileType)
	{
		case TXT:
			loadCPSfile(filename);
			break;
		case MMA:
			loadMMAfile(filename);
			break;
	}

}

void setInputFileType(string filename)
{
	if (endsWith(filename, ".txt")) inputFileType = TXT;
	else if (endsWith(filename, ".mma")) inputFileType = MMA;
	else
	{
		stringstream ss;
		ss << "ERROR: Unrecognized input file type for input file: " << filename;
		cerr << ss.str() << endl;
		errorStatus = 1;
		exit(errorStatus);
	}
}

bool setIOFile(int argNumber, IOtype ioType)
{
	string arg = getArg(argNumber);
	
	switch (ioType)
	{
		case Input:
			setInputFileType(arg);
			inputFilename = arg;
			break;
		case Output:
			outputFilename = arg;
			break;
	}
}

void setChordScaleMappingFile(int argNumber)
{
	chordScaleMappingFilename = getArg(argNumber);
}

void setInputFile(int argNumber)
{
	setIOFile(argNumber, Input);
}

void setOutputFile(int argNumber)
{
	setIOFile(argNumber, Output);
}

bool processOption(int argNumber)
{
	string arg = getArg(argNumber);

	if (arg.compare(REALTIME_OPTION) == 0)
	{
		realtime = true;
		if (argNumber+1 < getArgCount())
		{
			setChordScaleMappingFile(argNumber+1);
			return true;
		}
	}	
	else if (arg.compare(INPUT_FILE_OPTION) == 0)
	{
		setInputFile(argNumber+1);
		return true;
	}	
	else if (arg.compare(OUTPUT_FILE_OPTION) == 0)
	{
		setOutputFile(argNumber+1);
		return true;
	}
	else if (arg.compare(LOOP_MODE_OPTION) == 0)
	{
		toggle(loopMode);
	}	
	else if (arg.compare(DEBUG_OPTION) == 0)
	{
		toggle(debugMode);
	}
	else if (arg.compare(BRIGHT_MODE_OPTION) == 0)
	{
		toggle(brightMode);
	}
	else if (arg.compare(INDICATE_BASS_OPTION) == 0)
	{
		toggle(indicateBass);
	}
	else if (arg.compare(CHORDS_ONLY_OPTION) == 0)
	{
		toggle(ignoreScales);
	}
	else
	{
		stringstream ss;
		ss << "ERROR: Unrecoginized command line argument #" << argNumber << ": " << arg;
		cerr << ss.str() << endl;
		errorStatus = 1;
		exit(errorStatus);
	}
	
	return false;
	
}

void loadChordMaps(string filename, map<string, string>* forwardMap, map<string, string>* reverseMap)
{
	string mostRecentNoteString = "";
	vector<string> lines = getLines(filename);
	for (int i = 0; i < lines.size(); i++)
	{
		string line = lines[i];
		if (line.size() <= 0)
			continue;
			
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
		
		if (noteString.compare(".") == 0)
			noteString = copyString(mostRecentNoteString);
		else if (isValidNoteString(noteString))
			mostRecentNoteString = copyString(noteString);
		else
		{
			cerr << "ERROR: In file '" << filename << "'" << endl;
			cerr << "Note String " << noteString << " is not valid." << endl;
			cerr << "Exiting..." << endl;
			errorStatus = 3;
			exit(errorStatus);
		}
		
		forwardMap->insert(pair<string, string>(chordType, noteString));
		reverseMap->insert(pair<string, string>(noteString, chordType));
	}
}

void loadConfig() 
{
	loadChordMaps(CHORD_LIST_FILENAME, &chordMap, &reverseChordMap);
	loadChordMaps(SCALE_LIST_FILENAME, &scaleMap, &reverseScaleMap);
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
		ss << "WARNING: Unrecoginized note: '" << note << "'";
		cerr << ss.str() << endl;
		errorStatus = 3;
		return -1;
	}
}

string getChordNoteString(string chordType)
{
	if (isValidNoteString(chordType)) return chordType;

	string noteString = chordMap[chordType];

	if (noteString.size() == 0)
		noteString = scaleMap[chordType];

	if (noteString.size() == 0)
	{
		cerr << "Unrecognized chord type: " << chordType << endl;
		unrecognizedChordTypes = true;

		noteString = EMPTY_NOTE_STRING;
	}

	return noteString;
}

string transposeChord(string chordType, string root, string bass)
{
	string chordNoteString = getChordNoteString(chordType);
	
	if (chordNoteString.compare(EMPTY_NOTE_STRING) == 0) 
		return chordNoteString;
	
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
	return transposeChord(getChordType(scaleProgression[index]), getRoot(scaleProgression[index]), getBass(scaleProgression[index]));
}

void generateNoteProgression() 
{
	for (int i = 0; i < chordProgression.size(); i++)
	{
		string notesInChord = generateChord(i);
		string notesInScale = generateScale(i);
		
		if (ignoreScales)
			notesInScale = EMPTY_NOTE_STRING;

		noteProgression.push_back(combineChords(notesInChord, notesInScale));
	}

	if (unrecognizedChordTypes) 
	{
		cerr << endl << "ERROR: MIDI file could not be generated. Please update the chord list to include the missing chord types for this song." << endl;
		exit(1);
	}
}

void separateNotesOfChordChange(int indexOfFirstChord, int indexOfSecondChord, bool oddToEven)
{
		string firstChord = noteProgression[indexOfFirstChord];
		string secondChord = noteProgression[indexOfSecondChord];
		
		string notesByChannel[NUM_CHANNELS];
		for (int i = 0; i < NUM_CHANNELS; i++)
		{
			notesByChannel[i] = EMPTY_NOTE_STRING;
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
			notesByChannel[ODD_CHORD_CHANNEL][indexOfBassNote] = '0';
			notesByChannel[EVEN_CHORD_CHANNEL][indexOfBassNote] = '0';
			notesByChannel[MIXED_CHORD_CHANNEL][indexOfBassNote] = '0';
		}
		
		// fill in all bars of the first chord
		for (int beat = indexOfFirstChord; beat != indexOfSecondChord && beat < noteProgression.size(); beat++)
		{
			for (int channel = 0; channel < NUM_CHANNELS; channel++)
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
	for (int channel = 0; channel < NUM_CHANNELS; channel++)
	{
		for (int beat = 0; beat < noteProgression.size(); beat++)
			noteProgressionByChannel[channel].push_back(EMPTY_NOTE_STRING);
	}
	
	bool isOddToEvenChordChange = true; // keep track of odd/even parity for each chord change
	
	for (int indexOfCurrentChord = 0; indexOfCurrentChord < noteProgression.size();)
	{
		// look ahead until we find next chord change (first chord that is different from the current)
		
		int indexOfNextChord;
		for (indexOfNextChord = indexOfCurrentChord + 1; indexOfNextChord < noteProgression.size() && noteProgression[indexOfCurrentChord].compare(noteProgression[indexOfNextChord]) == 0 && (!indicateBass || getBass(chordProgression[indexOfCurrentChord]).compare(getBass(chordProgression[indexOfNextChord])) == 0); indexOfNextChord++);
		
		if (indexOfNextChord >= noteProgression.size()) // we're currently completing the last chord
		{
			if (indexOfCurrentChord == 0) // there are no chord changes
			{
				// Put all notes on same channel
				for (int i = 0; i < noteProgression.size(); i++)
				{
					noteProgressionByChannel[ODD_CHORD_CHANNEL][i] = noteProgression[i];
					noteProgressionByChannel[EVEN_CHORD_CHANNEL][i] = EMPTY_NOTE_STRING;
					noteProgressionByChannel[MIXED_CHORD_CHANNEL][i] = EMPTY_NOTE_STRING;
					noteProgressionByChannel[BASS_NOTE_CHANNEL][i] = EMPTY_NOTE_STRING;
					if (indicateBass)
					{
						int indexOfBassNote = getNoteIndex(getBass(chordProgression[i]));
						noteProgressionByChannel[BASS_NOTE_CHANNEL][i][indexOfBassNote] = noteProgression[i][indexOfBassNote];
						noteProgressionByChannel[ODD_CHORD_CHANNEL][i][indexOfBassNote] = '0';
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
					break;
				}
				else
				{
					// use same color
					for (int i = indexOfCurrentChord; i < noteProgression.size(); i++)
					{
						if (isOddToEvenChordChange)
						{
							noteProgressionByChannel[ODD_CHORD_CHANNEL][i] = noteProgression[i];
							noteProgressionByChannel[EVEN_CHORD_CHANNEL][i] = EMPTY_NOTE_STRING;
						}
						else // even to odd
						{
							noteProgressionByChannel[EVEN_CHORD_CHANNEL][i] = noteProgression[i];
							noteProgressionByChannel[ODD_CHORD_CHANNEL][i] = EMPTY_NOTE_STRING;
						}
						noteProgressionByChannel[MIXED_CHORD_CHANNEL][i] = EMPTY_NOTE_STRING;
						noteProgressionByChannel[BASS_NOTE_CHANNEL][i] = EMPTY_NOTE_STRING;
						if (indicateBass)
						{
							int indexOfBassNote = getNoteIndex(getBass(chordProgression[i]));
							noteProgressionByChannel[BASS_NOTE_CHANNEL][i][indexOfBassNote] = noteProgression[i][indexOfBassNote];
							noteProgressionByChannel[ODD_CHORD_CHANNEL][i][indexOfBassNote] = '0';
							noteProgressionByChannel[EVEN_CHORD_CHANNEL][i][indexOfBassNote] = '0';
							noteProgressionByChannel[MIXED_CHORD_CHANNEL][i][indexOfBassNote] = '0';
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

// adds a MIDI message issusing the set_tempo command to the specified BPM
void setTempo(int bpm)
{
	/*
	unsigned char statusByte = 0xFF; // meta message
	unsigned char metaByte = 0x51; // set tempo message
	unsigned char lengthByte = 0x04; // tempo stored in 4 bytes
	
	uint32_t microsecondsPerQuarterNote = getMicrosecondsPerBeat(bpm);
	
	// construct message byte array
	vector<unsigned char> setTempoMessage;
	setTempoMessage.push_back(statusByte);
	setTempoMessage.push_back(metaByte);
	setTempoMessage.push_back(lengthByte);
	for (int i = int(lengthByte) - 1; i >= 0; i--)
	{
		unsigned char tempoByte = *((unsigned char*)&microsecondsPerQuarterNote+i);
		setTempoMessage.push_back(tempoByte);
	}
	
	if (debugMode)
	{
		cout << "Constructing SET_TEMPO message with BPM=" << bpm << ": " << endl;
		for (int i = 0; i < setTempoMessage.size(); i++)
		{
			printf("[%d]: 0x%X\n", i, setTempoMessage[i]);
		}
		cout << endl;
	}
	
	for (int channel = 0; channel < NUM_CHANNELS; channel++)
	{
		midiOutputFile.addEvent(channel, 0, setTempoMessage); // add tempo change at start of each channel
	}
	*/
	
	for (int channel = 0; channel < NUM_CHANNELS; channel++)
	{
		midiOutputFile.addTempo(channel, 0, bpm);
	}
	
}

// Adds note on or off message to output file for the specified note on all octaves on the specified channel with the specified time
void addNoteMessage(int channel, int noteIndex, int noteBrightness, int ticks)
{
	unsigned char statusByte = 0x90; // note on message
	if (noteBrightness == 0) // note off message
		statusByte = 0x80;
	statusByte += channel + STARTING_CHANNEL;
	
	unsigned char velocityByte = 0x00;
	if (noteBrightness == 1)
		velocityByte = dimNoteVelocity;
	else if (noteBrightness == 2)
		velocityByte = brightNoteVelocity;
		
	unsigned char pitchByte;
	for (int octave = STARTING_OCTAVE; octave < ENDING_OCTAVE || octave == ENDING_OCTAVE && noteIndex == 0; octave++)
	{
		pitchByte = (12*octave)+noteIndex;
		
		vector<unsigned char> noteMessage;
		noteMessage.push_back(statusByte);
		noteMessage.push_back(pitchByte);
		noteMessage.push_back(velocityByte);
		
		midiOutputFile.addEvent(channel, ticks, noteMessage);
		
		if (debugMode)
		{
			cout << "Created the following note message with the following parameters:" << endl;
			cout << "Channel: " << channel << " | Note Index: " << noteIndex << " | Note Brightness: " << noteBrightness << " | Ticks: " << ticks << endl;
			cout << "Message: ";
			for (int i = 0; i < noteMessage.size(); i++)
			{
				printf("0x%X ", noteMessage[i]);
			}
			cout << endl;
			cout << endl;
		}
	}
}

void clearAllNotesForChannel(int channel, int ticks)
{
	for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
	{
		addNoteMessage(channel, noteIndex, 0, ticks);
	}
}

void clearAllNotes(int ticks)
{
	for (int channel = 0; channel < NUM_CHANNELS; channel++)
	{
		clearAllNotesForChannel(channel, ticks);
	}
}

void addEndOfTrackMessage(int channel, int ticks)
{
	clearAllNotesForChannel(channel, ticks);		
	/*
	unsigned char statusByte = 0xFF; // meta message
	unsigned char metaByte = 0x2F; // end track message
	unsigned char lengthByte = 0x00; // no data bytes for this message

	vector<unsigned char> endOfTrackMessage;
	endOfTrackMessage.push_back(statusByte);
	endOfTrackMessage.push_back(metaByte);
	endOfTrackMessage.push_back(lengthByte);
		
	midiOutputFile.addEvent(channel, ticks, endOfTrackMessage);
		
		if (debugMode)
	{
		cout << "Created the following end track message with the following parameters:" << endl;
		cout << "Channel: " << channel << " | Ticks: " << ticks << endl;
		cout << "Message: ";
		for (int i = 0; i < endOfTrackMessage.size(); i++)
		{
			printf("0x%X ", endOfTrackMessage[i]);
		}
		cout << endl;
		cout << endl;
	}
	*/
}

void endAllTracks(int ticks)
{
	for (int channel = 0; channel < NUM_CHANNELS; channel++)
	{
		if (!indicateBass && channel == BASS_NOTE_CHANNEL) continue;
		addEndOfTrackMessage(channel, ticks);
	}
}

void addUpdateMessage(int ticks)
{
	int channel = EVEN_CHORD_CHANNEL;
	
	if (indicateBass)
		channel = BASS_NOTE_CHANNEL;
	
	unsigned char statusByte = 0xB0 + channel + STARTING_CHANNEL; // control change message
	unsigned char dataByte = UPDATE_ALL_MESSAGE_CODE;
	unsigned char valueByte = 0X7F; // max on signal
	
	vector<unsigned char> updateMessage;
	updateMessage.push_back(statusByte);
	updateMessage.push_back(dataByte);
	updateMessage.push_back(valueByte);
		
	midiOutputFile.addEvent(channel, ticks, updateMessage);
		
	if (debugMode)
	{
		cout << "Created the following update message with the following parameters:" << endl;
		cout << "Ticks: " << ticks << endl;
		cout << "Message: ";
		for (int i = 0; i < updateMessage.size(); i++)
		{
			printf("0x%X ", updateMessage[i]);
		}
		cout << endl;
		cout << endl;
	}
}

void addUpdateMessage(int ticks, int channel)
{
	unsigned char statusByte = 0xB0 + channel + STARTING_CHANNEL; // control change message
	unsigned char dataByte = UPDATE_CHANNEL_MESSAGE_CODE;
	unsigned char valueByte = 0X7F; // max on signal
	
	vector<unsigned char> updateMessage;
	updateMessage.push_back(statusByte);
	updateMessage.push_back(dataByte);
	updateMessage.push_back(valueByte);
		
	midiOutputFile.addEvent(channel, ticks, updateMessage);
		
	if (debugMode)
	{
		cout << "Created the following update message with the following parameters:" << endl;
		cout << "Ticks: " << ticks << endl;
		cout << "Channel: " << channel << endl;
		cout << "Message: ";
		for (int i = 0; i < updateMessage.size(); i++)
		{
			printf("0x%X ", updateMessage[i]);
		}
		cout << endl;
		cout << endl;
	}
}

void createMidiFile()
{
	// Create MIDI file
	// Add tempo midi event
	// Add note ons and note offs for every octave based on chord changes to the appropriate channel
	// Make sure to use blinking lead-in
	// Write MIDI file
	
	// initialize midi file
	
	midiOutputFile.absoluteTicks();
	midiOutputFile.addTrack(NUM_CHANNELS-1); // 1 channel already present
	midiOutputFile.setTicksPerQuarterNote(TICKS_PER_QUARTER_NOTE);
	
	setTempo(beatsPerMinute);

	
	// add first chord

	for (int channel = 0; channel < NUM_CHANNELS; channel++)
	{
		if (channel == BASS_NOTE_CHANNEL && !indicateBass) continue;

		for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
		{
			int noteBrightness = noteProgressionByChannel[channel][0][noteIndex]-'0';
			if (noteBrightness > 0)
			{
				int tickOffset = noteIndex+1;
				tickOffset = 2;
				addNoteMessage(channel, noteIndex, noteBrightness, 0+tickOffset);
			}
		}
	}
	int tickOffset = 8;
	addUpdateMessage(0 + tickOffset);
		
	
	// add all chord changes

	for (int chordChange = 0; chordChange < chordChanges.size(); chordChange++)
	{
		int beatOfChordChange = chordChanges[chordChange];
		
		int beatOfChangingChord = beatOfChordChange - 1;
		if (beatOfChangingChord < 0) beatOfChangingChord = numBeats-1; // last beat in song

		if (beatOfChordChange != 0) // add all chord change notes except for chord change to beat 0 (first chord already added)
		{
			for (int channel = 0; channel < NUM_CHANNELS; channel++)
			{
				if (channel == BASS_NOTE_CHANNEL && !indicateBass) continue;

				// add chord notes
				for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
				{
					int noteBrightness = noteProgressionByChannel[channel][beatOfChordChange][noteIndex]-'0';
					tickOffset = -2;
					
					addNoteMessage(channel, noteIndex, noteBrightness, (beatOfChordChange*TICKS_PER_QUARTER_NOTE)+tickOffset);
				}
			}
		}

		if (beatOfChordChange != 0 || (beatOfChordChange == 0 && loopMode))
		{
			// add chord lead-in before chord change
			for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
			{
				int nextChordChannel;
				if (chordChange % 2)
				{
					nextChordChannel = ODD_CHORD_CHANNEL;
				}
				else 
				{
					nextChordChannel = EVEN_CHORD_CHANNEL;
				}
				
				if (noteProgression[beatOfChordChange][noteIndex] > '0')
				{
					tickOffset = -2;
					
					int channel;

					if (noteProgression[beatOfChangingChord][noteIndex] == '0')
					{ // chord change adds new note
						channel = nextChordChannel;
					}
					else if (noteProgression[beatOfChordChange][noteIndex] > noteProgression[beatOfChangingChord][noteIndex])
					{ // chord change increases brightness of currently active note
						// current note is the current root/bass note
						if (indicateBass && noteProgressionByChannel[BASS_NOTE_CHANNEL][beatOfChangingChord][noteIndex] > '0')
						{
							channel =  BASS_NOTE_CHANNEL;
						}
						else
						{
							channel = MIXED_CHORD_CHANNEL;
						}
					}
					else
					{
						continue;
					}

					// on beat
					addNoteMessage(channel, noteIndex, noteProgression[beatOfChordChange][noteIndex]-'0', (beatOfChangingChord*TICKS_PER_QUARTER_NOTE)+tickOffset);
					// off beat
					addNoteMessage(channel, noteIndex, noteProgression[beatOfChangingChord][noteIndex]-'0', (beatOfChangingChord*TICKS_PER_QUARTER_NOTE)+(TICKS_PER_QUARTER_NOTE/2)+tickOffset);
				}
			}
		}
	
		// update after writing each chord
		tickOffset = 2;
		addUpdateMessage((beatOfChordChange*TICKS_PER_QUARTER_NOTE)+tickOffset);
		addUpdateMessage((beatOfChangingChord*TICKS_PER_QUARTER_NOTE)+tickOffset); // transistion on beat
		addUpdateMessage((beatOfChangingChord*TICKS_PER_QUARTER_NOTE)+(TICKS_PER_QUARTER_NOTE/2)+tickOffset); // transistion off beat
		
	}

	
	// clear all notes after last beat in song
	
	for (int channel = 0; channel < NUM_CHANNELS; channel++)
	{
		for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
		{
			int noteBrightness = noteProgressionByChannel[channel][numBeats-1][noteIndex] - '0';
			if (noteBrightness > 0)
			{
				tickOffset = 0 - (channel * EMPTY_NOTE_STRING.size() + (noteIndex+1));
				tickOffset = -2;
				addNoteMessage(channel, noteIndex, 0, (numBeats*TICKS_PER_QUARTER_NOTE)+tickOffset);
			}
		}
		
		//tickOffset = channel;
		//addUpdateMessage((numBeats*TICKS_PER_QUARTER_NOTE)+tickOffset, channel);
	}
	
	tickOffset = 0;
	addUpdateMessage((numBeats*TICKS_PER_QUARTER_NOTE)+tickOffset);
	
	
	// finalize and write output file
	
	midiOutputFile.sortTracks();
	midiOutputFile.write(outputFilename);
	
	if (debugMode)
	{
		midiOutputFile.writeBinascWithComments(BINASC_DIRECTORY + outputFilename + ".binasc");
	}
}

void initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	realtime = false;
	brightMode = false;
	indicateBass = false;
	loopMode = true;
	debugMode = false;
	ignoreScales = false;
	
	chordScaleMappingFilename = "";
	inputFilename = "";
	outputFilename = "";

	loadConfig();
	
	// Process command line options
	parseArgs(argc, argv);
	
	for (int i = 1; i < getArgCount(); i++)
	{
		if (processOption(i))
			i++;
	}

	if (getArgCount() == 1)
		realtime = true;

	if (realtime)
	{
		if (chordScaleMappingFilename.size() == 0)
		{
			chordScaleMappingFilename = DEFAULT_CHORD_SCALE_MAPPING_FILENAME;
		}

		ifstream f(chordScaleMappingFilename.c_str());
		bool fileExists = f.good();
		f.close();

		if (fileExists)
		{
			loadChordScaleMapping(chordScaleMappingFilename);
		}
		
		else
		{
			generateChordScaleMapping(chordScaleMappingFilename);
		}	

		return;
	}
	
	if (inputFilename.size() == 0)
	{
		cerr << "Please specify either an input file (-i) or realtime mode (-t)." << endl;
		errorStatus = 1;
		exit(errorStatus);
	}
	
	if (outputFilename.size() == 0)
	{
		int indexOfLastSlash = -1;
		int indexOfLastPeriod = -1;
		for (int i = 0; i < inputFilename.size(); i++)
		{
			if (inputFilename[i] == '/' || inputFilename[i] == '\\') indexOfLastSlash = i;
			if (inputFilename[i] == '.') indexOfLastPeriod = i;
		}
		outputFilename = inputFilename.substr(indexOfLastSlash+1,indexOfLastPeriod-indexOfLastSlash-1) + "_CPV.mid";
	}
	
	loadInput(inputFilename);

	numBeats = chordProgression.size();
	
	if (numBeats == 0)
	{
		cerr << "No chords found in input file. Exiting..." << endl;
		errorStatus = 2;
		exit(errorStatus);
	}	
}

void displaySettings()
{
	cout << endl << "Settings: " << endl;

	cout << "Loop mode (enabled by default): " << boolToText(loopMode) << endl;
	cout << "Indicate bass (disabled by default): " << boolToText(indicateBass) << endl;
	cout << "Bright mode (disabled by default): " << boolToText(brightMode) << endl;
	cout << "Chords only (disabled by default): " << boolToText(ignoreScales) << endl;
	
	cout << endl;
}

void displayChordScaleMapping()
{
	cout << "Chord Scale Mapping (" << chordScaleMappingFilename << "): " << endl;
	cout << getChordScaleMappingString();
	cout << endl;
}

void displayChordMapping()
{
	cout << "Chord Types: " << endl;
	for (map<string, string>::const_iterator it = chordMap.begin(); it != chordMap.end(); it++)
	{
		cout << it->first << "   :   " << it->second << endl;
	}	
	cout << endl;
}

void displayScaleMapping()
{
	cout << "Scale Types: " << endl;
	for (map<string, string>::const_iterator it = scaleMap.begin(); it != scaleMap.end(); it++)
	{
		cout << it->first << "   :   " << it->second << endl;
	}	
	cout << endl;
}

int main(int argc, char** argv) 
{	
	initialize(argc, argv);

	if (debugMode)
	{
		displayChordMapping();
		displayScaleMapping();
	}

	if (realtime)
	{
		if (debugMode)
		{
			displayChordScaleMapping();
		}

		// do realtime stuff

		return errorStatus;
	}

	displaySettings();
	
	if (debugMode)
	{
		cout << "BPM: " << beatsPerMinute << endl;
		cout << endl;
		
		cout << "Number of Beats: " << numBeats << endl;
		cout << endl;
	
		cout << "Chord Progression: " << endl;
		for (int i = 0; i < chordProgression.size(); i++)
			cout << "[" << i << "]: " << chordProgression[i] << " | Root: " << getRoot(chordProgression[i]) << " | Bass: " << getBass(chordProgression[i]) << " | Type: " << getChordType(chordProgression[i]) << endl;
		cout << endl;
	
		cout << "Scale Progression: " << endl;
		for (int i = 0; i < scaleProgression.size(); i++)
			cout << "[" << i << "]: " << scaleProgression[i] << " | Root: " << getRoot(scaleProgression[i]) << " | Bass: " << getBass(scaleProgression[i]) << " | Type: " << getChordType(scaleProgression[i]) << endl;
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
		for (int i = 0; i < NUM_CHANNELS; i++)
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

	if (errorStatus == 0)
	{
		cout << endl;
		cout << "Output file '" << outputFilename << "' successfully written." << endl;
		cout << endl;
	}

	return errorStatus;
}
