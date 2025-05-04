cmake_minimum_required(VERSION 3.15)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(TargetArch)

project(MumblePlugin
	VERSION "0.1.0"
	DESCRIPTION "A positional audio plugin for World of Warcraft (x86) version 3.3.5a.12340"
	LANGUAGES "C"
)

set(PLUGIN_NAME "wow355pa")

add_library(plugin
	SHARED
		plugin.c
)

target_include_directories(plugin
	PUBLIC "${CMAKE_SOURCE_DIR}/include/"
)

# Add suffix for the respective OS
if (WIN32)
	set(PLUGIN_NAME "${PLUGIN_NAME}_win")
elseif (APPLE)
	set(PLUGIN_NAME "${PLUGIN_NAME}_macos")
elseif(UNIX)
	set(PLUGIN_NAME "${PLUGIN_NAME}_linux")
endif()

# Add suffix for target architecture
target_architecture(TARGET_ARCH)
string(TOLOWER "${TARGET_ARCH}" TARGET_ARCH)
if (NOT TARGET_ARCH STREQUAL "unknown")
	set(PLUGIN_NAME "${PLUGIN_NAME}_${TARGET_ARCH}")
endif()

set_target_properties(plugin PROPERTIES LIBRARY_OUTPUT_NAME "${PLUGIN_NAME}")

add_definitions(-D_GNU_SOURCE -D_DEFAULT_SOURCE)