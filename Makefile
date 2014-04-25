#CXX=i686-pc-mingw32-g++ -static
CXX=g++
CXXLINK=$(CXX)

#DEBUG=-O0 -fno-inline -D_GLIBCXX_DEBUG -g -fstack-protector-all -fdata-sections

DEBUG=-Ofast -g

#DEBUG += -fno-tree-vectorize

# -march=pentium -mno-sse -mno-sse2 -mno-sse3 -mmmx

CPPFLAGS+=$$(pkg-config --cflags sdl)
LDLIBS+=$$(pkg-config --libs sdl)

CPPFLAGS += $(SDL)

CPPFLAGS += -std=c++11 -pedantic -Wall -Wextra

include make.rules

