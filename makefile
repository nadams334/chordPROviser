all:
	g++ -std=c++11 -Wall -D__LINUX_ALSA__ main.cpp -o chordPROvisor -w -l midifile -l rtmidi -l asound -l pthread
	
