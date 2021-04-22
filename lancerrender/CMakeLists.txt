cmake_minimum_required (VERSION 3.1)
set(CMAKE_C_STANDARD 99)
project (lancerrender)

add_executable(
    txt2h txt2h/main.cpp
)

set(SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(LRSRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_custom_target(build-time-make-directory ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR})

macro(shader name ext)
    add_custom_command(
        OUTPUT ${SHADER_DIR}/${name}.h
        COMMAND txt2h ${name} ${LRSRC}/${name}.${ext} ${SHADER_DIR}/${name}.h
        DEPENDS ${LRSRC}/${name}.${ext} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endmacro()

shader(shader2d_vertex vert)
shader(shader2d_fragment frag)

add_library(lancerrender SHARED

gl/glad.c
src/lancerrender.c
src/lr_dynamicdraw.c
src/lr_geometry.c
src/lr_shader.c
src/lr_material.c
src/lr_blockalloc.c
src/lr_texture.c
src/lr_sort.c
src/lr_2d.c
src/lr_shaderfile.c
src/lr_dds.c
src/miniz.c
src/s3tc.c

${SHADER_DIR}/shader2d_vertex.h
${SHADER_DIR}/shader2d_fragment.h
)

# Set -lm, see if necessary
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(MLIB "m")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(MLIB "m")
else()
    set(MLIB "")
endif()

target_compile_definitions(lancerrender PRIVATE -DLR_BUILDING_DLL -D_CRT_SECURE_NO_WARNINGS)
target_include_directories(lancerrender PUBLIC include)
target_include_directories(lancerrender PRIVATE ./gl ${SHADER_DIR}) 

# SDL2 Dependency
if (DEFINED SDL2_INCLUDE_DIRS AND DEFINED SDL2_LIBRARIES)
	message(STATUS "lancerrender: using pre-defined SDL2 variables SDL2_INCLUDE_DIRS and SDL2_LIBRARIES")
	target_include_directories(lancerrender PRIVATE "$<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>")
	target_link_libraries(lancerrender PRIVATE ${SDL2_LIBRARIES})
else()
	# Only try to autodetect if both SDL2 variables aren't explicitly set
	find_package(SDL2 REQUIRED)
	if (TARGET SDL2::SDL2)
		message(STATUS "lancerrender: using TARGET SDL2::SDL2")
		target_link_libraries(lancerrender SDL2::SDL2)
	elseif (TARGET SDL2)
		message(STATUS "lancerrender: using TARGET SDL2")
		target_link_libraries(lancerrender SDL2)
	else()
		message(STATUS "lancerrender: no TARGET SDL2::SDL2, or SDL2, using variables")
		target_include_directories(lancerrender PRIVATE "$<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>")
        target_link_libraries(lancerrender ${SDL2_LIBRARIES})
	endif()
endif()

