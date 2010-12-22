VERSION=1.0.1
ARCHNAME=adlmidi-$(VERSION)
ARCHDIR=archives/

ARCHFILES=\
	midiplay.cc \
	dbopl.cpp dbopl.h \
	dumpbank.cc dumpmiles.cc \
	gen_adldata.cc \
	midiplay.bas \
	progdesc.php \
	\
	bnk_files/drum.bnk \
	bnk_files/file131.bnk \
	bnk_files/file132.bnk \
	bnk_files/file133.bnk \
	bnk_files/file134.bnk \
	bnk_files/file142.bnk \
	bnk_files/file143.bnk \
	bnk_files/file144.bnk \
	bnk_files/file145.bnk \
	bnk_files/file159.bnk \
	bnk_files/file167.bnk \
	bnk_files/file168.bnk \
	bnk_files/hamdrum.bnk \
	bnk_files/hammelo.bnk \
	bnk_files/intdrum.bnk \
	bnk_files/intmelo.bnk \
	bnk_files/melodic.bnk \
	bnk_files/rickdrum.bnk \
	bnk_files/rickmelo.bnk \
	doom2/genmidi.htc \
	doom2/genmidi.op2 \
	ibk_files/game.ibk \
	ibk_files/mt_fm.ibk \
	ibk_files/soccer-genmidi.ibk \
	ibk_files/soccer-percs.ibk \
	opl_files/file12.opl \
	opl_files/file13.opl \
	opl_files/file15.opl \
	opl_files/file16.opl \
	opl_files/file17.opl \
	opl_files/file19.opl \
	opl_files/file20.opl \
	opl_files/file21.opl \
	opl_files/file23.opl \
	opl_files/file24.opl \
	opl_files/file25.opl \
	opl_files/file26.opl \
	opl_files/file27.opl \
	opl_files/file29.opl \
	opl_files/file30.opl \
	opl_files/file31.opl \
	opl_files/file32.opl \
	opl_files/file34.opl \
	opl_files/file35.opl \
	opl_files/file36.opl \
	opl_files/file37.opl \
	opl_files/file41.opl \
	opl_files/file42.opl \
	opl_files/file47.opl \
	opl_files/file48.opl \
	opl_files/file49.opl \
	opl_files/file50.opl \
	opl_files/file53.opl \
	opl_files/file54.opl \
	opl_files/sample.ad \
	opl_files/sample.opl \
	opl_files/sc3.opl \
	opl_files/simfarm.ad \
	opl_files/simfarm.opl \
	opl_files/warcraft.ad

INSTALLPROGS=adlmidi

CXX=g++

#DEBUG=-O0 -fno-inline -D_GLIBCXX_DEBUG -g -fstack-protector-all -fdata-sections
DEBUG=-O3 -g

# For Cygwin:
#SDL=-I/usr/local/include/SDL -L/usr/local/lib -lSDL

# For anything else:
SDL=`pkg-config --cflags --libs sdl`

all: adlmidi gen_adldata dumpmiles dumpbank

adlmidi: midiplay.o dbopl.o
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


include depfun.mak
