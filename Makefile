#DEBUG=-O1 -fno-inline -D_GLIBCXX_DEBUG -g
DEBUG=-O3 -g
midiplay: midiplay.cc dbopl.cpp dbopl.h
	$(CXX) -ansi midiplay.cc -Wall -W dbopl.cpp $(DEBUG) \
		`pkg-config --cflags --libs sdl` -o $@
