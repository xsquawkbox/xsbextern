cmake_minimum_required(VERSION 3.0.2)

# export the conan zlib library so libpng finds it.
set(ZLIB_LIBRARY CONAN_PKG::ZLIB)
set(ZLIB_INCLUDE_DIR ${CONAN_INCLUDE_DIRS_ZLIB})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(XSBEXTERN_BUILD_LIBPNG ON) 
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(XSBEXTERN_BUILD_LIBPNG ON) 
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(XSBEXTERN_BUILD_LIBPNG ON)
  set(XSBEXTERN_BUILD_OPENAL_SOFT ON)
else()
  message(FATAL_ERROR "Unrecognised Platform.  Aborting.")
endif()

if (XSBEXTERN_BUILD_LIBPNG)
  set(PNG_FRAMEWORK FALSE CACHE BOOL "Build libpng as Framework" FORCE)
  set(PNG_DEBUG FALSE CACHE BOOL "Build libpng with debuggings" FORCE)
  set(PNG_STATIC TRUE CACHE BOOL "Build libpng static" FORCE)
  set(PNG_SHARED FALSE CACHE BOOL "Build libpng shared" FORCE)
  set(PNG_TESTS FALSE CACHE BOOL "Build libpng tests" FORCE)
  add_subdirectory(libpng-1.6.28 EXCLUDE_FROM_ALL)
  set(PNG_FOUND TRUE CACHE BOOL "PNG Library Available" FORCE)
  set(PNG_LIBRARY png_static CACHE STRING "PNG Library Path" FORCE)
else()
  find_library(PNG NAMES png png16 DOC "libpng image library")
endif()

if(XSBEXTERN_BUILD_OPENAL_SOFT)
  set(ALSOFT_UTILS OFF CACHE BOOL "Build and install utility programs" FORCE)
  set(ALSOFT_NO_CONFIG_UTIL ON CACHE BOOL "Disable building the alsoft-config utility" FORCE)
  set(ALSOFT_EXAMPLES OFF CACHE BOOL "Build and install example programs" FORCE)
  set(ALSOFT_TESTS OFF CACHE BOOL "Build and install test programs" FORCE)
  set(ALSOFT_CONFIG OFF CACHE BOOL "Install alsoft.conf sample configuration file" FORCE)
  set(ALSOFT_HRTF_DEFS OFF CACHE BOOL "Install HRTF definition files" FORCE)
  set(ALSOFT_AMBDEC_PRESETS OFF CACHE BOOL "Install AmbDec preset files" FORCE)
  set(ALSOFT_INSTALL ON CACHE BOOL "Install headers and libraries" FORCE)

  add_subdirectory(openal-soft-1.19.1 EXCLUDE_FROM_ALL)

  # OpenAL doesn't export its public headers with its target.  Force this.
  target_include_directories(OpenAL
	  INTERFACE
	  $<BUILD_INTERFACE:${OpenAL_SOURCE_DIR}/include>)
else()
  find_library(OpenAL NAMES al openal OpenAL DOC "OpenAL Audio Library")
endif()

