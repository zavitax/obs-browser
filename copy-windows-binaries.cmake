if(NOT EXISTS ${SRC_DIR} OR NOT EXISTS ${DST_DIR})
	return()
endif()

file(GLOB TARGET_FILES "${SRC_DIR}/*")
file(GLOB EXCLUDE_FILES "${SRC_DIR}/d3d*" "${SRC_DIR}/*.lib" "${SRC_DIR}/libEGL*.dll" "${SRC_DIR}/libGL*.dll")

foreach(exclude_file ${EXCLUDE_FILES})
	list(REMOVE_ITEM TARGET_FILES ${exclude_file})
endforeach()

file(COPY ${TARGET_FILES} DESTINATION ${DST_DIR})
