// chordPROviser
// Author: Nathan Adams
// 2016-2018

#include <ctime>
#include <string>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

#include "MidiFile.h"
#include "RtMidi.h"



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
const int NOTES_PER_OCTAVE = 12;
const int STARTING_OCTAVE = 3;
const int ENDING_OCTAVE = 8;
const int NUM_CHANNELS = 4;
const int REALTIME_CHANNEL = 0 - 1;
const int ODD_CHORD_CHANNEL = 1 - 1;
const int EVEN_CHORD_CHANNEL = 2 - 1;
const int MIXED_CHORD_CHANNEL = 3 - 1;
const int BASS_NOTE_CHANNEL = 4 - 1;
const int STARTING_CHANNEL = 11 - 1;

vector<string> noteProgressionByChannel[NUM_CHANNELS];
vector<int> chordChanges; // a list of every beat (zero-based) where a chord changes occurs

const int TICKS_PER_QUARTER_NOTE = 384;

// RtMidi
RtMidiIn* midiIn;
RtMidiOut* midiOut;

const string DEFAULT_RTMIDI_IN_NAME = "chordPROvisor-input";
const string DEFAULT_RTMIDI_OUT_NAME = "chordPROvisor-output";

bool autoConnectALSAPorts = true; // use aconnect to automatically establish ALSA connection on launch
const string DEFAULT_ALSA_INPUT_NAME_1 = "CH345";
const string DEFAULT_ALSA_OUTPUT_NAME_1 = DEFAULT_RTMIDI_IN_NAME;
const string DEFAULT_ALSA_INPUT_NAME_2 = DEFAULT_RTMIDI_OUT_NAME;
const string DEFAULT_ALSA_OUTPUT_NAME_2 = "midiLEDs-input";

std::vector<unsigned char>* lastMidiMessageReceived;

// Midi Messages
const unsigned char noteOnCodeMin = (unsigned char)0x90;
const unsigned char noteOnCodeMax = (unsigned char)0x9F;
const unsigned char noteOffCodeMin = (unsigned char)0x80;
const unsigned char noteOffCodeMax = (unsigned char)0x8F;

const unsigned char ccStatusCodeMin = (unsigned char)0xB0;
const unsigned char ccStatusCodeMax = (unsigned char)0xBF;

const int cc_damper = 64;
const int cc_sostenuto = 66;

const int cc_activate_realtime = /*cc_sostenuto*/29;

bool* damperActive;
bool* sostenutoActive;
bool* realtimeActive;

const int numNotes = 128;
const int numChannels = 16;

vector<int> activeNotes[numChannels];
vector<int> sostenutoNotes[numChannels];
vector<int> damperNotes[numChannels];

string activeChordScale;
string activeSuggestedScale;

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
const string DEFAULT_CHORD_SCALE_MAPPING_FILENAME = "config/map/chord-scale.cfg";

const string CHORD_LIST_FILENAME = "config/chords.cfg";
const string SCALE_LIST_FILENAME = "config/scales.cfg";

map<string, deque<string>> chordScaleMap; // M7 : [ 'ionian , 'lydian , 'mixolydian , ... ]

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

bool realtimeMode;

const string REALTIME_OPTION = "-t";

const string INPUT_FILE_OPTION = "-i";
const string OUTPUT_FILE_OPTION = "-o";
const string LOOP_MODE_OPTION = "-l";
const string BRIGHT_MODE_OPTION = "-b";
const string INDICATE_BASS_OPTION = "-r";
const string DEBUG_OPTION = "-d";
const string CHORDS_ONLY_OPTION = "-c";

const string EMPTY_NOTE_STRING = "000000000000";


