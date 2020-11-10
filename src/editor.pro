######################################################################
# Automatically generated by qmake (3.1) Sun May 24 23:49:39 2020
######################################################################

TEMPLATE = app
TARGET = ptcollab
INCLUDEPATH += .
win32:INCLUDEPATH += ../deps/include
macx:INCLUDEPATH += ../deps/include
QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.14

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
           editor/HostDialog.h \
           editor/RenderDialog.h \
           editor/Settings.h \
           editor/ShortcutsDialog.h \
           editor/sidemenu/BasicWoiceListModel.h \
           editor/sidemenu/DelayEffectModel.h \
           editor/sidemenu/IconHelper.h \
           editor/sidemenu/OverdriveEffectModel.h \
           editor/sidemenu/UserListModel.h \
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
           network/BroadcastServer.h \
           network/Client.h \
           network/ServerSession.h
FORMS += \
    editor/ConnectDialog.ui \
    editor/EditorWindow.ui \
    editor/HostDialog.ui \
    editor/RenderDialog.ui \
    editor/ShortcutsDialog.ui \
    editor/sidemenu/SelectWoiceDialog.ui \
    editor/sidemenu/SideMenu.ui
SOURCES += main.cpp \
           editor/ConnectDialog.cpp \
           editor/ConnectionStatusLabel.cpp \
           editor/HostDialog.cpp \
           editor/RenderDialog.cpp \
           editor/Settings.cpp \
           editor/ShortcutsDialog.cpp \
           editor/sidemenu/BasicWoiceListModel.cpp \
           editor/sidemenu/DelayEffectModel.cpp \
           editor/sidemenu/IconHelper.cpp \
           editor/sidemenu/OverdriveEffectModel.cpp \
           editor/sidemenu/UserListModel.cpp \
           editor/sidemenu/WoiceListModel.cpp \
           editor/views/Animation.cpp \
           editor/audio/AudioFormat.cpp \
           editor/Clipboard.cpp \
           editor/DummySyncServer.cpp \
           editor/EditState.cpp \
           editor/EditorScrollArea.cpp \
           editor/EditorWindow.cpp \
           editor/Interval.cpp \
           editor/views/KeyboardView.cpp \
           editor/audio/NotePreview.cpp \
           editor/views/MeasureView.cpp \
           editor/views/MooClock.cpp \
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
           network/BroadcastServer.cpp \
           network/Client.cpp \
           network/ServerSession.cpp

!win32:LIBS += -logg -lvorbisfile
win32:LIBS += -L"$$PWD/../deps/lib" -L"$$PWD/deps/lib" -llibogg_static -llibvorbisfile
macx:LIBS += -L/usr/local/lib

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

RESOURCES += icons.qrc

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

  distfiles.files = $$PWD/../res/sample_instruments $$PWD/../res/sample_songs
  distfiles.path = $$PREFIX/share/ptcollab/
  INSTALLS += distfiles

  license.files = $$PWD/../LICENSE
  license.path = $$PREFIX/share/doc/ptcollab
  INSTALLS += license

  pxtone_license.files = $$PWD/pxtone/LICENSE
  pxtone_license.path = $$PREFIX/share/doc/pxtone
  INSTALLS += pxtone_license
}
