CONFIG -= qt

TEMPLATE = lib
DEFINES += LIBADLCPP_LIBRARY

CONFIG += c++17

QMAKE_CXXFLAGS += -fopenmp -std=c++17
LIBS += -fopenmp

QMAKE_CXXFLAGS += -Wno-implicit-fallthrough

DEFINES += SUPPORT_VIDEO_OUTPUT SUPPORT_PUZZLE_GAME ADLMIDI_BUILD

INCLUDEPATH += $$PWD/../include/

TARGET = adlcpp

DESTDIR = $$PWD/../lib

SOURCES += \
    adldata.cc \
    adlinput.cc \
    adlplay.cc \
    adlui.cc \
    nukedopl3.c

HEADERS += \
    adldata.hh \
    adlpriv.hh \
    nukedopl3.h \
    puzzlegame.inc \
    6x9.inc \
    8x16.inc \
    9x15.inc \
    ../include/adlcpp.h \
    ../include/adlglob.h \
    ../include/adlinput.h \
    ../include/adlui.h \
    ../include/adlplay.h \
    ../include/fraction.hpp
