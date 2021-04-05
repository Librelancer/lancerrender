cmake_minimum_required (VERSION 3.1)
project(testapp)

set(CMAKE_C_STANDARD 99)
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC OFF CACHE BOOL "" FORCE)
set(CGLM_USE_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory(cglm)

add_custom_command(
  OUTPUT monkeyshader.shader
  COMMAND lrshadertool ${CMAKE_CURRENT_SOURCE_DIR}/monkeyshader.glsl monkeyshader.shader
  DEPENDS monkeyshader.glsl
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

shader(monkeyshader glsl)

add_executable(testapp
    main.c
    libs.c
    monkeyshader.shader
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

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

target_link_libraries(testapp lancerrender ${SDL2_LIBRARIES} cglm ${MLIB})
target_include_directories(testapp PUBLIC ${SDL2_INCLUDE_DIRS})
target_compile_options(testapp PUBLIC ${SDL2_CFLAGS_OTHER})
