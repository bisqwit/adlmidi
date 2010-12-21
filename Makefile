CXX=g++

#DEBUG=-O1 -fno-inline -D_GLIBCXX_DEBUG -g
DEBUG=-O3 -g

# For Cygwin:
#SDL=-I/usr/local/include/SDL -L/usr/local/lib -lSDL

# For anything else:
SDL=`pkg-config --cflags --libs sdl`

midiplay: midiplay.o dbopl.o
	$(CXX) -ansi $^ -Wall -W $(DEBUG) $(SDL) -o $@ 

midiplay.o: midiplay.cc dbopl.h
	$(CXX) -ansi $< -Wall -W $(DEBUG) $(SDL) -c -o $@ 

dbopl.o: dbopl.cpp dbopl.h
	$(CXX) -ansi $< -Wall -W $(DEBUG) $(SDL) -c -o $@ 
