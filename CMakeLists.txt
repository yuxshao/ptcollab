cmake_minimum_required (VERSION 3.10)

file(STRINGS "version" VERSION)

project (ptcollab
	DESCRIPTION "A collaborative piano-roll music sequencer"
	VERSION ${VERSION}
	LANGUAGES CXX
)

#add_subdirectory (submodules)
add_subdirectory (src)
