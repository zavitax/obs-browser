# Install bundles into plugin directory
macro(install_external_plugin_bundle target target-bundle-name)
	set_target_properties(${target} PROPERTIES
		PREFIX "")

	install(DIRECTORY ${target}/
		DESTINATION "${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/bin"
		USE_SOURCE_PERMISSIONS)
	add_custom_command(TARGET ${target} POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${target-bundle-name}"
			"${EXTERNAL_PLUGIN_OUTPUT_DIR}/$<CONFIGURATION>/${target}/bin/${target-bundle-name}"
		VERBATIM)

endmacro()

macro (set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
	set_property(TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY}
		${XCODE_VALUE})
endmacro (set_xcode_property)
