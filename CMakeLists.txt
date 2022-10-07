cmake_minimum_required (VERSION 3.10)

file(STRINGS "version" VERSION)

project (ptcollab
	DESCRIPTION "A collaborative piano-roll music sequencer"
	VERSION ${VERSION}
	LANGUAGES CXX
)

include (GNUInstallDirs)

#add_subdirectory (submodules)
add_subdirectory (res)
add_subdirectory (src)

install (FILES LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
