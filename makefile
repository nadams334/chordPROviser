UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
	preprocessor-definition := -D __LINUX_ALSA__
	sound-library := -l asound
	thread-library := -l pthread
else
	preprocessor-definition := -D __WINDOWS_MM__
	sound-library := -l winmm
	thread-library := -l multithreaded
endif

all:
	g++ -std=c++11 -Wall $(preprocessor-definition) main.cpp -o chordPROvisor -w -l midifile -l rtmidi $(sound-library) $(thread-library)
	
