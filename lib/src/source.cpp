#include "source.h"

#include <any>
#include <cstdint>
#include <fstream>
#include <ranges>
#include <sqlext.h>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <format>
#include <boost/container/vector.hpp>
#include <spdlog/spdlog.h>
#include "nanodbc/nanodbc.h"
#include "nanodbc_optional.cpp"


namespace {
	void bind(
		nanodbc::statement& s,
		short index,
		const db_type& value
	) {
		std::visit([&](const auto& v) {
			if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::monostate>) {
				s.bind_null(index);
			} else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) {
				s.bind(index, v.data());
			} else {
				s.bind(index, &v);
			}
		}, value);
	}

	size_t row_count(const db_data& data) {
		return std::visit([](const auto& col) {
			return col.data.size();
		}, data.begin()->second.data);
	}

	std::vector<std::any> bind(
		nanodbc::statement& s,
		const db_data& data,
		std::span<const nanodbc::string> columns
	) {
		std::vector<std::any> storage;
		storage.reserve(columns.size()*2);
		short index = 0;
		for (const auto& column_name : columns) {
			auto it = data.find(column_name);
			if (it == data.end())
				throw std::runtime_error(std::format("No column named \"{}\"", column_name));
			const auto& column = it->second;
			auto count = row_count(data);

			std::visit([&](const auto& c) {
				using T = std::decay_t<decltype(c)>;
				if constexpr (std::is_same_v<typename T::type, nanodbc::string>) {
					s.bind_strings(index, c.data, reinterpret_cast<const bool*>(c.nulls.data()));
				} else {
					s.bind(index, c.data.data(), count, reinterpret_cast<const bool*>(c.nulls.data()));
				}
			}, column.data);
			index++;
		}
		return storage;
	}

	db_type get(nanodbc::result& r, short col) {
		if (r.is_null(col))
			return std::monostate{};
		switch (r.column_c_datatype(col)) {
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			case SQL_C_BINARY:
				return r.get<nanodbc::string>(col);
			case SQL_C_SSHORT:
				return r.get<short>(col);
			case SQL_C_USHORT:
				return r.get<unsigned short>(col);
			case SQL_C_LONG:
			case SQL_C_SLONG:
				return r.get<int32_t>(col);
			case SQL_C_ULONG:
				return r.get<uint32_t>(col);
			case SQL_C_FLOAT:
				return r.get<float>(col);
			case SQL_C_DOUBLE:
				return r.get<double>(col);
			case SQL_C_SBIGINT:
				return r.get<int64_t>(col);
			case SQL_C_UBIGINT:
				return r.get<uint64_t>(col);
			case SQL_C_DATE:
				return r.get<nanodbc::date>(col);
			case SQL_C_TIME:
				return r.get<nanodbc::time>(col);
			case SQL_C_TIMESTAMP:
				return r.get<nanodbc::timestamp>(col);
		}
		throw std::runtime_error(std::format("Type \"{}\" is missing is mapping", r.column_c_datatype(col)));
	}

	db_column_ make_column_storage(int c_type) {
		switch (c_type) {
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			case SQL_C_BINARY:
				return typed_column_<nanodbc::string>{};
			case SQL_C_SSHORT:
			case SQL_C_LONG:
			case SQL_C_SLONG:
				return typed_column_<int32_t>{};
			case SQL_C_USHORT:
			case SQL_C_ULONG:
				return typed_column_<uint32_t>{};
			case SQL_C_FLOAT:
				return typed_column_<float>{};
			case SQL_C_DOUBLE:
				return typed_column_<double>{};
			case SQL_C_SBIGINT:
				return typed_column_<int64_t>{};
			case SQL_C_UBIGINT:
				return typed_column_<uint64_t>{};
			case SQL_C_DATE:
				return typed_column_<nanodbc::date>{};
			case SQL_C_TIME:
				return typed_column_<nanodbc::time>{};
			case SQL_C_TIMESTAMP:
				return typed_column_<nanodbc::timestamp>{};
			default:
				return typed_column_<nanodbc::string>{};
		}
	}
}

