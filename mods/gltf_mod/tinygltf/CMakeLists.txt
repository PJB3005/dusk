add_library(tinygltf STATIC tiny_gltf_v3.c)
target_include_directories(tinygltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tinygltf PRIVATE TINYGLTF3_ENABLE_FS)
