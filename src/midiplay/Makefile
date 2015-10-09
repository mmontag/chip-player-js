all: midiplay
midiplay: adlmidiplay.cpp
	g++ $^ -Wl,-rpath='$$ORIGIN' -I.. -L../../bin -ladlmidi -lSDL2 -o ../../bin/adlmidiplay

