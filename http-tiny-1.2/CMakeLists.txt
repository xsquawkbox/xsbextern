cmake_minimum_required(VERSION 3.0.2)


if(CMAKE_SYSTEM_NAME MATCHES "Linux")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
endif()

add_library(httptiny
	${XPMP_PLATFORM_SOURCES}
	http_lib.c)
target_include_directories(httptiny
	PUBLIC 
		${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(httptiny
	PRIVATE	${HTTPTINY_PLATFORM_LIBRARIES})
