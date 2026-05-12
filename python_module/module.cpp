#include <pybind11/pybind11.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/stl.h>
#include <source.h>
#include <iostream>
#include <Windows.h>

namespace py = pybind11;

std::string ansi_to_utf8(const std::string& ansi_str) {
	int wchar_count = MultiByteToWideChar(
		CP_ACP, 0,
		ansi_str.data(), ansi_str.length(),
		nullptr, 0
	);
	std::wstring wstr(wchar_count, 0);
	MultiByteToWideChar(
		CP_ACP, 0,
		ansi_str.data(), ansi_str.length(),
		wstr.data(), wchar_count
	);
	int utf8_count = WideCharToMultiByte(
		CP_UTF8, 0,
		wstr.data(), wstr.length(),
		nullptr, 0,
		nullptr, nullptr
	);
	std::string utf8_str(utf8_count, 0);
	WideCharToMultiByte(
		CP_UTF8, 0,
		wstr.data(), wstr.length(),
		utf8_str.data(), utf8_count,
		nullptr, nullptr
	);
	return utf8_str;
}

class db_data_class {
public:
	explicit
	db_data_class() noexcept = default;
	explicit
	db_data_class(db_data data) : _data{ std::move(data) } {};
	db_data_class(const db_data_class& other) : db_data_class{other._data} {};
	db_data_class(db_data_class&& other) noexcept : db_data_class{ std::move(other._data) } {};
	db_data_class& operator=(const db_data_class& other) noexcept {
		this->_data = other._data;
		return *this;
	}
	db_data_class& operator=(db_data_class&& other) noexcept {
		std::swap(other._data ,this->_data);
		return *this;
	}
	db_data_class(std::initializer_list<db_data::value_type> init) : _data{init} {}
private:
	db_data _data;
};

PYBIND11_MODULE(mymodule, m, py::mod_gil_not_used()) {
	m.doc() = "test lib.cpp module";

	py::class_<source>(m, "Source")
		// constructors
		.def(py::init<const std::string&>())
		.def_static("from_file", &source::from_file)
		//methods
		.def("select",
			[](
				source& self,
				const nanodbc::string& query,
				const std::vector<db_value>& params
			) {
				py::gil_scoped_release release;
				return self.select(query, params);
			},
			py::arg("query"),
			py::arg("params") = std::vector<db_value>{}
		)
		.def("insert",
			[](
				source& self,
				const nanodbc::string& query,
				const db_data& data,
				const std::vector<nanodbc::string>& columns
			) {
				py::gil_scoped_release release;
				self.insert(query, data, columns);
			},
			py::arg("query"),
			py::arg("data"),
			py::arg("params") = std::vector<std::string>{}
		);

	//py::class_<db_data>(m, "DB_Data")
	//	.def(py::init<const db_data&>())
	//	.def("__getitem__", [](db_data& self, nanodbc::string& key) {
	//		return self.at(key);
	//	});
	m.def("filter", [](db_data& self, const nanodbc::string& key, py::function fn) {
		::filter(self, key, [&](db_value v) {
			return fn(v).cast<bool>();
		});
	});

	py::register_exception_translator([](std::exception_ptr p) {
		try {
			if (p) std::rethrow_exception(p);
		} catch (const std::exception& e) {
			PyErr_SetString(PyExc_RuntimeError, ansi_to_utf8(e.what()).c_str());
		}
	});
}