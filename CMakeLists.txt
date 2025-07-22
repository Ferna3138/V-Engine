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

# Define source files
set(SOURCES
    Src/Shaders/compile.bat
    Src/Shaders/simple_shader.frag
    Src/Shaders/simple_shader.vert
    
    Src/Camera.cpp
    Src/Camera.hpp
    Src/Device.cpp
    Src/Device.hpp
    Src/First_App.cpp
    Src/First_App.hpp
    Src/Game_Object.hpp
    Src/Keyboard_Movement_Controller.cpp
    Src/Keyboard_Movement_Controller.hpp
    Src/main.cpp
    Src/Model.cpp
    Src/Model.hpp
    Src/Pipeline.cpp
    Src/Pipeline.hpp
    Src/Renderer.cpp
    Src/Renderer.hpp
    Src/Simple_Render_System.cpp
    Src/Simple_Render_System.hpp
    Src/Swap_Chain.cpp
    Src/Swap_Chain.hpp
    Src/Window.cpp
    Src/Window.hpp
)

add_executable(V-Engine ${SOURCES})

# Include directories
target_include_directories(V-Engine 
    PRIVATE
    Dependencies)


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
    target_link_libraries(V-Engine 
        glfw
        Vulkan::Vulkan)

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
foreach(source ${SOURCES})
    get_filename_component(source_path ${source} PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}/src" "" filter_path ${source_path})
    string(REPLACE "/" "\\" filter_path ${filter_path})
    source_group(${filter_path} FILES ${source})
endforeach()