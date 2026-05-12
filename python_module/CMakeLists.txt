pybind11_add_module(mymodule module.cpp)
target_link_libraries(mymodule PRIVATE nanodbc spdlog db2db)
target_include_directories(mymodule PRIVATE ${Boost_INCLUDE_DIRS})
