cmake_minimum_required(VERSION 3.21.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project("db2db")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	option(DB2DB_BUILD_TESTS "Build tests" ON)
else()
	option(DB2DB_BUILD_TESTS "Build tests" OFF)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	option(DB2DB_PYTHON_MODULE "Python module" ON)
else()
	option(DB2DB_PYTHON_MODULE "Python module" OFF)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_QUIET OFF)
set(NANODBC_ENABLE_UNICODE OFF)
set(NANODBC_DISABLE_EXAMPLES ON)
set(NANODBC_FORCE_LIBCXX OFF)
set(NANODBC_DISABLE_TESTS ON)
set(NANODBC_SHARED_LIBS ON)
set(INSTALL_GTEST OFF)
set(gtest_force_shared_crt ON)

include(FetchContent)

if(DB2DB_BUILD_TESTS)
	include(CTest)
endif()

add_subdirectory("lib")

if(DB2DB_PYTHON_MODULE)
	add_subdirectory("python_module")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cli/CMakeLists.txt")
	add_subdirectory("cli")
endif()
