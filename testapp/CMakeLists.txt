cmake_minimum_required (VERSION 3.1)
project(testapp)

set(CMAKE_C_STANDARD 99)
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC OFF CACHE BOOL "" FORCE)
set(CGLM_USE_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory(cglm)


macro(shader name)
    add_custom_command(
        OUTPUT ${name}.shader
        COMMAND lrshadertool ${CMAKE_CURRENT_SOURCE_DIR}/${name}.glsl ${name}.shader
        DEPENDS ${name}.glsl
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endmacro()

shader(monkeyshader)
shader(billboard)


add_executable(testapp
    main.c
    libs.c
    monkeyshader.shader
    billboard.shader
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(MLIB "m")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(MLIB "m")
else()
    set(MLIB "")
endif()



configure_file(texture.png texture.png COPYONLY)
configure_file(monkey.png monkey.png COPYONLY)
configure_file(alphatex.png alphatex.png COPYONLY)
configure_file(alphatex.bgra5551 alphatex.bgra5551 COPYONLY)
configure_file(dxt5.dds dxt5.dds COPYONLY)

target_link_libraries(testapp PUBLIC lancerrender cglm ${MLIB})

# SDL2 Dependency
if (DEFINED SDL2_INCLUDE_DIRS AND DEFINED SDL2_LIBRARIES)
	message(STATUS "testapp: using pre-defined SDL2 variables SDL2_INCLUDE_DIRS and SDL2_LIBRARIES")
	target_include_directories(testapp PRIVATE "$<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>")
	target_link_libraries(testapp PRIVATE ${SDL2_LIBRARIES})
else()
	# Only try to autodetect if both SDL2 variables aren't explicitly set
	find_package(SDL2 REQUIRED)
	if (TARGET SDL2::SDL2)
		message(STATUS "testapp: using TARGET SDL2::SDL2")
		target_link_libraries(testapp PRIVATE SDL2::SDL2)
	elseif (TARGET SDL2)
		message(STATUS "testapp: using TARGET SDL2")
		target_link_libraries(testapp PRIVATE SDL2)
	else()
		message(STATUS "testapp: no TARGET SDL2::SDL2, or SDL2, using variables")
		target_include_directories(testapp PRIVATE "$<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>")
        target_link_libraries(testapp PRIVATE ${SDL2_LIBRARIES})
	endif()
endif()
