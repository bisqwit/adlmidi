CXX=g++

#DEBUG=-O0 -fno-inline -D_GLIBCXX_DEBUG -g -fstack-protector-all -fdata-sections
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

gen_adldata: gen_adldata.o dbopl.o
	$(CXX) -ansi $^ -Wall -W $(DEBUG) $(SDL) -o $@ 

gen_adldata.o: gen_adldata.cc dbopl.h
	$(CXX) -ansi $< -Wall -W $(DEBUG) $(SDL) -c -o $@ 

dumpmiles: dumpmiles.o
	$(CXX) -ansi $^ -Wall -W $(DEBUG) $(SDL) -o $@ 

dumpmiles.o: dumpmiles.cc
	$(CXX) -ansi $< -Wall -W $(DEBUG) $(SDL) -c -o $@ 

dumpbank: dumpbank.o
	$(CXX) -ansi $^ -Wall -W $(DEBUG) $(SDL) -o $@ 

dumpbank.o: dumpbank.cc
	$(CXX) -ansi $< -Wall -W $(DEBUG) $(SDL) -c -o $@ 
