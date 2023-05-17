TEMPLATE = app
TARGET = ptcollab
OBJECTS_DIR = ../build/cache
MOC_DIR=../build/cache
DESTDIR=../build

# meta makefile generator breaks .qmake.conf sourcing in actual target makefiles
CONFIG -= debug_and_release

# On my local build top_srcdir is empty for some reason, so set it manually
equals(top_srcdir, "") { top_srcdir="$$PWD/.." }
message("top_srcdir: $${top_srcdir}")
VERSION = "$$cat($${top_srcdir}/version)"
message("Version: $${VERSION}")
!equals(VERSION, "") { DEFINES += "PTCOLLAB_VERSION=$${VERSION}" }

# Including /usr/include/rtmidi since in some dists RtMidi.h is in root dir and
# others it's in a subdir
INCLUDEPATH += . /usr/include/rtmidi
win32|macx:INCLUDEPATH += ../deps/include

win32 {
    contains(QMAKE_TARGET.arch, x86_64) {
        message("64-bit windows target")
        libdeps_dir="x64"
    }
    else {
        message("32-bit windows target")
        libdeps_dir="x86"
    }
    LIBS += -L"$$PWD/../deps/lib/$${libdeps_dir}" -L"$$PWD/deps/lib/$${libdeps_dir}"
}

macx:LIBS += -L/usr/local/lib

pkgconfig_required = false

include("../qmake/findLibrary.pri")

message("Building editor")
win32 {
    message("[default] Adding Windows Multimedia")
    LIBS += -lwinmm
}

tests_ogg = ogg_pkgconfig ogg_lib libogg_static
if(findLibrary("libogg(_static)", tests_ogg, true)) {
    config_ogg_pkgconfig {
        pkgconfig_required = true
        PKGCONFIG += ogg
    }
    config_ogg_lib: LIBS += -logg
    config_libogg_static: LIBS += -llibogg_static
}

tests_vorbisfile = vorbisfile_pkgconfig vorbisfile_lib libvorbisfile_static
if(findLibrary("libvorbisfile", tests_vorbisfile, true)) {
    config_vorbisfile_pkgconfig {
        pkgconfig_required = true
        PKGCONFIG += vorbisfile
    }
    config_vorbisfile_lib: LIBS += -lvorbisfile
    config_libvorbisfile_static: LIBS += -llibvorbis_static -llibvorbisfile_static
}

tests_rtmidi = rtmidi_pkgconfig rtmidi_lib
if(findLibrary("RtMidi", tests_rtmidi, false)) {
    config_rtmidi_pkgconfig {
        pkgconfig_required = true
        PKGCONFIG += rtmidi
    }
    config_rtmidi_lib: LIBS += -lrtmidi
    DEFINES += RTMIDI_SUPPORTED
}

if (equals(pkgconfig_required, "true")) {
    message("Using PKGCONFIG: $$PKGCONFIG")
    CONFIG += link_pkgconfig
}

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets multimedia

CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += pxINCLUDE_OGGVORBIS
# You can make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# Please consult the documentation of the deprecated API in order to know
# how to port your code away from it.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Input
HEADERS += \
           editor/ConnectDialog.h \
           editor/ConnectionStatusLabel.h \
           editor/CopyOptionsDialog.h \
           editor/HostDialog.h \
           editor/InputEvent.h \
           editor/KeySpinBox.h \
           editor/MidiWrapper.h \
           editor/NewWoiceDialog.h \
           editor/RenderDialog.h \
           editor/Settings.h \
           editor/SettingsDialog.h \
           editor/ShortcutsDialog.h \
           editor/sidemenu/SongTitleDialog.h \
           editor/StyleEditor.h \
           editor/WelcomeDialog.h \
           editor/audio/VolumeMeter.h \
           editor/sidemenu/BasicWoiceListModel.h \
           editor/sidemenu/DelayEffectModel.h \
           editor/sidemenu/IconHelper.h \
           editor/sidemenu/OverdriveEffectModel.h \
           editor/sidemenu/TableView.h \
           editor/sidemenu/UserListModel.h \
           editor/sidemenu/VolumeMeterWidget.h \
           editor/sidemenu/WoiceListModel.h \
           editor/views/Animation.h \
           editor/audio/AudioFormat.h \
           editor/Clipboard.h \
           editor/ComboOptions.h \
           editor/DummySyncServer.h \
           editor/EditState.h \
           editor/EditorScrollArea.h \
           editor/EditorWindow.h \
           editor/Interval.h \
           editor/views/KeyboardView.h \
           editor/audio/NotePreview.h \
           editor/views/MeasureView.h \
           editor/views/MooClock.h \
           editor/views/NoteBrush.h \
           editor/views/ParamView.h \
           editor/PxtoneClient.h \
           editor/PxtoneController.h \
           editor/audio/PxtoneIODevice.h \
           editor/sidemenu/PxtoneSideMenu.h \
           editor/audio/PxtoneUnitIODevice.h \
           editor/sidemenu/SelectWoiceDialog.h \
           editor/sidemenu/SideMenu.h \
           editor/sidemenu/UnitListModel.h \
           editor/views/ViewHelper.h \
           network/AbstractServerSession.h \
           network/Client.h \
           network/LocalServerSession.h \
           network/ServerSession.h \
           protocol/Data.h \
           protocol/Hello.h \
           protocol/NoIdMap.h \
           protocol/PxtoneEditAction.h \
           protocol/RemoteAction.h \
           protocol/SerializeVariant.h \
           pxtone/pxtn.h \
           pxtone/pxtnDelay.h \
           pxtone/pxtnDescriptor.h \
           pxtone/pxtnError.h \
           pxtone/pxtnEvelist.h \
           pxtone/pxtnMaster.h \
           pxtone/pxtnMax.h \
           pxtone/pxtnMem.h \
           pxtone/pxtnOverDrive.h \
           pxtone/pxtnPulse_Frequency.h \
           pxtone/pxtnPulse_Noise.h \
           pxtone/pxtnPulse_NoiseBuilder.h \
           pxtone/pxtnPulse_Oggv.h \
           pxtone/pxtnPulse_Oscillator.h \
           pxtone/pxtnPulse_PCM.h \
           pxtone/pxtnService.h \
           pxtone/pxtnText.h \
           pxtone/pxtnUnit.h \
           pxtone/pxtnWoice.h \
           pxtone/pxtoneNoise.h \
           network/BroadcastServer.h
FORMS += \
    editor/ConnectDialog.ui \
    editor/CopyOptionsDialog.ui \
    editor/EditorWindow.ui \
    editor/HostDialog.ui \
    editor/NewWoiceDialog.ui \
    editor/RenderDialog.ui \
    editor/SettingsDialog.ui \
    editor/ShortcutsDialog.ui \
    editor/sidemenu/SongTitleDialog.ui \
    editor/WelcomeDialog.ui \
    editor/sidemenu/SelectWoiceDialog.ui \
    editor/sidemenu/SideMenu.ui
