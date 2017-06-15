// chordPROviser
// Author: Nathan Adams
// 2016-2017

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

// Song info
int beatsPerMinute = 0;
int numBeats = 0;

vector<string> chordProgression;
vector<string> scaleProgression;
vector<string> noteProgression;

const int startingOctave = 3;
const int endingOctave = 8;
const int numChannels = 4;
const int ODD_CHORD_CHANNEL = 0;
const int EVEN_CHORD_CHANNEL = 1;
const int MIXED_CHORD_CHANNEL = 2;
const int BASS_NOTE_CHANNEL = 3;

vector<string> noteProgressionByChannel[numChannels];
vector<int> chordChanges; // a list of every beat (zero-based) where a chord changes occurs

const int TICKS_PER_QUARTER_NOTE = 48;

// File I/O
MidiFile midiOutputFile;

enum IOtype { Input, Output };
enum InputFileType { MMA, TXT, Other };

string inputFilename;
string outputFilename;

InputFileType inputFileType;

const string OUTPUT_DIRECTORY = "output/";
const string BINASC_DIRECTORY = "binasc/";

// Config data
const string CHORD_LIST_FILENAME = "config/chords.cfg";

map<string, string> chordMap;

const int dimnessFactor = 8;
const int brightnessFactor = 10; // bright notes will be this factor brighter than dim notes (ie. 10 = 10x as bright)
const int dimNoteVelocity = dimnessFactor - 1;
const int brightNoteVelocity = (dimnessFactor * brightnessFactor) - 1;

// Command line args
vector<string> commandLineArgs;

bool loopMode;
bool brightMode;
bool indicateBass;
bool debugMode;

const string INPUT_FILE_OPTION = "-i";
const string OUTPUT_FILE_OPTION = "-o";
const string LOOP_MODE_OPTION = "-l";
const string BRIGHT_MODE_OPTION = "-b";
const string INDICATE_BASS_OPTION = "-r";
const string DEBUG_OPTION = "-d";

const string EMPTY_NOTE_STRING = "000000000000";


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

void debugOptions()
{
	cout << endl << "Args(" << getArgCount() << "): " << endl;

	for (int i = 0; i < getArgCount(); i++)
	{
		cout << getArg(i) << endl;
	}

	cout << endl << "Options: " << endl;
	cout << "Loop mode enabled (default 1): " << loopMode << endl;
	cout << "Bright mode (default 0): " << brightMode << endl;
	cout << "Indicate bass (default 1): " << indicateBass << endl;
	
	cout << endl;
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
			optionName = INPUT_FILE_OPTION;
			inputFilename = arg;
			break;
		case Output:
			optionName = OUTPUT_FILE_OPTION;
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

	if (arg.compare(INPUT_FILE_OPTION) == 0)
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
		
	if (chordNoteString.size() == 0)
	{
		cerr << "Unrecogonized chord type: " << chordType << endl;
		chordNoteString = EMPTY_NOTE_STRING;
	}
	
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
		noteProgression.push_back(combineChords(notesInChord, notesInScale));
	}
}

