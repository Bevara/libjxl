cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBJXL_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/reframe_jxl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_jxl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libcxx_compat_shim.cpp
)

SET(LIBJXL_LIB
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libjxl.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libhwy.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libbrotlicommon.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libbrotlidec.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libc++.a
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libc++abi.a
)

SET(LIBJXL_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_filter(libjxl
        "${LIBJXL_SRC}"
        "${LIBJXL_LIB}"
        []
        ""
        "${LIBJXL_INC}"
        "-O0"
        "1")

# See libcxx_compat_shim.cpp: at -Oz the compiler can prove some of this
# file's calls never need a particular std::string/sort growth path for
# the specific (fixed, small) test inputs used, and prunes it away along
# with the symbol libjxl_1 needs to import. -O0 keeps every call site real.
set_source_files_properties(libcxx_compat_shim.cpp PROPERTIES COMPILE_FLAGS "-O0")
target_compile_features(libjxl_1 PRIVATE cxx_std_20)
