cmake_minimum_required(VERSION 3.21.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project("db2db")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(FETCHCONTENT_QUIET OFF)
set(NANODBC_ENABLE_UNICODE OFF)
set(NANODBC_DISABLE_EXAMPLES ON)
set(NANODBC_FORCE_LIBCXX OFF)
set(NANODBC_DISABLE_TESTS ON)
set(NANODBC_SHARED_LIBS ON)
set(INSTALL_GTEST OFF)
set(gtest_force_shared_crt ON)

include(FetchContent)
include(CTest)

find_package(pybind11 CONFIG)

if(NOT pybind11_FOUND)
	FetchContent_Declare(
		pybind11
		GIT_REPOSITORY https://github.com/pybind/pybind11
		GIT_TAG v3.0.4
	)
	FetchContent_MakeAvailable(pybind11)
endif()

find_package(ODBC REQUIRED)
find_package(nanodbc CONFIG)

if(NOT nanodbc_FOUND)
	FetchContent_Declare(
		nanodbc
		GIT_REPOSITORY https://github.com/nanodbc/nanodbc
		GIT_TAG fd9b4f551b0f03780168c4b2ba880dcb5777aad4
	)
	FetchContent_MakeAvailable(nanodbc)
endif()

find_package(spdlog CONFIG)

if(NOT spdlog_FOUND)
	FetchContent_Declare(
		spdlog
		GIT_REPOSITORY https://github.com/gabime/spdlog
		GIT_TAG v1.17.0
	)
	FetchContent_MakeAvailable(spdlog)
endif()

find_package(GTest CONFIG)

if(NOT GTest_FOUND)
	FetchContent_Declare(
		GTest
		GIT_REPOSITORY https://github.com/google/googletest
		GIT_TAG v1.17.0
	)
	FetchContent_MakeAvailable(GTest)
endif()

find_package(tomlplusplus CONFIG)

if(NOT tomlplusplus_FOUND)
	FetchContent_Declare(
		tomlplusplus
		GIT_REPOSITORY https://github.com/marzer/tomlplusplus
		GIT_TAG v3.4.0
	)
	FetchContent_MakeAvailable(tomlplusplus)
endif()

add_subdirectory("lib")

# add_subdirectory ("python_module")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cli/CMakeLists.txt")
	add_subdirectory("cli")
endif()
