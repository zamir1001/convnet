CMAKE_MINIMUM_REQUIRED(VERSION 3.5.0 FATAL_ERROR)

PROJECT(zlib-download NONE)

INCLUDE(ExternalProject)
ExternalProject_Add(zlib
	GIT_REPOSITORY https://github.com/madler/zlib.git
	GIT_TAG master
	SOURCE_DIR "${DNN_DEPENDENCIES_SOURCE_DIR}/zlib-1.2.11"
	BINARY_DIR "${DNN_DEPENDENCIES_BINARY_DIR}/zlib-1.2.11"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
)
