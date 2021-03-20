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
src/lr_geometry.c
src/lr_shader.c
src/lr_material.c
src/lr_blockalloc.c
src/lr_texture.c
src/lr_sort.c
src/lr_2d.c
src/lr_shaderfile.c
src/miniz.c

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


find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
target_compile_definitions(lancerrender PRIVATE -DLR_BUILDING_DLL -D_CRT_SECURE_NO_WARNINGS)
target_include_directories(lancerrender PUBLIC include)
target_link_libraries(lancerrender ${SDL2_LIBRARIES} ${MLIB})
target_include_directories(lancerrender PRIVATE ${SDL2_INCLUDE_DIRS} ./gl ${SHADER_DIR}) 
