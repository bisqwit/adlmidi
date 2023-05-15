TEMPLATE = app
CONFIG -= qt
CONFIG += console
CONFIG += c++17

!win32:{
CONFIG += link_pkgconfig
PKGCONFIG += sdl2
}

QMAKE_CXXFLAGS += -std=c++17

win32:{
    LIBS += -lwinmm
}

INCLUDEPATH += $$PWD/../include/ $$PWD/../src/

DEFINES += SUPPORT_VIDEO_OUTPUT SUPPORT_PUZZLE_GAME

TARGET = adlmidi

SOURCES += \
    midiplay.cc

LIBS += -L$$PWD/../lib -ladlcpp
