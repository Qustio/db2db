cmake_minimum_required(VERSION 3.21.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project("db2db")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS ON)
set(NANODBC_ENABLE_UNICODE OFF)
set(NANODBC_DISABLE_TESTS ON)
set(NANODBC_DISABLE_EXAMPLES ON)
set(NANODBC_FORCE_LIBCXX OFF)
set(NANODBC_SHARED_LIBS ON)
set(gtest_force_shared_crt ON)

include(FetchContent)
include(CTest)

find_package(Boost 1.91 CONFIG COMPONENTS headers program_options container)
if(NOT Boost_FOUND)
	message(STATUS "Boost not found")
	set(BOOST_INCLUDE_LIBRARIES headers program_options container)
	set(BOOST_ENABLE_CMAKE ON)
	FetchContent_Declare(
		Boost
		SOURCE_DIR
		${CMAKE_SOURCE_DIR}/deps/boost-1.91.0-1-cmake/boost-1.91.0-1
	)
	FetchContent_MakeAvailable(Boost)
endif()

find_package(pybind11 CONFIG)
if(NOT pybind11_FOUND)
	FetchContent_Declare(pybind11 SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/pybind11)
	FetchContent_MakeAvailable(pybind11)
endif()

find_package(ODBC REQUIRED)
find_package(nanodbc CONFIG)
if(NOT nanodbc_FOUND)
	FetchContent_Declare(nanodbc SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/nanodbc)
	FetchContent_MakeAvailable(nanodbc)
endif()

find_package(spdlog CONFIG)
if(NOT spdlog_FOUND)
	FetchContent_Declare(spdlog SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/spdlog)
	FetchContent_MakeAvailable(spdlog)
endif()

find_package(GTest CONFIG)
if(NOT GTest_FOUND)
	FetchContent_Declare(GTest SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/googletest)
	set(BUILD_SHARED_LIBS OFF)
	FetchContent_MakeAvailable(GTest)
	set(BUILD_SHARED_LIBS ON)
endif()

find_package(tomlplusplus CONFIG)
if(NOT tomlplusplus_FOUND)
	FetchContent_Declare(
		tomlplusplus
		SOURCE_DIR
		${CMAKE_SOURCE_DIR}/deps/tomlplusplus
	)
	FetchContent_MakeAvailable(tomlplusplus)
endif()

add_subdirectory("lib")
#add_subdirectory ("python_module")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cli/CMakeLists.txt")
	add_subdirectory("cli")
endif()
