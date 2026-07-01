find_package(pybind11 CONFIG)

if(NOT pybind11_FOUND)
	FetchContent_Declare(
		pybind11
		GIT_REPOSITORY https://github.com/pybind/pybind11
		GIT_TAG v3.0.4
	)
	FetchContent_MakeAvailable(pybind11)
endif()

pybind11_add_module(mymodule module.cpp)
target_link_libraries(mymodule PRIVATE db2db)
