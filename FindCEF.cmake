include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_OBS QUIET cef)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_lib_suffix 64)
else()
	set(_lib_suffix 32)
endif()

if(NOT DEFINED CEFBuildType)
	set(CEFBuildType "Release")
endif()

find_path(CEF_INCLUDE_DIR "include/cef_version.h"
	HINTS
		ENV CEFPath
		ENV CEFPath${_lib_suffix}
		${CEF_ROOT_DIR}
		${CEFPath}
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
	)

find_library(CEF_LIBRARY
	NAMES cef libcef cef.lib libcef.o "Chromium Embedded Framework"
	PATHS
		/usr/include /usr/local/include /opt/local/include /sw/include
		ENV CEFPath
		ENV CEFPath${_lib_suffix}
		${CEF_ROOT_DIR}
		${CEFPath}
	PATH_SUFFIXES
		${CEFBuildType}
	)

find_library(CEFWRAPPER_LIBRARY
	NAMES cef_dll_wrapper libcef_dll_wrapper
	PATHS
		${CEF_ROOT_DIR}
		${CEF_INCLUDE_DIR}
	PATH_SUFFIXES
		build/libcef_dll/${CEFBuildType}
		build/libcef_dll
		build${_lib_suffix}/libcef_dll/${CEFBuildType}
		build${_lib_suffix}/libcef_dll
		../build/libcef_dll/${CEFBuildType}
		../build/libcef_dll
		../build${_lib_suffix}/libcef_dll/${CEFBuildType}
		../build${_lib_suffix}/libcef_dll
	)

if (NOT CEF_LIBRARY)
	message(FATAL_ERROR "Could not find the CEF shared library" )
endif (NOT CEF_LIBRARY)

if (NOT CEFWRAPPER_LIBRARY)
	message(FATAL_ERROR "Could not find the CEF wrapper library" )
endif (NOT CEFWRAPPER_LIBRARY)

set(CEF_LIBRARIES
	${CEF_LIBRARY}
	${CEFWRAPPER_LIBRARY})

find_package_handle_standard_args(CEF DEFAULT_MSG CEF_LIBRARY 
	CEFWRAPPER_LIBRARY CEF_INCLUDE_DIR)

mark_as_advanced(CEF_LIBRARY CEF_WRAPPER_LIBRARY CEF_LIBRARIES 
	CEF_INCLUDE_DIR)

if(NOT DEFINED CEF_ROOT_DIR)
	set(CEF_ROOT_DIR "${CEF_INCLUDE_DIR}")
endif()
