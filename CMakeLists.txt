cmake_minimum_required(VERSION 3.24)

# Discard stale compiler paths from a previous configure. This allows CMake to
# select the currently installed toolchain from CC/CXX or PATH.
if(DEFINED CMAKE_C_COMPILER AND NOT EXISTS "${CMAKE_C_COMPILER}")
    unset(CMAKE_C_COMPILER CACHE)
endif()
if(DEFINED CMAKE_CXX_COMPILER AND NOT EXISTS "${CMAKE_CXX_COMPILER}")
    unset(CMAKE_CXX_COMPILER CACHE)
endif()

project(MedievalPort LANGUAGES C CXX)

# Emit compile_commands.json so editor tooling resolves the real per-file flags.
# The Visual Studio generators do not support this, so only request it elsewhere.
if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

include(CTest)
find_package(Threads REQUIRED)

option(MEDIEVALPORT_BUILD_APP "Build the OpenGL editor" ON)
option(MEDIEVALPORT_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(MEDIEVALPORT_ENABLE_GL_TESTS "Run tests requiring an OpenGL 4.5 desktop driver" OFF)

include(cmake/Dependencies.cmake)

function(medievalport_project_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(${target} PRIVATE GLFW_INCLUDE_NONE)
    if(WIN32)
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
        if(MEDIEVALPORT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wold-style-cast)
        if(MEDIEVALPORT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# The importer, scene math, camera and persistence are testable without a GL context.
add_library(MedievalPortCpu STATIC
    src/core/Camera.cpp
    src/core/SceneMath.cpp
    src/io/ConfigManager.cpp
    src/io/ImageData.cpp
    src/io/LoadData.cpp
    src/io/ModelLoaderThread.cpp
    src/utils/Log.cpp
)
target_include_directories(MedievalPortCpu PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(MedievalPortCpu PUBLIC Threads::Threads MedievalPortCpuDependencies)
medievalport_project_target(MedievalPortCpu)

if(BUILD_TESTING)
    add_executable(AssetTests tests/AssetTests.cpp)
    target_link_libraries(AssetTests PRIVATE MedievalPortCpu)
    medievalport_project_target(AssetTests)
    add_test(NAME Assets COMMAND AssetTests)
    set_tests_properties(Assets PROPERTIES TIMEOUT 30)

    add_executable(SceneTests tests/SceneTests.cpp)
    target_link_libraries(SceneTests PRIVATE MedievalPortCpu MedievalPortGlad)
    medievalport_project_target(SceneTests)
    add_test(NAME SceneMath COMMAND SceneTests)
endif()

if(MEDIEVALPORT_BUILD_APP)
    add_library(MedievalPortEngine STATIC
        src/core/Application.cpp
        src/core/Control.cpp
        src/editor/Editor.cpp
        src/graphics/GlObject.cpp
        src/graphics/GraphicsContext.cpp
        src/graphics/IndexBuffer.cpp
        src/graphics/Renderer.cpp
        src/graphics/Shader.cpp
        src/graphics/SkyboxSystem.cpp
        src/graphics/TextureArray.cpp
        src/graphics/TextureResidency.cpp
        src/graphics/VertexArray.cpp
        src/graphics/VertexBuffer.cpp
        src/io/FileParser.cpp
        src/ui/ImguiInterface.cpp
        src/utils/DebugMessage.cpp
        src/window/WindowSystem.cpp
    )
    target_link_libraries(MedievalPortEngine PUBLIC MedievalPortCpu MedievalPortGlad
        PRIVATE MedievalPortImgui glfw OpenGL::GL)
    medievalport_project_target(MedievalPortEngine)

    add_executable(MedievalPort src/core/main.cpp)
    target_link_libraries(MedievalPort PRIVATE MedievalPortEngine)
    if(WIN32)
        target_link_libraries(MedievalPort PRIVATE shell32)
    endif()
    medievalport_project_target(MedievalPort)
    add_custom_target(MedievalPortAssets
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res/shaders"
                "$<TARGET_FILE_DIR:MedievalPort>/res/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/res/textures"
                "$<TARGET_FILE_DIR:MedievalPort>/res/textures"
        VERBATIM
    )
    add_dependencies(MedievalPort MedievalPortAssets)

    if(BUILD_TESTING AND MEDIEVALPORT_ENABLE_GL_TESTS)
        add_executable(GraphicsTests tests/GraphicsTests.cpp)
        target_link_libraries(GraphicsTests PRIVATE MedievalPortEngine MedievalPortGlad glfw)
        medievalport_project_target(GraphicsTests)
        add_test(NAME Graphics COMMAND GraphicsTests "${PROJECT_SOURCE_DIR}/res")
        set_tests_properties(Graphics PROPERTIES TIMEOUT 60 LABELS "OpenGL")
    endif()
endif()
