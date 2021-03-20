cmake_minimum_required(VERSION 3.1)
project(lrshadertool)

add_executable (lrshadertool 
    main.cpp
    gl2spv.cpp
    shfile.cpp
    platform.cpp
    miniz.c
)

target_link_libraries(lrshadertool
    glslang 
    SPIRV 
    spirv-cross-core 
    spirv-cross-glsl 
    spirv-cross-util 
)