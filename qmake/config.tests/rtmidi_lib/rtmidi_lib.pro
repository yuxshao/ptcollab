include("../common.pri")

INCLUDEPATH += . /usr/local/include/rtmidi /usr/local/include /usr/include/rtmidi /usr/include
win32|macx:INCLUDEPATH += ../../../deps/include

win32 {
    contains(QMAKE_TARGET.arch, x86_64) { libdeps_dir="x64" }
    else { libdeps_dir="x86" }
    LIBS += -L"$$PWD/../../../deps/lib/$$libdeps_dir" -L"$$PWD/../../deps/lib/$$libdeps_dir"
}
macx:LIBS += -L/usr/local/lib

DEFINES += RTMIDI
LIBS += -lrtmidi
