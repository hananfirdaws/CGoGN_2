project(cgogn_geometry
	LANGUAGES CXX
	)

set(HEADER_FILES
	dll.h
	types/plane_3d.h
	types/geometry_traits.h
	functions/orientation.h
	types/vec.h
	types/eigen.h
	)

set(SOURCE_FILES
	types/plane_3d.cpp
	types/vec.cpp
	)

add_library(${PROJECT_NAME} SHARED ${HEADER_FILES} ${SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "_d")

target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${EIGEN3_INCLUDE_DIR}>
	$<BUILD_INTERFACE:${CGOGN_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include/cgogn/geometry>
	)

target_link_libraries(${PROJECT_NAME} cgogn_core)

install(DIRECTORY .
	DESTINATION include/cgogn/geometry
	FILES_MATCHING PATTERN "*.h"
	)

install(TARGETS ${PROJECT_NAME}
	EXPORT ${PROJECT_NAME}Targets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	)

if(CGOGN_BUILD_TESTS)
	add_subdirectory(tests)
endif()