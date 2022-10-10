if (UNIX AND NOT APPLE)
	install (FILES ptcollab.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

	# Directory layout doesn't match what the standard needs
	# In repo: app_icons/<resolution>/ptcollab.<format>
	# Required: <icondir>/<resolution>/apps/ptcollab.<format>
	# We find all icon files, extract the <resolution> from their parent directory's name
	# and construct the correct installation dirs from there
	file (GLOB icons "app_icons/*/${PROJECT_NAME}.*")
	foreach (icon ${icons})
		get_filename_component (icon_parent_path ${icon} DIRECTORY)
		get_filename_component (icon_resolution ${icon_parent_path} NAME)
		install (FILES ${icon} DESTINATION "${PTCOLLAB_ICONDIR}/${icon_resolution}/apps")
	endforeach ()
endif ()

install (DIRECTORY sample_instruments sample_songs DESTINATION "${PTCOLLAB_DEMODIR}")
