cmake_minimum_required(VERSION 3.0.2)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(XSBEXTERN_BUILD_OPENAL_SOFT ON)
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

