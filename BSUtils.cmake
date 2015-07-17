# Install bundles into plugin directory
macro(install_external_plugin_bundle target target-bundle-path)
	set_target_properties(${target} PROPERTIES
		PREFIX "")

	install(TARGETS ${target}
		BUNDLE DESTINATION "${EXTERNAL_PLUGIN_OUTPUT_DIR}/bin")

	get_filename_component(target-bundle-name ${target-bundle-path} NAME)

	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"$<TARGET_FILE_DIR:${target-bundle-path}"
			"${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/bin/${target-bundle-name}"
		VERBATIM)

endmacro()

macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
	set_property(TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY}
		${XCODE_VALUE})
endmacro (set_xcode_property)
