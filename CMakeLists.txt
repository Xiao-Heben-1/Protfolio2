cmake_minimum_required(VERSION 3.20)

project(Render LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED COMPONENTS glslc)

option(RENDER_BUILD_EDITOR "Build ImGui docking editor" ON)
if(RENDER_BUILD_EDITOR)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
add_subdirectory(include/glfw-source)
endif()


# =========================================================
# Assimp
# =========================================================

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(include/assimp)


# =========================================================
# ImGui
# =========================================================

if(RENDER_BUILD_EDITOR)
set(RENDER_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui-docking" CACHE PATH "Dear ImGui docking source")
add_library(imgui STATIC)

target_sources(imgui PRIVATE
    ${RENDER_IMGUI_DIR}/imgui.cpp
    ${RENDER_IMGUI_DIR}/imgui_draw.cpp
    ${RENDER_IMGUI_DIR}/imgui_tables.cpp
    ${RENDER_IMGUI_DIR}/imgui_widgets.cpp

    ${RENDER_IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${RENDER_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${RENDER_IMGUI_DIR}
    ${RENDER_IMGUI_DIR}/backends
)

target_link_directories(imgui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(imgui PRIVATE
    Vulkan::Vulkan
    glfw
)
endif()


# =========================================================
# Render
# =========================================================

file(GLOB_RECURSE RENDER_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

list(FILTER RENDER_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")
list(FILTER RENDER_SOURCES EXCLUDE REGEX "/src/front/")
add_library(render_core STATIC ${RENDER_SOURCES})
add_library(render::core ALIAS render_core)
add_executable(Render src/main.cpp)
target_link_libraries(Render PRIVATE render::core)
set(RENDER_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/library/builtin" CACHE PATH "Built-in SPIR-V output directory")
file(GLOB_RECURSE RENDER_SHADER_INCLUDES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/core/include/*.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/shader/light/*.glsl"
)
function(render_compile_shader source output)
    get_filename_component(output_directory "${RENDER_SHADER_DIR}/${output}" DIRECTORY)
    add_custom_command(
        OUTPUT "${RENDER_SHADER_DIR}/${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_directory}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" --target-env=vulkan1.3 ${ARGN}
                -I "${CMAKE_CURRENT_SOURCE_DIR}/shader/core/include"
                -I "${CMAKE_CURRENT_SOURCE_DIR}/shader"
                "${CMAKE_CURRENT_SOURCE_DIR}/${source}" -o "${RENDER_SHADER_DIR}/${output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
                ${RENDER_SHADER_INCLUDES}
        VERBATIM
    )
    set(RENDER_SPIRV ${RENDER_SPIRV} "${RENDER_SHADER_DIR}/${output}" PARENT_SCOPE)
endfunction()
render_compile_shader(shader/core/pass/opaque_rendering/opaque.vert opaque/opaque.vert.spv)
render_compile_shader(shader/core/pass/opaque_rendering/opaque.frag opaque/opaque.frag.spv)
render_compile_shader(shader/core/pass/geometry_pass/geometry.vert geometry_pass/geometry.vert.spv)
render_compile_shader(shader/core/pass/geometry_pass/geometry.frag geometry_pass/geometry.frag.spv)
render_compile_shader(shader/core/pass/transparent_rendering/capture.vert transparent_rendering/capture/capture.vert.spv)
render_compile_shader(shader/core/pass/transparent_rendering/capture.frag transparent_rendering/capture/capture.frag.spv)
render_compile_shader(shader/core/pass/transparent_rendering/sort.comp transparent_rendering/sort/sort.comp.spv)
render_compile_shader(shader/core/pass/transparent_rendering/resolve.vert transparent_rendering/resolve/resolve.vert.spv)
render_compile_shader(shader/core/pass/transparent_rendering/resolve.frag transparent_rendering/resolve/resolve.frag.spv)
render_compile_shader(shader/core/pass/pre_processing/frustum_cull.comp pre_processing/frustum_cull.comp.spv)
render_compile_shader(shader/core/pass/skybox/skybox.vert skybox/skybox.vert.spv)
render_compile_shader(shader/core/pass/skybox/skybox.frag skybox/skybox.frag.spv)
render_compile_shader(shader/core/pass/post_processing/tone_mapping.vert post_processing/tone_mapping.vert.spv)
render_compile_shader(shader/core/pass/post_processing/tone_mapping.frag post_processing/tone_mapping.frag.spv)
add_custom_target(render_shaders DEPENDS ${RENDER_SPIRV})
add_dependencies(Render render_shaders)
if(RENDER_BUILD_EDITOR)
    file(GLOB_RECURSE RENDER_FRONT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/front/*.cpp")
    list(FILTER RENDER_FRONT_SOURCES EXCLUDE REGEX "/main\\.cpp$")
    add_library(render_runtime SHARED ${RENDER_FRONT_SOURCES})
    add_library(render::runtime ALIAS render_runtime)
    set_target_properties(render_runtime PROPERTIES OUTPUT_NAME render PREFIX "" DEBUG_POSTFIX "")
    target_compile_definitions(render_runtime PRIVATE RENDER_RUNTIME_EXPORTS)
    target_link_libraries(render_runtime PRIVATE render::core imgui comdlg32)
    target_include_directories(render_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_dependencies(render_runtime render_shaders)
    add_executable(RenderEditor src/front/main.cpp)
    target_link_libraries(RenderEditor PRIVATE render::runtime shell32)
    if(MINGW)
        target_link_options(render_runtime PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
        target_link_options(RenderEditor PRIVATE -static)
    endif()
    add_custom_command(TARGET RenderEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/config/workspace.json" "$<TARGET_FILE_DIR:RenderEditor>/workspace.json")
endif()
target_compile_definitions(render_core PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
# Assimp 与引擎静态链接同一份 RapidJSON 模板，成员迭代器的返回 ABI 必须一致。
target_compile_definitions(render_core PUBLIC RAPIDJSON_HAS_STDSTRING=1)
if(ASSIMP_RAPIDJSON_NO_MEMBER_ITERATOR)
    target_compile_definitions(render_core PUBLIC RAPIDJSON_NOMEMBERITERATORCLASS)
endif()
target_include_directories(render_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/assimp/contrib/rapidjson/include
)

target_link_directories(render_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(render_core PUBLIC
    Vulkan::Vulkan
    assimp
)

option(RENDER_BUILD_PRIM_TESTS "Build primitive streaming lifecycle tests" OFF)
if(RENDER_BUILD_PRIM_TESTS)
    enable_testing()
    add_executable(qem_test tests/qem_test.cpp)
    target_include_directories(qem_test PRIVATE src include include/assimp/contrib/rapidjson/include)
    if(MINGW)
        target_link_options(qem_test PRIVATE -static)
    endif()
    add_test(NAME qem_test COMMAND qem_test)
    set_tests_properties(qem_test PROPERTIES TIMEOUT 30)
    add_executable(object_loader_test tests/object_loader_test.cpp)
    target_link_libraries(object_loader_test PRIVATE render::core)
    if(MINGW)
        target_link_options(object_loader_test PRIVATE -static)
    endif()
    add_test(NAME object_loader_test COMMAND object_loader_test)
    set_tests_properties(object_loader_test PROPERTIES TIMEOUT 30)
    add_executable(prim_stream_test
        tests/prim_stream_test.cpp
        src/file/config/scence_prim.cpp
        src/file/config/resource/page_manager.cpp
        src/tools/structures/BVH/BVH.cpp src/file/tools/guid.cpp)
    target_include_directories(prim_stream_test PRIVATE src include include/assimp/contrib/rapidjson/include)
    # Keep the test independent of DLL search order in CTest/IDE launchers.
    if(MINGW)
        target_link_options(prim_stream_test PRIVATE -static)
    endif()
    add_test(NAME prim_stream_test COMMAND prim_stream_test)
    set_tests_properties(prim_stream_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 30)

    # Exercise the real scanner/resource integration, including cache restoration.
    add_executable(geometry_cache_test tests/geometry_cache_test.cpp tests/config_resource_test.cpp tests/render_pass_config_test.cpp)
    target_link_libraries(geometry_cache_test PRIVATE render::core)
    target_compile_definitions(geometry_cache_test PRIVATE "TEST_GLSLC_EXECUTABLE=\"${Vulkan_GLSLC_EXECUTABLE}\"")
    if(MINGW)
        target_link_options(geometry_cache_test PRIVATE -static)
    endif()
    add_test(NAME geometry_cache_test COMMAND geometry_cache_test)
    set_tests_properties(geometry_cache_test PROPERTIES TIMEOUT 30)

    add_executable(behavior_test tests/behavior_test.cpp)
    target_link_libraries(behavior_test PRIVATE render::core)
    if(MINGW)
        target_link_options(behavior_test PRIVATE -static)
    endif()
    add_test(NAME behavior_test COMMAND behavior_test)
    set_tests_properties(behavior_test PROPERTIES TIMEOUT 30)
endif()

option(RENDER_BUILD_CULL_GPU_TESTS "Build Vulkan GPU frustum culling test" OFF)
if(RENDER_BUILD_CULL_GPU_TESTS)
    enable_testing()
    add_executable(frustum_cull_gpu_test tests/frustum_cull_gpu_test.cpp)
    target_link_libraries(frustum_cull_gpu_test PRIVATE render::core)
    if(MINGW)
        target_link_options(frustum_cull_gpu_test PRIVATE -static)
    endif()
    add_dependencies(frustum_cull_gpu_test render_shaders)
    add_test(NAME frustum_cull_gpu_test COMMAND frustum_cull_gpu_test "${RENDER_SHADER_DIR}/pre_processing/frustum_cull.comp.spv")
    set_tests_properties(frustum_cull_gpu_test PROPERTIES TIMEOUT 60)
endif()



