#CXX=i686-w64-mingw32-g++ -m64
#CXX=x86_64-w64-mingw32-g++ -m64 -static-libgcc -static-libstdc++

CXX     = g++
CXXLINK = $(CXX)
RM      = rm
AR      = ar

#DEBUG = -O0 -fno-inline -D_GLIBCXX_DEBUG -g -fstack-protector-all -fdata-sections -fsanitize=address

DEBUG=-Ofast -g -fopenmp -march=native

ADLMIDI_LIBNAME         = libadlcpp.so
ADLMIDI_STATICLIBNAME   = libadlcpp.a

MIDIPLAY_LINK_LIB=$(ADLMIDI_LIBNAME)

#DEBUG += -fno-tree-vectorize

# -march=pentium -mno-sse -mno-sse2 -mno-sse3 -mmmx

CPPFLAGS += $(shell pkg-config --cflags sdl2) -Wno-unused-result -Wno-implicit-fallthrough
LDLIBS   += $(shell pkg-config --libs sdl2) -L. -ladlcpp -no-pie
#CPPFLAGS += $(SDL)

#LDLIBS += -lwinmm

CPPFLAGS += -std=c++17 -pedantic -Wall -Wextra

include make.rules

