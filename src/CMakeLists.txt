# C++14 required
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (CMAKE_AUTOUIC ON)

# Identify Qt version we're using
#message (STATUS "Attempting to identify Qt version.")
#find_package (Qt6 COMPONENTS Core)
#if (Qt6_FOUND)
#	set (QT_VERSION 6)
#else()
	find_package (Qt5 COMPONENTS Core)
	if (Qt5_FOUND)
		set (QT_VERSION 5)
	else()
#		message (FATAL_ERROR "Unable to locate either Qt5 or Qt6!")
		message (FATAL_ERROR "Unable to locate Qt5!")
	endif()
#endif()
message(STATUS "Found Qt${QT_VERSION}")

set (QT_COMPONENTS Core Gui Multimedia Widgets)
if (QT_VERSION EQUAL 6)
	# TODO check if needed when adding Qt6 support
	# https://www.qt.io/blog/porting-from-qt-5-to-qt-6-using-qt5compat-library
	# list (APPEND QT_COMPONENTS Core5Compat)
endif (QT_VERSION EQUAL 6)
find_package ("Qt${QT_VERSION}" COMPONENTS ${QT_COMPONENTS} REQUIRED LinguistTools)

FILE(GLOB TS_FILES translations/*.ts)
qt5_add_translation(qmFiles ${TS_FILES})

# Unfortunately in Qt5 we have to generate the qrc ourselves.
set(TRANSLATIONS_FILE ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
file(WRITE ${TRANSLATIONS_FILE} "<?xml version=\"1.0\" encoding=\"UTF-8\"?><RCC><qresource prefix='/i18n'>")
foreach(FILE_PATH ${qmFiles})
    get_filename_component(FILE_NAME ${FILE_PATH} NAME)
    file(APPEND ${TRANSLATIONS_FILE} "<file alias=\"${FILE_NAME}\">${FILE_PATH}</file>")
endforeach()
file (APPEND ${TRANSLATIONS_FILE} "</qresource></RCC>")

# C/C++ & qrc Qt Resource files
set (PTCOLLAB_SOURCES
	main.cpp
	editor/ConnectDialog.cpp
	editor/ConnectionStatusLabel.cpp
	editor/CopyOptionsDialog.cpp
	editor/HostDialog.cpp
	editor/InputEvent.cpp
	editor/KeySpinBox.cpp
	editor/MidiWrapper.cpp
	editor/NewWoiceDialog.cpp
	editor/RenderDialog.cpp
	editor/Settings.cpp
	editor/SettingsDialog.cpp
	editor/ShortcutsDialog.cpp
	editor/sidemenu/SongTitleDialog.cpp
	editor/StyleEditor.cpp
	editor/WelcomeDialog.cpp
	editor/audio/VolumeMeter.cpp
	editor/sidemenu/BasicWoiceListModel.cpp
	editor/sidemenu/DelayEffectModel.cpp
	editor/sidemenu/IconHelper.cpp
	editor/sidemenu/OverdriveEffectModel.cpp
	editor/sidemenu/TableView.cpp
	editor/sidemenu/UserListModel.cpp
	editor/sidemenu/VolumeMeterWidget.cpp
	editor/sidemenu/WoiceListModel.cpp
	editor/views/Animation.cpp
	editor/audio/AudioFormat.cpp
	editor/Clipboard.cpp
	editor/ComboOptions.cpp
	editor/DummySyncServer.cpp
	editor/EditState.cpp
	editor/EditorScrollArea.cpp
	editor/EditorWindow.cpp
	editor/Interval.cpp
	editor/views/KeyboardView.cpp
	editor/audio/NotePreview.cpp
	editor/views/MeasureView.cpp
	editor/views/MooClock.cpp
        editor/views/NoteBrush.cpp
	editor/views/ParamView.cpp
	editor/PxtoneClient.cpp
	editor/PxtoneController.cpp
	editor/audio/PxtoneIODevice.cpp
	editor/sidemenu/PxtoneSideMenu.cpp
	editor/audio/PxtoneUnitIODevice.cpp
	editor/sidemenu/SelectWoiceDialog.cpp
	editor/sidemenu/SideMenu.cpp
	editor/sidemenu/UnitListModel.cpp
	editor/views/ViewHelper.cpp
	network/AbstractServerSession.cpp
	network/Client.cpp
	network/LocalServerSession.cpp
	network/ServerSession.cpp
	protocol/Data.cpp
	protocol/Hello.cpp
	protocol/NoIdMap.cpp
	protocol/PxtoneEditAction.cpp
	protocol/RemoteAction.cpp
	pxtone/pxtnDelay.cpp
	pxtone/pxtnDescriptor.cpp
	pxtone/pxtnError.cpp
	pxtone/pxtnEvelist.cpp
	pxtone/pxtnMaster.cpp
	pxtone/pxtnMem.cpp
	pxtone/pxtnOverDrive.cpp
	pxtone/pxtnPulse_Frequency.cpp
	pxtone/pxtnPulse_Noise.cpp
	pxtone/pxtnPulse_NoiseBuilder.cpp
	pxtone/pxtnPulse_Oggv.cpp
	pxtone/pxtnPulse_Oscillator.cpp
	pxtone/pxtnPulse_PCM.cpp
	pxtone/pxtnService.cpp
	pxtone/pxtnService_moo.cpp
	pxtone/pxtnText.cpp
	pxtone/pxtnUnit.cpp
	pxtone/pxtnWoice.cpp
	pxtone/pxtnWoice_io.cpp
	pxtone/pxtnWoicePTV.cpp
	pxtone/pxtoneNoise.cpp
	network/BroadcastServer.cpp
        icons.qrc
        styles.qrc
        ${qmFiles}
        ${TRANSLATIONS_FILE}
)

# UI forms
set (PTCOLLAB_FORMS
	editor/ConnectDialog.ui
	editor/CopyOptionsDialog.ui
	editor/EditorWindow.ui
	editor/HostDialog.ui
	editor/NewWoiceDialog.ui
	editor/RenderDialog.ui
	editor/SettingsDialog.ui
	editor/ShortcutsDialog.ui
	editor/sidemenu/SongTitleDialog.ui
	editor/WelcomeDialog.ui
	editor/sidemenu/SelectWoiceDialog.ui
	editor/sidemenu/SideMenu.ui
)

set (PTCOLLAB_INCLUDEPATHS
	"${CMAKE_CURRENT_SOURCE_DIR}"
	editor
	network
	protocol
	pxtone
)

if (APPLE)
	set (MACOSX_BUNDLE_ICON_FILE icon.icns)
	set (MACOSX_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/${MACOSX_BUNDLE_ICON_FILE}")
	set_source_files_properties (${MACOSX_ICNS}
		PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
	)
	list (APPEND PTCOLLAB_SOURCES ${MACOSX_ICNS})

        list (APPEND PTCOLLAB_SOURCES editor/MacOsStyleEditor.mm)
        # Title bar customization :)
endif()

add_executable (${PROJECT_NAME} WIN32 MACOSX_BUNDLE
        ${PTCOLLAB_SOURCES}
        ${PTCOLLAB_FORMS}
)

if(APPLE) # ugh
    find_library(COCOA Cocoa REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA})
    # Necessary Cocoa libraries for title bar customization
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

target_include_directories (${PROJECT_NAME} PRIVATE ${PTCOLLAB_INCLUDEPATHS})

# Filter out compile-time dependencies which we can't / don't need to link against
set(QT_LIBRARIES ${QT_COMPONENTS})
list(REMOVE_ITEM QT_LIBRARIES
	LinguistTools
)
list(TRANSFORM QT_LIBRARIES PREPEND "Qt${QT_VERSION}::")
target_link_libraries (${PROJECT_NAME} PUBLIC ${QT_LIBRARIES})

target_compile_definitions (${PROJECT_NAME}
	PRIVATE
	QT_DEPRECATED_WARNINGS
	PTCOLLAB_VERSION=${PROJECT_VERSION}
	${DEPENDENCIES_DEFINES}
)

target_include_directories (${PROJECT_NAME}
	SYSTEM PRIVATE
	${DEPENDENCIES_INCLUDE_DIRS}
)
target_compile_options (${PROJECT_NAME}
	PRIVATE
	${DEPENDENCIES_COMPILE_OPTIONS}
)
if ("${CMAKE_VERSION}" VERSION_LESS "3.13")
	message (WARNING
		"CMake version is <3.13, using old pkg-config LDFLAGS. "
		"You may encounter linking problems with these!"
	)
	target_link_libraries (${PROJECT_NAME}
		PRIVATE
		${DEPENDENCIES_LDFLAGS_LEGACY}
	)
else()
	target_link_libraries (${PROJECT_NAME}
		PRIVATE
		${DEPENDENCIES_LIBRARIES}
	)
	target_link_directories (${PROJECT_NAME}
		PRIVATE
		${DEPENDENCIES_LINK_DIRS}
	)
	target_link_options (${PROJECT_NAME}
		PRIVATE
		${DEPENDENCIES_LINK_OPTIONS}
	)
endif()

install (TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_BINDIR}")

install (FILES pxtone/LICENSE-pxtone DESTINATION "${CMAKE_INSTALL_DOCDIR}")
