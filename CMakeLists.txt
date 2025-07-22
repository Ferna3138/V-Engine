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
    
    Src/Components/Game_Object.hpp
    Src/Components/Model.cpp
    Src/Components/Model.hpp

    Src/Renderer/Device.cpp
    Src/Renderer/Device.hpp
    Src/Renderer/Pipeline.cpp
    Src/Renderer/Pipeline.hpp
    Src/Renderer/Renderer.cpp
    Src/Renderer/Renderer.hpp
    Src/Renderer/Swap_Chain.cpp
    Src/Renderer/Swap_Chain.hpp
    Src/Renderer/Window.cpp
    Src/Renderer/Window.hpp
    
    Src/Systems/Camera.cpp
    Src/Systems/Camera.hpp
    Src/Systems/Keyboard_Movement_Controller.cpp
    Src/Systems/Keyboard_Movement_Controller.hpp

    Src/First_App.cpp
    Src/First_App.hpp
    Src/main.cpp
    Src/Simple_Render_System.cpp
    Src/Simple_Render_System.hpp
    
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