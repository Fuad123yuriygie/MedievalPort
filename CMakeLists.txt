cmake_minimum_required(VERSION 3.10)

# Project name
project(MedievalPort)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Defines
add_definitions(-DFUAD_LOCAL)
add_definitions(-DDEBUG)
add_definitions(-DTINYOBJLOADER_IMPLEMENTATION)

# Add debug flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g3 -O0") # Enable debug symbols
endif()

# Source files (recursively collect sources/headers)
file(GLOB_RECURSE SOURCES
    "${PROJECT_SOURCE_DIR}/src/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
    "${PROJECT_SOURCE_DIR}/src/*.c"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/externals/src/*.c"
    "${PROJECT_SOURCE_DIR}/externals/src/*.cpp"
)

# Specify the directory containing header files
include_directories(${PROJECT_SOURCE_DIR}/externals/include)
include_directories(${PROJECT_SOURCE_DIR}/externals/include/imgui)

# Add all immediate subdirectories of src to include paths so existing
# includes that reference headers by basename continue to work.
file(GLOB SRC_SUBDIRS RELATIVE ${PROJECT_SOURCE_DIR} "${PROJECT_SOURCE_DIR}/src/*")
foreach(subdir ${SRC_SUBDIRS})
    if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/${subdir}")
        include_directories("${PROJECT_SOURCE_DIR}/${subdir}")
    endif()
endforeach()

# Specify the directory containing library files
link_directories(${PROJECT_SOURCE_DIR}/externals/lib)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link glfw3 and opengl32 libraries
target_link_libraries(${PROJECT_NAME} glfw3 opengl32)