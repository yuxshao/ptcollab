cmake_minimum_required (VERSION 3.10)

file(STRINGS "version" VERSION)

project (ptcollab
	DESCRIPTION "A collaborative piano-roll music sequencer"
	VERSION ${VERSION}
	LANGUAGES CXX
)

option (INSTALL_FLAT
	"Install everything under CMAKE_INSTALL_PREFIX instead of using Unix-style directory layout (GNUInstallDirs)"
	${WIN32}
)

if (NOT INSTALL_FLAT)
	include (GNUInstallDirs)
	set (PTCOLLAB_DEMODIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
	set (PTCOLLAB_ICONDIR "${CMAKE_INSTALL_DATADIR}/icons/hicolor")
else()
	if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		if (WIN32)
			set (PTCOLLAB_DEFAULT_INSTALLDIR "C:/${PROJECT_NAME}")
		else()
			set (PTCOLLAB_DEFAULT_INSTALLDIR "/opt/${PROJECT_NAME}")
		endif()
		set (CMAKE_INSTALL_PREFIX "${PTCOLLAB_DEFAULT_INSTALLDIR}" CACHE PATH "Install base" FORCE)
	endif()

	# Set GNUInstallDirs vars
	set (CMAKE_INSTALL_BINDIR "${CMAKE_INSTALL_PREFIX}")
	set (CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_PREFIX}")
	set (CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_PREFIX}")

	set (PTCOLLAB_DEMODIR "${CMAKE_INSTALL_DATADIR}")
	set (PTCOLLAB_ICONDIR "${CMAKE_INSTALL_DATADIR}/icons")
endif()

add_subdirectory (extern)
add_subdirectory (res)
add_subdirectory (src)

install (FILES LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")
