cmake_minimum_required(VERSION 3.25)
project(gltf_mod CXX)
set(CMAKE_CXX_STANDARD 20)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DUSK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Path to dusk source root")
    option(DUSK_MOD_USE_FULL_TREE "Use full build instead of the minimal mod SDK" OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if (DUSK_MOD_USE_FULL_TREE)
        add_subdirectory("${DUSK_DIR}" dusk EXCLUDE_FROM_ALL)
    else ()
        add_subdirectory("${DUSK_DIR}/sdk" dusk-sdk EXCLUDE_FROM_ALL)
    endif ()
endif ()

add_mod(gltf_mod
    FEATURES game fmt webgpu
    SOURCES  src/mod.cpp src/loader.cpp src/render.cpp src/image_io.cpp src/shader.cpp
    MOD_JSON mod.json
    RES_DIR  res
)

include(FetchContent)
include(wesl.cmake)

FetchContent_Declare(
        glm
        GIT_REPOSITORY	https://github.com/g-truc/glm.git
        GIT_TAG 	8d1fd52e5ab5590e2c81768ace50c72bae28f2ed #refs/tags/1.0.3
)

FetchContent_MakeAvailable(glm)

add_subdirectory(tinygltf)
add_subdirectory(src/helpers)

target_compile_definitions(gltf_mod PRIVATE GLM_ENABLE_EXPERIMENTAL)
target_precompile_headers(gltf_mod PRIVATE <vector> <memory>)
target_link_libraries(gltf_mod PRIVATE tinygltf glm::glm png_static wesl nlohmann_json::nlohmann_json imgui)

if (MSVC)
    target_compile_options(gltf_mod PRIVATE /guard:cf)
    target_link_options(gltf_mod PRIVATE /guard:cf)
    target_link_options(gltf_mod PRIVATE /CETCOMPAT)
endif ()