bool operator==(const db_data &lhs, const db_data &rhs) {
  if (lhs.size() != rhs.size())
    return false;

  for (const auto &[key, col] : lhs) {
    auto it = rhs.find(key);
    if (it == rhs.end())
      return false;

    auto &dd = col;
    auto &ddd = it->second;
    // reuse db_column::operator== via span
    if (!(col == it->second))
      return false;
  }
  return true;
}

void filter(
	db_data &data, nanodbc::string column,
	std::function<bool(db_type &)> fn
) {
	if (data.empty())
		return;
	auto &values = data[column];
	std::vector<int> to_delete{};
	std::visit(
		[&](auto &values) {
		// using T = typename std::decay_t<decltype(values)>::type;
		for (size_t i = 0; i < values.data.size(); i++) {
			db_type element = values.data[i];
			if (!fn(element))
			to_delete.push_back(i);
		}
		},
		values.data);
	size_t offset = 0;
	for (auto& [key, col_values] : data) {
		std::visit([&to_delete](auto& data) {
			for (size_t i : to_delete)
				data.nulls[i] = 1;
		}, col_values.data);
	}
}

source::source(const nanodbc::string& connection_string)
: _connection(connection_string) {
	
};

source source::from_file(std::filesystem::path filename) {
	std::wifstream file(filename);
	if (!file)
		throw std::runtime_error("cannot open conn.txt");
	std::wstring cs;
	std::getline(file, cs);
	if (!cs.empty() && cs.back() == '\r')
		cs.pop_back();
	if (cs.empty())
		throw std::runtime_error("connection string is empty");
	//spdlog::info("{}", std::string(cs.begin(), cs.end()));
	return {nanodbc::string(cs.begin(), cs.end())};
}

db_data source::select	(
	const nanodbc::string& query,
	std::span<const db_type> params
) {
	db_data data{};
	auto s = nanodbc::statement(
		_connection,
		query
	);
	for (const auto& [i, param] : std::views::enumerate(params)) {
		::bind(s, i, param);
	}
	auto result = s.execute();
	for (int i = 0; i < result.columns(); i++) {
		data[result.column_name(i)].data = make_column_storage(result.column_c_datatype(i));
	}
	while (result.next()) {
		for (short i = 0; i < result.columns(); i++) {
			try {
				auto value = ::get(result, i);
				std::visit([&value](auto& column) {
					using T = typename std::decay_t<decltype(column)>::type;
					auto* val = std::get_if<T>(&value);
					column.nulls.push_back(val ? 0 : 1);
					column.data.push_back(val ? std::move(*val) : T{});
				}, data[result.column_name(i)].data);
			}
			catch (const std::runtime_error& e) {
				spdlog::error("{}", e.what());
			}
		}
	}
	return data;
}

db_type source::select_one(
	const nanodbc::string& query,
	std::span<const db_type> params
) {
	db_data data{};
	auto s = nanodbc::statement(
		_connection,
		query
	);
	for (const auto& [i, param] : std::views::enumerate(params)) {
		::bind(s, i, param);
	}
	auto result = s.execute();
	if (!result.next()) {
		return db_type{};
	}
	if (!result.is_null(0)) {
		return get(result, 0);
		//return result.get<nanodbc::string>(0);
	}
	return db_type{};
}

void source::exec(
	const nanodbc::string& query,
	std::span<const db_type> params
) {
	auto s = nanodbc::statement(
		_connection,
		query
	);
	for (const auto& [i, param] : std::views::enumerate(params)) {
		::bind(s, i, param);
	}
	auto result = s.execute();
	return;
}

void source::insert(
	const nanodbc::string& query,
	const db_data& data,
	std::span<const nanodbc::string> columns
) {
	auto count = row_count(data);
	if (count == 0)
		return;
	nanodbc::statement insert_statement(_connection, query);
	auto a = ::bind(insert_statement, data, columns);
	nanodbc::transact(insert_statement, count);
}

source::~source() {
	
}
