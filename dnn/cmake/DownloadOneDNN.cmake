CMAKE_MINIMUM_REQUIRED(VERSION 3.5.0 FATAL_ERROR)

PROJECT(oneDNN-download NONE)

INCLUDE(ExternalProject)
ExternalProject_Add(oneDNN
	GIT_REPOSITORY https://github.com/oneapi-src/oneDNN.git
	GIT_TAG master
	SOURCE_DIR "${DNN_DEPENDENCIES_SOURCE_DIR}/oneDNN"
	BINARY_DIR "${DNN_DEPENDENCIES_BINARY_DIR}/oneDNN"
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	TEST_COMMAND ""
)