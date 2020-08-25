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

INCLUDEPATH += $$PWD/../include/

DEFINES += SUPPORT_VIDEO_OUTPUT SUPPORT_PUZZLE_GAME

TARGET = adlmidi

SOURCES += \
    midiplay.cc

LIBS += -L$$PWD/../lib -ladlcpp
