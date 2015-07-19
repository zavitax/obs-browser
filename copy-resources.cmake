if(NOT EXISTS ${SRC_DIR} OR NOT EXISTS ${DST_DIR})
	return()
endif()

file(GLOB TARGET_FILES "${SRC_DIR}/*.pak" "${SRC_DIR}/*.dat")
file(COPY ${TARGET_FILES} DESTINATION ${DST_DIR})
