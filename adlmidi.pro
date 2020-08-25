TEMPLATE = subdirs

SUBDIRS = \
    adlcpp \
    midiplay \
    gen_adldata

adlcpp.file = src/libadlcpp.pro
midiplay.file = src/midiplay.pro
gen_adldata.file = utils/gen_adldata.pro

midiplay.depends = adlcpp
