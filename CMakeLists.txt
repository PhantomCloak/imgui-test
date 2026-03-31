cmake_minimum_required(VERSION 3.22)
project(imgui_webgpu_testbed C CXX)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)

set(SOURCES
    src/main.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
)

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")

    target_compile_definitions(${PROJECT_NAME} PUBLIC
        IMGUI_IMPL_WEBGPU_BACKEND_DAWN
    )

    target_compile_options(${PROJECT_NAME} PUBLIC
        --use-port=emdawnwebgpu
        --use-port=contrib.glfw3
        -sDISABLE_EXCEPTION_CATCHING=1
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        --use-port=emdawnwebgpu
        --use-port=contrib.glfw3
        -sWASM=1
        -sASYNCIFY=1
        -sALLOW_MEMORY_GROWTH=1
        -sNO_EXIT_RUNTIME=0
        -sASSERTIONS=1
        -sDISABLE_EXCEPTION_CATCHING=1
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/Resources@/Resources"
    )
endif()