SOURCES += main.cpp \
           editor/ConnectDialog.cpp \
           editor/ConnectionStatusLabel.cpp \
           editor/CopyOptionsDialog.cpp \
           editor/HostDialog.cpp \
           editor/InputEvent.cpp \
           editor/KeySpinBox.cpp \
           editor/MidiWrapper.cpp \
           editor/NewWoiceDialog.cpp \
           editor/RenderDialog.cpp \
           editor/Settings.cpp \
           editor/SettingsDialog.cpp \
           editor/ShortcutsDialog.cpp \
           editor/sidemenu/SongTitleDialog.cpp \
           editor/StyleEditor.cpp \
           editor/WelcomeDialog.cpp \
           editor/audio/VolumeMeter.cpp \
           editor/sidemenu/BasicWoiceListModel.cpp \
           editor/sidemenu/DelayEffectModel.cpp \
           editor/sidemenu/IconHelper.cpp \
           editor/sidemenu/OverdriveEffectModel.cpp \
           editor/sidemenu/TableView.cpp \
           editor/sidemenu/UserListModel.cpp \
           editor/sidemenu/VolumeMeterWidget.cpp \
           editor/sidemenu/WoiceListModel.cpp \
           editor/views/Animation.cpp \
           editor/audio/AudioFormat.cpp \
           editor/Clipboard.cpp \
           editor/ComboOptions.cpp \
           editor/DummySyncServer.cpp \
           editor/EditState.cpp \
           editor/EditorScrollArea.cpp \
           editor/EditorWindow.cpp \
           editor/Interval.cpp \
           editor/views/KeyboardView.cpp \
           editor/audio/NotePreview.cpp \
           editor/views/MeasureView.cpp \
           editor/views/MooClock.cpp \
           editor/views/NoteBrush.cpp \
           editor/views/ParamView.cpp \
           editor/PxtoneClient.cpp \
           editor/PxtoneController.cpp \
           editor/audio/PxtoneIODevice.cpp \
           editor/sidemenu/PxtoneSideMenu.cpp \
           editor/audio/PxtoneUnitIODevice.cpp \
           editor/sidemenu/SelectWoiceDialog.cpp \
           editor/sidemenu/SideMenu.cpp \
           editor/sidemenu/UnitListModel.cpp \
           editor/views/ViewHelper.cpp \
           network/AbstractServerSession.cpp \
           network/Client.cpp \
           network/LocalServerSession.cpp \
           network/ServerSession.cpp \
           protocol/Data.cpp \
           protocol/Hello.cpp \
           protocol/NoIdMap.cpp \
           protocol/PxtoneEditAction.cpp \
           protocol/RemoteAction.cpp \
           pxtone/pxtnDelay.cpp \
           pxtone/pxtnDescriptor.cpp \
           pxtone/pxtnError.cpp \
           pxtone/pxtnEvelist.cpp \
           pxtone/pxtnMaster.cpp \
           pxtone/pxtnMem.cpp \
           pxtone/pxtnOverDrive.cpp \
           pxtone/pxtnPulse_Frequency.cpp \
           pxtone/pxtnPulse_Noise.cpp \
           pxtone/pxtnPulse_NoiseBuilder.cpp \
           pxtone/pxtnPulse_Oggv.cpp \
           pxtone/pxtnPulse_Oscillator.cpp \
           pxtone/pxtnPulse_PCM.cpp \
           pxtone/pxtnService.cpp \
           pxtone/pxtnService_moo.cpp \
           pxtone/pxtnText.cpp \
           pxtone/pxtnUnit.cpp \
           pxtone/pxtnWoice.cpp \
           pxtone/pxtnWoice_io.cpp \
           pxtone/pxtnWoicePTV.cpp \
           pxtone/pxtoneNoise.cpp \
           network/BroadcastServer.cpp

macx:LIBS += -framework Cocoa
macx:OBJECTIVE_SOURCES += editor/MacOsStyleEditor.mm

# Rules for deployment.
isEmpty(PREFIX) {
    win32:PREFIX = C:/$${TARGET}
    else:PREFIX = /opt/$${TARGET}
}

win32:target.path = $$PREFIX
else:target.path = $$PREFIX/bin
INSTALLS += target

# Icons
win32:RC_ICONS = icon.ico
macx:ICON = icon.icns

DISTFILES +=

RESOURCES += icons.qrc \
	styles.qrc

unix {
  desktopfile.files = $$PWD/../res/ptcollab.desktop
  desktopfile.path = $$PREFIX/share/applications/
  INSTALLS += desktopfile

  iconsizes = 16 32 48 64
  for(iconsize, iconsizes) {
    iconres = $${iconsize}x$${iconsize}
    thisicon = iconfile_$${iconres}
    $${thisicon}.files = $$PWD/../res/app_icons/$${iconres}/ptcollab.png
    $${thisicon}.path = $$PREFIX/share/icons/hicolor/$${iconres}/apps/
    INSTALLS += $${thisicon}
  }

  svgicon.files = $$PWD/../res/app_icons/scalable/ptcollab.svg
  svgicon.path = $$PREFIX/share/icons/hicolor/scalable/apps/
  INSTALLS += svgicon
}

distfiles.files = $$PWD/../res/sample_instruments $$PWD/../res/sample_songs
unix:distfiles.path = $$PREFIX/share/ptcollab/
win32:distfiles.path = $$PREFIX
INSTALLS += distfiles

license.files = $$PWD/../LICENSE
unix:license.path = $$PREFIX/share/doc/ptcollab
win32:license.path = $$PREFIX
INSTALLS += license

pxtone_license.files = $$PWD/pxtone/LICENSE-pxtone
unix:pxtone_license.path = $$PREFIX/share/doc/pxtone
win32:pxtone_license.path = $$PREFIX
INSTALLS += pxtone_license

win32 {
  deps_licenses.files = ../deps/license/*
  deps_licenses.path = $$PREFIX
  INSTALLS += deps_licenses
}
