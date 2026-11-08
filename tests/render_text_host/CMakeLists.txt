cmake_minimum_required(VERSION 3.20)
project(render_text LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(RENDER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../render" CACHE PATH "Engine source and definitions")
set(RENDER_BUILD "${RENDER_ROOT}/build" CACHE PATH "Built render DLL")
add_library(render SHARED IMPORTED)
set_target_properties(render PROPERTIES
    IMPORTED_LOCATION "${RENDER_BUILD}/render.dll"
    IMPORTED_IMPLIB "${RENDER_BUILD}/librender.dll.a"
    INTERFACE_INCLUDE_DIRECTORIES "${RENDER_ROOT}/src")
add_executable(render_text test_host/main.cpp)
target_link_libraries(render_text PRIVATE render shell32)
if(MINGW)
    target_link_options(render_text PRIVATE -static)
endif()
add_custom_command(TARGET render_text POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RENDER_BUILD}/render.dll" "$<TARGET_FILE_DIR:render_text>/render.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/workspace.json" "$<TARGET_FILE_DIR:render_text>/workspace.json")
