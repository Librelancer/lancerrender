cmake_minimum_required(VERSION 3.1)
project(lren)
option (LR_BUILD_TESTAPP "Build the test app" ON)
option (LR_BUILD_SHADERTOOL "Build the lrshadertool shader processor" ON)
# CMP0077 is only available in CMake 3.12 and above
# This should get rid of the PythonInterp dependency
# which ends up being problematic on Ubuntu 20.04
# Disable unused spirv items
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
# Disable unused glslang items
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
# external
if(LR_BUILD_SHADERTOOL)
    add_subdirectory(SPIRV-Cross spirvcross)
    add_subdirectory(glslang glslang)
endif()
# our code
add_subdirectory(lancerrender lancerrender)
if(LR_BUILD_SHADERTOOL)
    add_subdirectory(lrshadertool lrshadertool)
endif()
if(LR_BUILD_TESTAPP)
    add_subdirectory(testapp)
endif()
