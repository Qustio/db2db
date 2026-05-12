cmake_minimum_required (VERSION 3.10)

# if (POLICY CMP0141)
#   cmake_policy(SET CMP0141 NEW)
#   set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
# endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

project ("db2db")

set(BUILD_SHARED_LIBS ON)
set(BOOST_INCLUDE_LIBRARIES headers program_options container)
set(BOOST_ENABLE_CMAKE ON)
set(NANODBC_ENABLE_UNICODE OFF)
set(NANODBC_FORCE_LIBCXX OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

include(FetchContent)
include(CTest)
FetchContent_Declare(
   Boost
   SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/boost-1.91.0-1-cmake/boost-1.91.0-1
)
FetchContent_Declare(
	pybind11
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/pybind11
)
FetchContent_Declare(
	nanodbc
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/nanodbc
)
FetchContent_Declare(
	spdlog
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/spdlog
)
FetchContent_Declare(
	googletest
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/googletest
)
FetchContent_Declare(
	tomlplusplus
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/tomlplusplus
)
FetchContent_MakeAvailable(Boost)
FetchContent_MakeAvailable(pybind11)
FetchContent_MakeAvailable(nanodbc)
FetchContent_MakeAvailable(spdlog)
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(googletest)
set(BUILD_SHARED_LIBS ON)
FetchContent_MakeAvailable(tomlplusplus)

add_subdirectory ("lib")
#add_subdirectory ("python_module")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cli/CMakeLists.txt")
	add_subdirectory ("cli")
endif()
