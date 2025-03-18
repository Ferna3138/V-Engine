cmake_minimum_required(VERSION 3.28.0)
project(V-Engine VERSION 1.0.0)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

# Set GLFW paths based on platform
if(APPLE)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/dependencies/lib-arm64/libglfw.3.dylib)
elseif(WIN32)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/dependencies/lib-vc2022/glfw3.lib)
endif()

add_executable(V-Engine
    src/backend/glfw_backend.cpp
    src/backend/glfw_backend.h
    
    src/controller/app.cpp
    src/controller/app.h

    src/logging/logger.cpp
    src/logging/logger.h

    src/renderer/device.cpp
    src/renderer/device.h
    src/renderer/instance.cpp
    src/renderer/instance.h
    src/renderer/renderer.cpp
    src/renderer/renderer.h

    src/main.cpp
)

target_include_directories(V-Engine 
    PRIVATE
    dependencies
)

target_link_libraries(V-Engine
    ${GLFW_LIB}
    Vulkan::Vulkan
)

# Organize source files into filters for Visual Studio
foreach(source IN ITEMS 
    src/backend/glfw_backend.cpp
    src/backend/glfw_backend.h

    src/controller/app.cpp
    src/controller/app.h

    src/logging/logger.cpp
    src/logging/logger.h

    src/renderer/device.cpp
    src/renderer/device.h
    src/renderer/instance.cpp
    src/renderer/instance.h
    src/renderer/renderer.cpp
    src/renderer/renderer.h

    src/main.cpp
)

    get_filename_component(source_path ${source} PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}/src" "" filter_path ${source_path})
    string(REPLACE "/" "\\" filter_path ${filter_path})
    source_group(${filter_path} FILES ${source})
endforeach()