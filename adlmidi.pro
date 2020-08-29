TEMPLATE = subdirs

SUBDIRS = \
    adlcpp \
    midiplay \
    gen_adldata

adlcpp.file = src/libadlcpp.pro
midiplay.file = src/midiplay.pro
gen_adldata.file = utils/gen_adldata.pro

midiplay.depends = adlcpp

!win32:{
    SUBDIRS += sdl2example
    sdl2example.file = examples/sdl2_audio/sdl2_audio.pro
    sdl2example.depends = adlcpp
}
