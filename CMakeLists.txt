cmake_minimum_required(VERSION 3.0.2)
set(SKIP_INSTALL_ALL TRUE)
add_subdirectory(zlib-1.2.8)
set(ZLIB_FOUND TRUE)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib-1.2.8 ${CMAKE_CURRENT_BINARY_DIR}/zlib-1.2.8)
set(ZLIB_LIBRARY $<TARGET_FILE:zlibstatic>)

set(PNG_FRAMEWORK FALSE)
set(PNG_DEBUG FALSE)
set(PNG_STATIC TRUE)
set(PNG_SHARED FALSE)
set(PNG_TESTS FALSE)
add_subdirectory(lpng1623)
set(PNG_FOUND TRUE)
set(PNG_LIBRARY png_static)
add_dependencies(png_static zlibstatic)

# PCRE
set(PCRE_BUILD_PCREGREP OFF CACHE BOOL "Build pcregrep")
set(PCRE_BUILD_TESTS OFF CACHE BOOL "Build pcre tests")
set(PCRE_SHOW_REPORT OFF CACHE BOOL "Show PCRE configuration report")
add_subdirectory(pcre-8.37)

# HTTP-Tiny
add_subdirectory(http-tiny-1.2)