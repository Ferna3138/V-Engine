cmake_minimum_required(VERSION 3.28.0)
project(V-Engine VERSION 1.0.0)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

# Set GLFW paths based on platform
if(APPLE)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/Dependencies/lib-arm64/libglfw.3.dylib)
elseif(WIN32)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/Dependencies/lib-vc2022/glfw3.lib)
endif()

# Set TinyOBJ path
set(TINYOBJ_PATH ${CMAKE_SOURCE_DIR}/Dependencies/libs/tinyobjloader)


file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/*.cpp)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/*.hpp)

# ImGui sources
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/Dependencies/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_DOCKING)

add_executable(V-Engine ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(V-Engine 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/Src
    Dependencies
    ${TINYOBJ_PATH}
    ${IMGUI_DIR})


if(WIN32)
    target_link_libraries(V-Engine 
        ${GLFW_LIB}
        Vulkan::Vulkan)

    set(SHADER_COMPILE_SCRIPT ${CMAKE_SOURCE_DIR}/Src/Shaders/compile.bat)

    # Create custom target to compile shaders
    add_custom_target(CompileShaders
        COMMAND ${SHADER_COMPILE_SCRIPT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Src/Shaders
        COMMENT "Running compile.bat to compile shaders"
    )

    # Make sure shaders compile before V-Engine builds
    add_dependencies(V-Engine CompileShaders)

elseif(UNIX)
    if(APPLE)
        target_link_libraries(V-Engine 
            ${GLFW_LIB}
            Vulkan::Vulkan)
    else()
        target_link_libraries(V-Engine 
            glfw
            Vulkan::Vulkan)
    endif()
    set(SHADER_COMPILE_SCRIPT ${CMAKE_SOURCE_DIR}/Src/Shaders/compile.sh)

    execute_process(COMMAND chmod +x ${SHADER_COMPILE_SCRIPT})

    add_custom_target(CompileShaders
        COMMAND ${SHADER_COMPILE_SCRIPT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Src/Shaders
        COMMENT "Running compile.sh to compile shaders"
    )

    add_dependencies(V-Engine CompileShaders)

endif()


# Organize files into filters for Visual Studio
foreach(source_file ${SOURCES} ${HEADERS})
    get_filename_component(source_path ${source_file} PATH)
    file(RELATIVE_PATH source_path_rel ${CMAKE_SOURCE_DIR}/Src ${source_path})

    if(NOT source_path_rel STREQUAL "")
        string(REPLACE "/" "\\" group_path ${source_path_rel})
        source_group("${group_path}" FILES ${source_file})
    else()
        source_group("" FILES ${source_file})
    endif()
endforeach()