void end(int status)
{
	delete midiIn;
	delete midiOut;
	exit(status);
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
	
	if (str.size() == NOTES_PER_OCTAVE && ((numZeroes + numOnes + numTwos) == NOTES_PER_OCTAVE)) return true;
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
		end(errorStatus);
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

	map<string,deque<string>>::iterator it;
	for (it = chordScaleMap.begin(); it != chordScaleMap.end(); it++)
	{
		string chord = it->first;
		string chordName = reverseChordMap[chord];
		if (chordName.size() > 0) chord = chordName;

		deque<string> scales = it->second;

		string scalesString = "[ ";
		for (int i = 0; i < scales.size(); i++)
		{
			string scale = scales[i];
			string scaleName = reverseScaleMap[scale];
			if (scaleName.size() > 0) scale = scaleName;

			scalesString += scale;
			scalesString += " , ";
		}
		scalesString = scalesString.substr(0, scalesString.size() - 3) + " ]";

		chordScaleMappingString += chord + "   :   " + scalesString + "\n";
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
		deque<string> scales;

		vector<string> words = split(line, ' ');

		chord = words[0];
		if (!isValidNoteString(chord)) chord = chordMap[words[0]];
		if (!isValidNoteString(chord))
		{
			cerr << "ERROR (" << filename << "): '" << words[0] << "' is not a valid chord. Exiting..." << endl;
			errorStatus = 2;
			exit(errorStatus);
		}

		for (int i = 1; i < words.size(); i++)
		{
			string word = words[i];
			string scale = "";

			if (word.compare(":") == 0) continue;
			else if (word.compare("[") == 0) continue;
			else if (word.compare("]") == 0) continue;
			else if (word.compare(",") == 0) continue;

			else scale = word;
			if (!isValidNoteString(scale)) scale = scaleMap[word];
			if (!isValidNoteString(scale))
			{
				cerr << "ERROR (" << filename << "): '" << word << "' is not a valid scale. Exiting..." << endl;
				errorStatus = 2;
				exit(errorStatus);
			}

			scales.push_back(scale);
		}

		chordScaleMap.insert(pair<string, deque<string>>(chord, scales));
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
		end(errorStatus);
	}
}

void loadMMAfile(string filename)
{
	cerr << "ERROR: MMA files not yet supported." << endl;
	cerr << "Exiting..." << endl;
	errorStatus = 1;
	end(errorStatus);
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
		end(errorStatus);
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
		realtimeMode = true;
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
		end(errorStatus);
	}
	
	return false;
	
}

void loadChordMap(string filename, map<string, string>* forwardMap, map<string, string>* reverseMap)
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
			end(errorStatus);
		}
		
		forwardMap->insert(pair<string, string>(chordType, noteString));
		reverseMap->insert(pair<string, string>(noteString, chordType));
	}
}