void separateNotesOfChordChange(int indexOfFirstChord, int indexOfSecondChord, bool oddToEven)
{
		string firstChord = noteProgression[indexOfFirstChord];
		string secondChord = noteProgression[indexOfSecondChord];
		
		string notesByChannel[numChannels];
		for (int i = 0; i < numChannels; i++)
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
		}
		
		// fill in all bars of the first chord
		for (int beat = indexOfFirstChord; beat != indexOfSecondChord && beat < noteProgression.size(); beat++)
		{
			for (int channel = 0; channel < numChannels; channel++)
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
	for (int channel = 0; channel < numChannels; channel++)
	{
		for (int beat = 0; beat < noteProgression.size(); beat++)
			noteProgressionByChannel[channel].push_back(EMPTY_NOTE_STRING);
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
	
	for (int channel = 0; channel < numChannels; channel++)
	{
		midiOutputFile.addEvent(channel, 0, setTempoMessage); // add tempo change at start of each channel
	}
	*/
	
	for (int channel = 0; channel < numChannels; channel++)
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
	statusByte += channel;
	
	unsigned char velocityByte = 0x00;
	if (noteBrightness == 1)
		velocityByte = dimNoteVelocity;
	else if (noteBrightness == 2)
		velocityByte = brightNoteVelocity;
		
	unsigned char pitchByte;
	for (int octave = startingOctave; octave < endingOctave; octave++)
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
	for (int channel = 0; channel < numChannels; channel++)
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
	for (int channel = 0; channel < numChannels; channel++)
	{
		if (!indicateBass && channel == BASS_NOTE_CHANNEL) continue;
		addEndOfTrackMessage(channel, ticks);
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
	midiOutputFile.addTrack(numChannels-1); // 1 channel already present
	midiOutputFile.setTicksPerQuarterNote(TICKS_PER_QUARTER_NOTE);
	
	setTempo(beatsPerMinute);

	
	// add first chord

	for (int channel = 0; channel < numChannels; channel++)
	{
		if (channel == BASS_NOTE_CHANNEL && !indicateBass) continue;

		for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
		{
			int noteBrightness = noteProgressionByChannel[channel][0][noteIndex]-'0';
			if (noteBrightness > 0)
			{
				addNoteMessage(channel, noteIndex, noteBrightness, 0);
			}
		}
	}
		
	
	// add all chord changes

	for (int chordChange = 0; chordChange < chordChanges.size(); chordChange++)
	{
		for (int channel = 0; channel < numChannels; channel++)
		{
			if (channel == BASS_NOTE_CHANNEL && !indicateBass) continue;
			
			int beatOfChordChange = chordChanges[chordChange];
			
			if (beatOfChordChange != 0) // don't need to re-add first chord, only need to add blinking lead-in
			{
				// add chord notes
				for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
				{
					int noteBrightness = noteProgressionByChannel[channel][beatOfChordChange][noteIndex]-'0';
					addNoteMessage(channel, noteIndex, noteBrightness, beatOfChordChange*TICKS_PER_QUARTER_NOTE);
				}
			}
			else if (!loopMode)
			{
				return; // only show chord change lead-in back to beat 0 if loop mode is enabled
			}
			
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
				
				int beatOfChangingChord = beatOfChordChange - 1;
				if (beatOfChangingChord < 0) beatOfChangingChord = numBeats-1; // last beat in song
				if (noteProgression[beatOfChordChange][noteIndex] > '0' && noteProgression[beatOfChangingChord][noteIndex] == '0')
				{
					// on beat
					addNoteMessage(nextChordChannel, noteIndex, noteProgressionByChannel[nextChordChannel][beatOfChordChange][noteIndex]-'0', beatOfChangingChord*TICKS_PER_QUARTER_NOTE);
					// off beat
					addNoteMessage(nextChordChannel, noteIndex, 0, (beatOfChangingChord*TICKS_PER_QUARTER_NOTE)+(TICKS_PER_QUARTER_NOTE/2));
				}
			}
		}	
	}

	
	// clear all notes after last beat in song
	
	for (int channel = 0; channel < numChannels; channel++)
	{
		for (int noteIndex = 0; noteIndex < EMPTY_NOTE_STRING.size(); noteIndex++)
		{
			int noteBrightness = noteProgressionByChannel[channel][numBeats-1][noteIndex] - '0';
			if (noteBrightness > 0)
			{
				addNoteMessage(channel, noteIndex, 0, numBeats*TICKS_PER_QUARTER_NOTE);
			}
		}
	}
	
	
	// finalize and write output file
	
	midiOutputFile.sortTracks();
	midiOutputFile.write(OUTPUT_DIRECTORY + outputFilename);
	
	if (debugMode)
	{
		midiOutputFile.writeBinascWithComments(BINASC_DIRECTORY + outputFilename + ".binasc");
	}
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

	numBeats = chordProgression.size();
	
	if (numBeats == 0)
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
		debugOptions();
		
		cout << "Chord Types: " << endl;
		for (map<string, string>::const_iterator it = chordMap.begin(); it != chordMap.end(); it++)
		{
			cout << it->first << "   :   " << it->second << endl;
		}	
		cout << endl;
	
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
		for (int i = 0; i < numChannels; i++)
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
