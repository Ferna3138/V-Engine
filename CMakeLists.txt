cmake_minimum_required(VERSION 3.28.0)
project(V-Engine VERSION 1.0.0)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

# Set GLFW paths based on platform
if(APPLE)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/dependencies/lib-arm64/libglfw.3.dylib)
    set(SHADERC_LIB ${CMAKE_SOURCE_DIR}/dependencies/lib/libshaderc_shared.1.dylib)
elseif(WIN32)
    set(GLFW_LIB ${CMAKE_SOURCE_DIR}/dependencies/lib-vc2022/glfw3.lib)
    set(SHADERC_LIB ${CMAKE_SOURCE_DIR}/dependencies/lib/shaderc_combined.lib)
endif()

add_executable(V-Engine
    shaders/compile.bat
    shaders/fragment.frag
    shaders/vertex.vert

    src/backend/file.cpp
    src/backend/file.h
    src/backend/glfw_backend.cpp
    src/backend/glfw_backend.h
    src/backend/vma_usage.cpp
    
    src/controller/app.cpp
    src/controller/app.h

    src/factories/mesh_factory.cpp
    src/factories/mesh_factory.h

    src/logging/logger.cpp
    src/logging/logger.h

    src/renderer/buffer.cpp
    src/renderer/buffer.h
    src/renderer/command.cpp
    src/renderer/command.h
    src/renderer/device.cpp
    src/renderer/device.h
    src/renderer/frame.cpp
    src/renderer/frame.h
    src/renderer/image.cpp
    src/renderer/image.h
    src/renderer/instance.cpp
    src/renderer/instance.h
    src/renderer/renderer.cpp
    src/renderer/renderer.h
    src/renderer/shader.cpp
    src/renderer/shader.h
    src/renderer/swapchain.cpp
    src/renderer/swapchain.h
    src/renderer/synchronisation.cpp
    src/renderer/synchronisation.h

    src/main.cpp
)


target_include_directories(V-Engine 
    PRIVATE
    dependencies 
)

if(UNIX AND NOT APPLE)
    target_link_libraries(V-Engine
        glfw
        Vulkan::Vulkan
    )


    add_custom_target(CompileShaders ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling shaders..."
        COMMAND bash ${CMAKE_SOURCE_DIR}/shaders/compile.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/shaders
        COMMENT "Running compile.sh to compile shaders"
    )

    add_dependencies(V-Engine CompileShaders)


else()
    target_link_libraries(V-Engine
        ${GLFW_LIB}
        ${SHADERC_LIB}
    )
endif()



# Organize source files into filters for Visual Studio
foreach(source IN ITEMS 
    shaders/compile.bat
    shaders/fragment.frag
    shaders/vertex.vert

    src/backend/file.cpp
    src/backend/file.h
    src/backend/glfw_backend.cpp
    src/backend/glfw_backend.h
    src/backend/vma_usage.cpp

    src/controller/app.cpp
    src/controller/app.h

    src/factories/mesh_factory.cpp
    src/factories/mesh_factory.h

    src/logging/logger.cpp
    src/logging/logger.h

    src/renderer/buffer.cpp
    src/renderer/buffer.h
    src/renderer/command.cpp
    src/renderer/command.h
    src/renderer/device.cpp
    src/renderer/device.h
    src/renderer/frame.cpp
    src/renderer/frame.h
    src/renderer/image.cpp
    src/renderer/image.h
    src/renderer/instance.cpp
    src/renderer/instance.h
    src/renderer/renderer.cpp
    src/renderer/renderer.h
    src/renderer/shader.cpp
    src/renderer/shader.h
    src/renderer/swapchain.cpp
    src/renderer/swapchain.h
    src/renderer/synchronisation.cpp
    src/renderer/synchronisation.h

    src/main.cpp
)

    get_filename_component(source_path ${source} PATH)
    string(REPLACE "${CMAKE_SOURCE_DIR}/src" "" filter_path ${source_path})
    string(REPLACE "/" "\\" filter_path ${filter_path})
    source_group(${filter_path} FILES ${source})
endforeach()
