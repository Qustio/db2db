#include <db2db/source.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

namespace py = pybind11;

PYBIND11_MODULE(mymodule, m, py::mod_gil_not_used()) {
	m.doc() = "test lib.cpp module";
}