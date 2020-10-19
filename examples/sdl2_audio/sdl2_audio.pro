TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

CONFIG += link_pkgconfig
PKGCONFIG += sdl2

SOURCES += \
        main.cpp

INCLUDEPATH += $$PWD/../../include

LIBS += -L$$PWD/../../lib -ladlcpp
