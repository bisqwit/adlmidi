TEMPLATE = app
CONFIG -= qt
CONFIG += console
CONFIG += c++11

TARGET = gen_adldata

QMAKE_CXXFLAGS += -Wno-implicit-fallthrough

INCLUDEPATH += $$PWD/../src

SOURCES += \
    gen_adldata.cc \
    dbopl.cpp

HEADERS += \
    $$PWD/dbopl.h

