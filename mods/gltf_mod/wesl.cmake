add_library(wesl STATIC IMPORTED)
set_target_properties(wesl PROPERTIES IMPORTED_LOCATION "E:/Projects/wesl-rs/target/debug/wesl_c.lib")
target_include_directories(wesl INTERFACE "E:/Projects/wesl-rs/crates/wesl-c/include")

if (WIN32)
    target_link_libraries(wesl INTERFACE Ws2_32.lib ntdll.lib Userenv.lib )
endif ()
