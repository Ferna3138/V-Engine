cmake_minimum_required(VERSION 3.28.0)
project(V-Engine VERSION 1.0.0)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

# ------------------------------------------------------------------
#  Dependency locations
# ------------------------------------------------------------------
set(GLFW_LIB ${CMAKE_SOURCE_DIR}/Dependencies/lib-vc2022/glfw3.lib)
set(GLFW_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Dependencies/glfw/include)
set(TINYOBJ_PATH ${CMAKE_SOURCE_DIR}/Dependencies/libs/tinyobjloader)
set(TINYGLTF_PATH ${CMAKE_SOURCE_DIR}/Dependencies/libs/tinygltfloader)
set(ENTT_PATH ${CMAKE_SOURCE_DIR}/Dependencies/libs/EnTT)

# Add enkiTS (vendored in Dependencies/enkiTS, upstream: https://github.com/dougbinks/enkiTS @ ccd4e8c)
set(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENKITS_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/Dependencies/enkiTS)

# GLFW: prebuilt lib on Windows, built from source elsewhere.
if(WIN32)
    set(GLFW_TARGET ${GLFW_LIB})
else()
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/Dependencies/glfw)
    set(GLFW_TARGET glfw)
endif()

# ------------------------------------------------------------------
#  ImGui (application layer only)
# ------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/Dependencies/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/Dependencies/
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    $<$<PLATFORM_ID:Windows>:${GLFW_INCLUDE_DIR}>
)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_DOCKING)

# ==================================================================
#  Engine layers:  Foundation  <-  Rendering  <-  Application
#  Each layer is its own static library so the dependency direction
#  is enforced by the linker, not just by convention.
# ==================================================================

# ---- Foundation -------------------------------------------------
file(GLOB_RECURSE FOUNDATION_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/Foundation/*.cpp)
file(GLOB_RECURSE FOUNDATION_HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/Foundation/*.hpp)
add_library(vengine_foundation STATIC ${FOUNDATION_SOURCES} ${FOUNDATION_HEADERS})
target_include_directories(vengine_foundation PUBLIC
    ${CMAKE_SOURCE_DIR}/Src
    ${CMAKE_SOURCE_DIR}/Dependencies
    $<$<PLATFORM_ID:Windows>:${GLFW_INCLUDE_DIR}>
)
target_link_libraries(vengine_foundation PUBLIC Vulkan::Vulkan enkiTS ${GLFW_TARGET})

# ---- Rendering -------------------------------------------------
file(GLOB_RECURSE RENDERING_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/Rendering/*.cpp)
file(GLOB_RECURSE RENDERING_HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/Rendering/*.hpp)
add_library(vengine_rendering STATIC ${RENDERING_SOURCES} ${RENDERING_HEADERS})
target_include_directories(vengine_rendering PUBLIC
    ${TINYOBJ_PATH}
    ${TINYGLTF_PATH}
)
target_compile_definitions(vengine_rendering PUBLIC TINYGLTF3_ENABLE_FS)
target_link_libraries(vengine_rendering PUBLIC vengine_foundation)

# ---- Application (executable) ---------------------------------
file(GLOB_RECURSE APPLICATION_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/Application/*.cpp)
file(GLOB_RECURSE APPLICATION_HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/Application/*.hpp)
add_executable(V-Engine ${APPLICATION_SOURCES} ${APPLICATION_HEADERS})
target_include_directories(V-Engine PRIVATE ${ENTT_PATH})
target_link_libraries(V-Engine PRIVATE vengine_rendering imgui)

# ------------------------------------------------------------------
#  Shader compilation
# ------------------------------------------------------------------
if(WIN32)
    set(SHADER_COMPILE_SCRIPT ${CMAKE_SOURCE_DIR}/Src/Rendering/Shaders/compile.bat)
else()
    set(SHADER_COMPILE_SCRIPT ${CMAKE_SOURCE_DIR}/Src/Rendering/Shaders/compile.sh)
    execute_process(COMMAND chmod +x ${SHADER_COMPILE_SCRIPT})
endif()

add_custom_target(CompileShaders
    COMMAND ${SHADER_COMPILE_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Src/Rendering/Shaders
    COMMENT "Compiling shaders"
)
add_dependencies(V-Engine CompileShaders)

# ------------------------------------------------------------------
#  Visual Studio: mirror the on-disk folders into solution filters
# ------------------------------------------------------------------
source_group(TREE ${CMAKE_SOURCE_DIR}/Src FILES
    ${FOUNDATION_SOURCES} ${FOUNDATION_HEADERS}
    ${RENDERING_SOURCES} ${RENDERING_HEADERS}
    ${APPLICATION_SOURCES} ${APPLICATION_HEADERS})
