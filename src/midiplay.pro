TEMPLATE = app
CONFIG -= qt
CONFIG += console
CONFIG += c++17

!win32:{
CONFIG += link_pkgconfig
PKGCONFIG += sdl2
}

QMAKE_CXXFLAGS += -fopenmp -std=c++17
LIBS += -fopenmp

win32:{
    LIBS += -lwinmm -static-libgcc -static-libstdc++ -static -lpthread
}
INCLUDEPATH += $$PWD/include/

TARGET = adlmidi

SOURCES += \
    adldata.cc \
    adlinput.cc \
    dbopl.cpp \
    midiplay.cc

HEADERS += \
    ../include/adlinput.h \
    adldata.hh \
    adlpriv.hh \
    dbopl.h \
    fraction \
    puzzlegame.inc \
    6x9.inc \
    8x16.inc \
    9x15.inc \
    ../include/adlcpp.h
