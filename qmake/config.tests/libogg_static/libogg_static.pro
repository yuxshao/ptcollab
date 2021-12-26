include("../common.pri")

INCLUDEPATH += . /usr/local/include/rtmidi /usr/local/include /usr/include/rtmidi /usr/include
win32|macx:INCLUDEPATH += ../../../deps/include

win32:LIBS += -L"$$PWD/../../../deps/lib" -L"$$PWD/../../deps/lib"
macx:LIBS += -L/usr/local/lib

DEFINES += OGG
LIBS += -llibogg_static32
