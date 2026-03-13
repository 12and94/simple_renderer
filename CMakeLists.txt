cmake_minimum_required(VERSION 3.28)

project(vulkan_renderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VR_ENABLE_VALIDATION "Enable Vulkan validation in debug builds" ON)
option(VR_ENABLE_TINYGLTF "Enable glTF loading with tinygltf header" OFF)
set(VR_TINYGLTF_ROOT "" CACHE PATH "Path to tinygltf root directory containing tiny_gltf.h")

find_package(Vulkan REQUIRED COMPONENTS glslc)

set(VR_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/render/RenderScene.cpp
    src/render/Renderer.cpp
    src/scene/GltfLoader.cpp
    src/scene/Scene.cpp
    src/vulkan/Swapchain.cpp
    src/vulkan/VulkanContext.cpp
)

set(VR_HEADERS
    src/app/Application.h
    src/app/InputState.h
    src/app/Timer.h
    src/core/Assert.h
    src/core/FileIO.h
    src/core/Log.h
    src/math/Math.h
    src/render/RenderScene.h
    src/render/Renderer.h
    src/scene/Camera.h
    src/scene/GltfLoader.h
    src/scene/Material.h
    src/scene/Mesh.h
    src/scene/Node.h
    src/scene/Scene.h
    src/scene/Transform.h
    src/vulkan/FrameContext.h
    src/vulkan/Swapchain.h
    src/vulkan/VulkanContext.h
)

add_executable(vulkan_renderer ${VR_SOURCES} ${VR_HEADERS})

target_include_directories(vulkan_renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(vulkan_renderer PRIVATE Vulkan::Vulkan user32)

target_compile_definitions(
    vulkan_renderer
    PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        UNICODE
        _UNICODE
        $<$<CONFIG:Debug>:VR_DEBUG_BUILD=1>
        $<$<BOOL:${VR_ENABLE_VALIDATION}>:VR_ENABLE_VALIDATION=1>
)

if(VR_ENABLE_TINYGLTF)
    if(VR_TINYGLTF_ROOT STREQUAL "")
        message(FATAL_ERROR "VR_ENABLE_TINYGLTF=ON requires VR_TINYGLTF_ROOT to be set.")
    endif()

    if(EXISTS "${VR_TINYGLTF_ROOT}/tiny_gltf.h")
        target_include_directories(vulkan_renderer PRIVATE "${VR_TINYGLTF_ROOT}")
    elseif(EXISTS "${VR_TINYGLTF_ROOT}/include/tiny_gltf.h")
        target_include_directories(vulkan_renderer PRIVATE "${VR_TINYGLTF_ROOT}/include")
    else()
        message(FATAL_ERROR "tiny_gltf.h not found under VR_TINYGLTF_ROOT.")
    endif()

    target_compile_definitions(vulkan_renderer PRIVATE VR_HAS_TINYGLTF=1)
endif()

if(MSVC)
    target_compile_options(vulkan_renderer PRIVATE /W4 /permissive- /EHsc /Zc:preprocessor)
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT vulkan_renderer)

if (NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found via Vulkan SDK. Required for shader compilation.")
endif()

set(VR_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${VR_SHADER_OUTPUT_DIR}")

set(VR_SHADER_SOURCES
    shaders/basic.vert
    shaders/basic.frag
)

set(VR_SHADER_OUTPUTS)
foreach(SHADER_FILE IN LISTS VR_SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_FILE}" NAME)
    set(SPIRV_OUT "${VR_SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT "${SPIRV_OUT}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}" -o "${SPIRV_OUT}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILE}"
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND VR_SHADER_OUTPUTS "${SPIRV_OUT}")
endforeach()

add_custom_target(vulkan_renderer_shaders ALL DEPENDS ${VR_SHADER_OUTPUTS})
add_dependencies(vulkan_renderer vulkan_renderer_shaders)
