VERSION=1.1.2
ARCHNAME=adlmidi-$(VERSION)
ARCHDIR=archives/
NOGZIPARCHIVES=1

ARCHFILES=\
	midiplay.cc \
	dbopl.cpp dbopl.h \
	adldata.cc adldata.hh \
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

#CXX=i686-pc-mingw32-g++ -static
CXX=g++
CXXLINK=$(CXX)

#DEBUG=-O0 -fno-inline -D_GLIBCXX_DEBUG -g -fstack-protector-all -fdata-sections
DEBUG=-O3 -g -fexpensive-optimizations -ffast-math
#DEBUG += -fno-tree-vectorize
# -march=pentium -mno-sse -mno-sse2 -mno-sse3 -mmmx

CPPFLAGS+=`test -f /bin/sh.exe||pkg-config --cflags sdl`
LDLIBS+=`test -f /bin/sh.exe||pkg-config --libs sdl`

CPPFLAGS += $(SDL)

CPPFLAGS += -ansi -Wall -W 

CXX += `test -f /bin/sh.exe&&echo '-mwin32 -mconsole -mno-cygwin'`
CPPFLAGS += `test -f /bin/sh.exe&&echo '-I/usr/include/mingw -mno-cygwin -I/usr/include/w32api'`
LDLIBS += `test -f /bin/sh.exe&&echo '-L/usr/local/lib -L/usr/lib/w32api -lwinmm'`
# ^For cygwin. For anything else, remove this line.

all: adlmidi gen_adldata dumpmiles dumpbank

adlmidi: midiplay.o dbopl.o adldata.o
	$(CXXLINK)  $^  $(DEBUG) $(SDL) -o $@ $(LDLIBS)

midiplay.o: midiplay.cc dbopl.h adldata.hh
	$(CXX) $(CPPFLAGS) $<  $(DEBUG) $(SDL) -c -o $@ 

dbopl.o: dbopl.cpp dbopl.h
	$(CXX) $(CPPFLAGS) $<  $(DEBUG)  -c -o $@ 

adldata.o: adldata.cc adldata.hh
	$(CXX) $(CPPFLAGS) $<  $(DEBUG)  -c -o $@ 

gen_adldata: gen_adldata.o dbopl.o
	$(CXXLINK)  $^  $(DEBUG)  -o $@  $(LDLIBS)

gen_adldata.o: gen_adldata.cc dbopl.h
	$(CXX) $(CPPFLAGS) $<  $(DEBUG)  -c -o $@ 

dumpmiles: dumpmiles.o
	$(CXXLINK)  $^  $(DEBUG)  -o $@  $(LDLIBS)

dumpmiles.o: dumpmiles.cc
	$(CXX) $(CPPFLAGS) $<  $(DEBUG)  -c -o $@ 

dumpbank: dumpbank.o
	$(CXXLINK)  $^  $(DEBUG)  -o $@  $(LDLIBS)

dumpbank.o: dumpbank.cc
	$(CXX) $(CPPFLAGS) $<  $(DEBUG)  -c -o $@ 


include depfun.mak
