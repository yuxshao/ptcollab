if (UNIX AND NOT APPLE)
	install (FILES ptcollab.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
	# Directory layout doesn't match what the standard needs
	file (GLOB icons "app_icons/*/${PROJECT_NAME}.*")
	foreach (icon ${icons})
		get_filename_component (icon_parent_path ${icon} DIRECTORY)
		get_filename_component (icon_parent_dir ${icon_parent_path} NAME)
		install (FILES ${icon} DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${icon_parent_dir}/apps")
	endforeach ()
endif ()

install (DIRECTORY sample_instruments sample_songs DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")
