include_guard(GLOBAL)
include(FetchContent)

# Keep dependency URLs and checksums here. FetchContent caches the sources in
# the build tree's _deps directory; no third-party sources belong in Git.
# SOURCE_SUBDIR skips upstream build scripts for headers and custom-built targets.
FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
    URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
    TLS_VERIFY TRUE
    SOURCE_SUBDIR _unused
)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
    URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406
    TLS_VERIFY TRUE
    SOURCE_SUBDIR _unused
)
# stb_image 2.30 and stb_image_resize2 2.14; stb does not publish version tags.
FetchContent_Declare(stb
    URL https://github.com/nothings/stb/archive/8cfb1605c02aee9fb6eb5d8ea559017745bd9a16.tar.gz
    URL_HASH SHA256=0698557ac25a91e19f41365ef253c8ac8a3ace3d1173c75d33c616d530217659
    TLS_VERIFY TRUE
    SOURCE_SUBDIR _unused
)
# Post-rc13 snapshot retaining the existing comment and material parsing fixes.
FetchContent_Declare(tinyobjloader
    URL https://github.com/tinyobjloader/tinyobjloader/archive/3bb554cf74428d7db13418b4aca1b9752a1d2be8.tar.gz
    URL_HASH SHA256=81eaf7bfff527904d1d1320bd7a6b1577379458b57ecd2f496c52396b5034292
    TLS_VERIFY TRUE
    SOURCE_SUBDIR _unused
)
FetchContent_MakeAvailable(glm nlohmann_json stb tinyobjloader)

add_library(MedievalPortCpuDependencies INTERFACE)
target_include_directories(MedievalPortCpuDependencies SYSTEM INTERFACE
    "${glm_SOURCE_DIR}"
    "${nlohmann_json_SOURCE_DIR}/single_include"
    "${stb_SOURCE_DIR}"
    "${tinyobjloader_SOURCE_DIR}"
)

if(MEDIEVALPORT_BUILD_APP OR BUILD_TESTING)
    # SceneTests also checks vertex layouts, which use GLAD's GL type constants.
    find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)
    FetchContent_Declare(glad
        URL https://github.com/Dav1dde/glad/archive/refs/tags/v0.1.36.tar.gz
        URL_HASH SHA256=8470ed1b0e9fbe88e10c34770505c8a1dc8ccb78cadcf673331aaf5224f963d2
        TLS_VERIFY TRUE
        SOURCE_SUBDIR _unused
    )
    FetchContent_MakeAvailable(glad)

    # Match the original GLAD 1 loader API. --reproducible uses the pinned
    # Khronos specification and headers, so generation needs no extra downloads.
    add_custom_command(
        OUTPUT "${glad_BINARY_DIR}/src/glad.c"
               "${glad_BINARY_DIR}/include/glad/glad.h"
               "${glad_BINARY_DIR}/include/KHR/khrplatform.h"
        COMMAND "${Python3_EXECUTABLE}" -X utf8 -m glad
                --profile=compatibility --api=gl=4.6 --generator=c --spec=gl
                --extensions= --reproducible "--out-path=${glad_BINARY_DIR}"
        WORKING_DIRECTORY "${glad_SOURCE_DIR}"
        DEPENDS "${glad_SOURCE_DIR}/glad/__main__.py"
                "${glad_SOURCE_DIR}/glad/files/gl.xml"
                "${CMAKE_CURRENT_LIST_FILE}"
        COMMENT "Generating the GLAD OpenGL loader"
        VERBATIM
    )
    add_library(MedievalPortGlad STATIC "${glad_BINARY_DIR}/src/glad.c")
    target_include_directories(MedievalPortGlad SYSTEM PUBLIC "${glad_BINARY_DIR}/include")
    target_link_libraries(MedievalPortGlad PRIVATE ${CMAKE_DL_LIBS})
endif()

if(MEDIEVALPORT_BUILD_APP)
    find_package(OpenGL REQUIRED)
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
        URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
        TLS_VERIFY TRUE
    )
    FetchContent_Declare(imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.6.tar.gz
        URL_HASH SHA256=5b17c01f69545bde732b14936d89ce0f508adb83e8b56fa82448371845172bc3
        TLS_VERIFY TRUE
        SOURCE_SUBDIR _unused
    )

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "Build GLFW into the executable" FORCE)
    if(UNIX AND NOT APPLE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW's optional Wayland backend")
    endif()
    FetchContent_MakeAvailable(glfw imgui)

    # ImGui's OpenGL backend keeps its embedded loader separate from GLAD.
    add_library(MedievalPortImgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )
    target_include_directories(MedievalPortImgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")
    target_compile_features(MedievalPortImgui PRIVATE cxx_std_20)
    target_compile_definitions(MedievalPortImgui PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(MedievalPortImgui PRIVATE glfw ${CMAKE_DL_LIBS})
endif()
