cmake_minimum_required(VERSION 3.20)
project(zonvras CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan-generated files
find_package(glfw3     REQUIRED)
find_package(glm       REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(imgui     REQUIRED)

# Find Vulkan SDK (provides vulkan-1.lib and glslc)
find_package(Vulkan REQUIRED)

# imgui backend sources – conanfile.py writes this file during `conan install`
include(${CMAKE_BINARY_DIR}/imgui_package_path.cmake)
set(IMGUI_BACKENDS_DIR ${IMGUI_PACKAGE_FOLDER}/res/bindings)

# ---- SPIR-V compilation ----
find_program(GLSLC glslc HINTS
    $ENV{VULKAN_SDK}/Bin
    $ENV{VULKAN_SDK}/bin
    REQUIRED
)

set(SHADER_SRC_DIR  ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR  ${CMAKE_BINARY_DIR}/shaders)

# Shared vertex shader + all example fragment shaders
# Re-run CMake when new example folders are added.
set(SHADER_SOURCES ${SHADER_SRC_DIR}/fullscreen.vert)
file(GLOB_RECURSE EXAMPLE_FRAGS ${SHADER_SRC_DIR}/examples/*/main.frag)
list(APPEND SHADER_SOURCES ${EXAMPLE_FRAGS})

set(SHADER_SPIRV)
foreach(SHADER ${SHADER_SOURCES})
    # Preserve the directory structure under build/shaders/
    file(RELATIVE_PATH REL_PATH ${SHADER_SRC_DIR} ${SHADER})
    set(SPV ${SHADER_OUT_DIR}/${REL_PATH}.spv)
    get_filename_component(SPV_DIR ${SPV} DIRECTORY)
    file(MAKE_DIRECTORY ${SPV_DIR})
    add_custom_command(
        OUTPUT  ${SPV}
        COMMAND ${GLSLC} ${SHADER} -o ${SPV}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${REL_PATH}"
        VERBATIM
    )
    list(APPEND SHADER_SPIRV ${SPV})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_SPIRV})

# ---- Executable ----
add_executable(zonvras
    src/main.cpp
    src/app.cpp
    src/camera.cpp
    src/vulkan_context.cpp
    src/shader_watcher.cpp
    # ImGui backends (compiled as part of our target)
    ${IMGUI_BACKENDS_DIR}/imgui_impl_vulkan.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
)

add_dependencies(zonvras shaders)

target_include_directories(zonvras PRIVATE
    src
    ${IMGUI_BACKENDS_DIR}
)

target_link_libraries(zonvras PRIVATE
    glfw
    glm::glm
    Vulkan::Vulkan
    Vulkan::Headers
    imgui::imgui
)

# Pass shader output directory to the app
target_compile_definitions(zonvras PRIVATE
    SHADER_DIR="${SHADER_OUT_DIR}/"
)

if(MSVC)
    target_compile_options(zonvras PRIVATE /W4 /wd4201 /wd4324)
else()
    target_compile_options(zonvras PRIVATE -Wall -Wextra)
endif()