void loadConfig() 
{
	loadChordMap(CHORD_LIST_FILENAME, &chordMap, &reverseChordMap);
	loadChordMap(SCALE_LIST_FILENAME, &scaleMap, &reverseScaleMap);
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

string transposeScale(string scale, string fromRoot, string toRoot)
{
	if (scale.compare(EMPTY_NOTE_STRING) == 0) 
		return scale;
	
	int fromRootIndex = getNoteIndex(fromRoot);
	int toRootIndex = getNoteIndex(toRoot);

	if (fromRootIndex >= 0 && toRootIndex >= 0)
	{
		// shift chord based on specified roots

		int offset = toRootIndex - fromRootIndex;
		if (offset < 0)	offset += NOTES_PER_OCTAVE;

		scale = shiftStringRight(scale, offset);
	}
	else
	{
		cerr << "ERROR - [transposeScale]: One or both root notes unrecognized: " << endl;
		cerr << "fromRoot: " << fromRoot << endl;
		cerr << "toRoot: " << toRoot << endl;
		cerr << "scale: " << scale << endl;
		cerr << "No transposition will be done." << endl;
		errorStatus = 3;
	}
	
	return scale;
}

string addBassNoteToScale(string scale, string bassNote)
{
	int bassIndex = getNoteIndex(bassNote);

	if (bassIndex >= 0)
	{
		// add bass note if not present in chord
		if (scale[bassIndex] == '0') scale[bassIndex] = '1';
	}
	else
	{
		cerr << "ERROR - [addBassNoteToScale]: Bass note unrecognized: " << endl;
		cerr << "bassNote: " << bassNote << endl;
		cerr << "scale: " << scale << endl;
		cerr << "No modification will be done." << endl;
		errorStatus = 3;
	}

	return scale;
}

string generateScale(int index, vector<string> progression)
{
	string scale = getChordNoteString(getChordType(progression[index]));
	scale = transposeScale(scale, "C", getRoot(progression[index]));
	scale = addBassNoteToScale(scale, getBass(progression[index]));
	return scale;
}

void generateNoteProgression() 
{
	for (int i = 0; i < chordProgression.size(); i++)
	{
		string notesInChord = generateScale(i, chordProgression);
		string notesInScale = generateScale(i, scaleProgression);
		
		if (ignoreScales)
			notesInScale = EMPTY_NOTE_STRING;

		noteProgression.push_back(combineChords(notesInChord, notesInScale));
	}

	if (unrecognizedChordTypes) 
	{
		cerr << endl << "ERROR: MIDI file could not be generated. Please update the chord list to include the missing chord types for this song." << endl;
		end(1);
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
		pitchByte = (NOTES_PER_OCTAVE*octave)+noteIndex;
		
		vector<unsigned char> noteMessage;
		noteMessage.push_back(statusByte);
		noteMessage.push_back(pitchByte);
		noteMessage.push_back(velocityByte);
		
		if (ticks < 0)
		{
			midiOut->sendMessage(&noteMessage);
		}
		else
		{
			midiOutputFile.addEvent(channel, ticks, noteMessage);
		}
		
		if (debugMode && false)
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
		
	if (ticks < 0)
	{
		midiOut->sendMessage(&updateMessage);
	}
	else
	{
		midiOutputFile.addEvent(channel, ticks, updateMessage);
	}
		
	if (debugMode && false)
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
		
	if (ticks < 0)
	{
		midiOut->sendMessage(&updateMessage);
	}
	else
	{
		midiOutputFile.addEvent(channel, ticks, updateMessage);
	}
		
	if (debugMode && false)
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

void setNote(int channel, int note, int velocity)
{
	if (velocity > 0) // turning note on
	{
		activeNotes[channel].push_back(note);
		if (damperActive[channel])
		{
			activeNotes[channel].push_back(note);
			damperNotes[channel].push_back(note);
		}
	}
	
	else if (velocity == 0) // turning note off
	{
		if (!activeNotes[channel].empty())
		{
			for (int i = 0; i < activeNotes[channel].size(); i++)
			{
				if (activeNotes[channel][i] == note)
				{
					activeNotes[channel].erase(activeNotes[channel].begin()+i);
				}
			}
		}
	}
	
	else
	{
		cerr << "WARNING: Invalid velocity for note " << note << " on channel " << channel << ". Note message ignored." << endl;
		return;
	}
}

string getScale(string chordScale)
{
	if (!isValidNoteString(chordScale))
	{
		cerr << "INTERNAL ERROR: getScale('" << chordScale << "'): parameter is not valid note string. Ignoring..." << endl;
		return EMPTY_NOTE_STRING;
	}	

	string key = EMPTY_NOTE_STRING;
	for (int i = 0; i < chordScale.size(); i++)
	{
		if (chordScale[i] == '1' || chordScale[i] == '2')
			key[i] = '1';
	}

	if (chordScaleMap.find(key) == chordScaleMap.end())
	{
		if (debugMode) cerr << "WARNING - getScale('" << chordScale << "'): no scale found. Returning empty." << endl;
		return EMPTY_NOTE_STRING;
	}

	deque<string> scales = chordScaleMap[key];
	string scale = scales.front();

	for (int i = 0; i < chordScale.size(); i++)
	{
		if (chordScale[i] == '2')
			scale[i] = '2';
	}
	
	if (debugMode)
		cout << "INFO - getScale('" << chordScale << "'): returned '" << scale << "'" << endl;

	return scale;
}

void outputScale(string scale)
{
	for (int i = 0; i < scale.size(); i++)
	{
		addNoteMessage(REALTIME_CHANNEL, i, scale[i] - '0', -1);
	}
	addUpdateMessage(-1);
}

void setPriorityScale(string chord, string scale)
{
	if (debugMode) cout << "INFO - setPriorityScale('" << chord << "', '" << scale << "')" << endl;

	if (!isValidNoteString(chord))
	{
		if (debugMode) cerr << "WARNING - setPriorityScale('" << chord << "', '" << scale << "'): parameter 1 is not a valid chord. Ignoring..." << endl;
		return;
	}

	if (!isValidNoteString(scale))
	{
		if (debugMode) cerr << "WARNING - setPriorityScale('" << chord << "', '" << scale << "'): parameter 2 is not a valid scale. Ignoring..." << endl;
		return;
	}	

	string normalizedChord = EMPTY_NOTE_STRING;
	for (int i = 0; i < chord.size(); i++)
	{
		if (chord[i] == '2')
			normalizedChord[i] = '1';
	}

	string normalizedScale = EMPTY_NOTE_STRING;
	for (int i = 0; i < scale.size(); i++)
	{
		if (scale[i] == '1' || scale[i] == '2')
			normalizedScale[i] = '1';
	}

	if (chordScaleMap.find(normalizedChord) == chordScaleMap.end())
	{
		if (debugMode) cerr << "WARNING - setPriorityScale('" << chord << "', '" << scale << "'): normalized chord '" << normalizedChord << "' not found. Ignoring..." << endl;
		return;
	}
	deque<string> scales = chordScaleMap[normalizedChord];
	
	for (unsigned int i = 0; i < scales.size(); i++)
	{
		if (scales[i] == normalizedScale)
		{
			scales.erase(scales.begin()+i);
			break;
		}
	}

	scales.push_front(normalizedScale);
	chordScaleMap[normalizedChord] = scales;
}

void activateRealtime(bool enable, int channel)
{
	if (enable)
	{
		for (int i = 0; i < activeNotes[channel].size(); i++)
		{
			int activeNote = activeNotes[channel][i];
			int noteIndex = activeNote % 12;
			if (activeChordScale[noteIndex] == '0')
			{
				int intensity = 0;
				if (realtimeActive[channel])
					intensity = 1;
				else
					intensity = 2;

				activeChordScale[noteIndex] = '0' + intensity;
			}
		}

		string suggestedScale = getScale(activeChordScale);

		if (EMPTY_NOTE_STRING.compare(suggestedScale) != 0 && activeSuggestedScale.compare(suggestedScale) != 0)
		{
			if (realtimeActive[channel])
			{
				setPriorityScale(activeChordScale, suggestedScale);
				outputScale(EMPTY_NOTE_STRING);
			}

			activeSuggestedScale = suggestedScale;	
			outputScale(suggestedScale);
		}

		realtimeActive[channel] = true;
	}
	else
	{
		realtimeActive[channel] = false;
		activeChordScale = EMPTY_NOTE_STRING;
		activeSuggestedScale = EMPTY_NOTE_STRING;

		outputScale(EMPTY_NOTE_STRING);
	}
}

void handleActivateRealtimeMessage(bool enable, int channel)
{
	if (enable)
	{
		if (realtimeActive[channel])
		{
			if (debugMode) std::cerr << "WARNING: Received realtime ON request, but realtime is already active. Ignoring request." << std::endl;
			return;	
		}
	}
	else
	{
		if (!realtimeActive[channel])
		{
			if (debugMode) std::cerr << "WARNING: Received realtime OFF request, but realtime is not currently active. Ignoring request." << std::endl;
			return;
		}
	}

	activateRealtime(enable, channel);
}

void handleDamperMessage(bool enable, int channel)
{
	if (enable)
	{
		if (damperActive[channel])
		{
			if (debugMode) cerr << "WARNING: Received damper ON request, but damper is already active. Ignoring request." << endl;
			return;
		}

		damperActive[channel] = true;

		unsigned int size = activeNotes[channel].size();
		for (int i = 0; i < size; i++)
		{
			int activeNote = activeNotes[channel][i];
			activeNotes[channel].push_back(activeNote);
			damperNotes[channel].push_back(activeNote);
		}
	}
	else
	{
		if (!damperActive[channel])
		{
			if (debugMode) cerr << "WARNING: Received damper OFF request, but damper is not currently active. Ignoring request." << endl;
			return;
		}

		damperActive[channel] = false;

		// Turn all damper notes off
		for (int i = 0; i < damperNotes[channel].size(); i++)
		{
			int activeNote = damperNotes[channel][i];
			setNote(channel, activeNote, 0); // turn note off
		}	

		damperNotes[channel].clear();
	}
}

void handleSostenutoMessage(bool enable, int channel)
{
	if (enable)
	{
		if (sostenutoActive[channel])
		{
			if (debugMode) cerr << "WARNING: Received sostenuto ON request, but sostenuto is already active. Ignoring request." << endl;
			return;
		}

		sostenutoActive[channel] = true;

		unsigned int size = activeNotes[channel].size();
		for (int i = 0; i < size; i++)
		{
			int activeNote = activeNotes[channel][i];
			activeNotes[channel].push_back(activeNote);
			sostenutoNotes[channel].push_back(activeNote);
		}
	}
	else
	{
		if (!sostenutoActive[channel])
		{
			if (debugMode) cerr << "WARNING: Received sostenuto OFF request, but sostenuto is not currently active. Ignoring request." << endl;
			return;
		}

		sostenutoActive[channel] = false;

		// Turn all sostenuto notes off
		for (int i = 0; i < sostenutoNotes[channel].size(); i++)
		{
			int activeNote = sostenutoNotes[channel][i];
			setNote(channel, activeNote, 0); // turn note off
		}	

		sostenutoNotes[channel].clear();
	}
}

void onMidiMessageReceived(double deltatime, std::vector<unsigned char>* message, void* userData)
{
	lastMidiMessageReceived->clear();
	for (unsigned int i = 0; i < message->size(); i++)
	{
		lastMidiMessageReceived->push_back(message->at(i));
	}

	int code = (int) message->at(0);

	if (code >= noteOnCodeMin && code <= noteOnCodeMax)
	{
		int channel = code - noteOnCodeMin;
		setNote(channel, message->at(1), message->at(2));

		if (message->at(2) > 0 && realtimeMode && realtimeActive[channel])
		{
			activateRealtime(true, channel);
		}
	}	
	else if (code >= noteOffCodeMin && code <= noteOffCodeMax)
	{
		int channel = code - noteOffCodeMin;
		setNote(channel, message->at(1), 0);
	}
	else if (code >= ccStatusCodeMin && code <= ccStatusCodeMax)
	{
		int ccCode = (int) message->at(1);
		int value = (int) message->at(2);
		int channel = code - (int) ccStatusCodeMin;

		if (ccCode == cc_sostenuto)
		{
			handleSostenutoMessage(value > 0, channel);
		}
		else if (ccCode == cc_damper)
		{
			handleDamperMessage(value > 0, channel);
		}

		if (ccCode = cc_activate_realtime)
		{
			handleActivateRealtimeMessage(value > 0, channel);
		}
	}
}

void initializeRtMidi()
{
	damperActive = new bool[numChannels];
	sostenutoActive = new bool[numChannels];
	realtimeActive = new bool[numChannels];

	for (int i = 0; i < numChannels; i++)
	{
		damperActive[i] = false;
		sostenutoActive[i] = false;
		realtimeActive[i] = false;
	}

	midiIn = new RtMidiIn(RtMidi::Api::UNSPECIFIED, DEFAULT_RTMIDI_IN_NAME, 100);
	midiIn->openVirtualPort();
	midiIn->setCallback( &onMidiMessageReceived );
	midiIn->ignoreTypes( true, true, true );

	midiOut = new RtMidiOut(RtMidi::Api::UNSPECIFIED, DEFAULT_RTMIDI_OUT_NAME);
	midiOut->openVirtualPort();

	lastMidiMessageReceived = new std::vector<unsigned char>();

	// Connect ALSA Ports
	if (autoConnectALSAPorts)
	{
		string command1 = "aconnect \"" + DEFAULT_ALSA_INPUT_NAME_1 + "\" \"" + DEFAULT_ALSA_OUTPUT_NAME_1 + "\"";
		string command2 = "aconnect \"" + DEFAULT_ALSA_INPUT_NAME_2 + "\" \"" + DEFAULT_ALSA_OUTPUT_NAME_2 + "\"";		
		
		system(command1.c_str());
		system(command2.c_str());
	}
}

void initialize(int argc, char** argv)
{
	// Initialize variables
	errorStatus = 0;
	realtimeMode = false;
	brightMode = false;
	indicateBass = false;
	loopMode = true;
	debugMode = false;
	ignoreScales = false;
	
	chordScaleMappingFilename = "";
	inputFilename = "";
	outputFilename = "";

	activeChordScale = EMPTY_NOTE_STRING;
	activeSuggestedScale = EMPTY_NOTE_STRING;

	loadConfig();
	
	// Process command line options
	parseArgs(argc, argv);
	
	for (int i = 1; i < getArgCount(); i++)
	{
		if (processOption(i))
			i++;
	}

	if (getArgCount() == 1)
		realtimeMode = true;

	if (realtimeMode)
	{
		initializeRtMidi();

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

		cout << endl << "Realtime mode active." << endl << endl;

		return;
	}
	
	if (inputFilename.size() == 0)
	{
		cerr << "Please specify either an input file (-i) or realtime mode (-t)." << endl;
		errorStatus = 1;
		end(errorStatus);
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
		end(errorStatus);
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

void realtimeLoop()
{
	char input;
	while (cin.get(input))
	{

	}
	cin.clear();
	cout << endl << "Received EOF." << endl;
}

string getFormattedTimestamp()
{
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];

	time (&rawtime);
	timeinfo = localtime(&rawtime);
	
	strftime(buffer,sizeof(buffer),"%m-%d-%Y_%I:%M:%S",timeinfo);
	string str(buffer);

	return str;
}

void wouldYouLikeToSave()
{
	cout << "Would you like to save changes to chord-scale priority?" << endl;
	cout << "1) Yes" << endl;
	cout << "2) No" << endl;
	cout << "Please enter a number: ";

	int input;
	cin >> input;
	cout << endl;

	if (input == 1)
	{
		cout << "Would you like to create a new file or overwrite '" << chordScaleMappingFilename << "'?" << endl;
		cout << "1) Save to new file" << endl;
		cout << "2) Overwrite" << endl;
		cout << "Please enter a number: ";

		cin >> input;
		cout << endl;

		string fileToSaveTo = "";

		if (input == 2)
		{
			fileToSaveTo = chordScaleMappingFilename;
		}
		else
		{
			fileToSaveTo = chordScaleMappingFilename.substr(0, chordScaleMappingFilename.find_last_of(".")) + "[" + getFormattedTimestamp() + "]" + chordScaleMappingFilename.substr(chordScaleMappingFilename.find_last_of("."), chordScaleMappingFilename.size());
		}

		writeChordScaleMapping(fileToSaveTo);
		cout << "Changes have been saved to '" << fileToSaveTo << "'." << endl;
	}
	else
	{
		cout << "Changes will not be saved." << endl;
	}
}

int main(int argc, char** argv) 
{	
	initialize(argc, argv);

	if (debugMode)
	{
		displayChordMapping();
		displayScaleMapping();
	}

	if (realtimeMode)
	{
		if (debugMode)
		{
			displayChordScaleMapping();
		}

		realtimeLoop();

		cout << endl;

		wouldYouLikeToSave();

		cout << endl;

		end(errorStatus);
	}

	// else continue to standard chord progression mode

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

	end(errorStatus);
}
