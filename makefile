UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
	preprocessor-definition := __LINUX_ALSA__
	sound-library := asound
	thread-library := pthread
else
	preprocessor-definition := __WINDOWS_MM__
	sound-library := winmm
	thread-library := multithreaded
endif

all:
	g++ -std=c++11 -Wall -D $(preprocessor-definition) main.cpp -o chordPROvisor -w -l midifile -l rtmidi -l $(sound-library) -l $(thread-library)
	
