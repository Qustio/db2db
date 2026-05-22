#include "source.h"

#include <fstream>
#include <ranges>
#include <stdexcept>
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
		const db_value& value
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

	void bind(
		nanodbc::statement& s,
		const db_data& data,
		std::span<const nanodbc::string> columns
	) {
		short index = 0;
		for (const auto& column_name : columns) {
			auto it = data.find(column_name);
			if (it == data.end())
				throw std::runtime_error(std::format("No column named \"{}\"", column_name));
			const auto& column = it->second;
			const size_t count = column.size();

			boost::container::vector<bool> nulls(count);
			for (size_t i = 0; i < count; i++)
				nulls[i] = std::holds_alternative<std::monostate>(column[i]);

			auto first_non_null = std::ranges::find_if(column, [](const db_value& v) {
				return !std::holds_alternative<std::monostate>(v);
			});

			if (first_non_null == column.end()) {
				s.bind_null(index, count);
			} else {
				std::visit([&]<typename T>(const T&) {
					if constexpr (std::is_same_v<T, std::monostate>) {
						s.bind_null(index, count);
					} else if constexpr (std::is_same_v<T, nanodbc::string>) {
						std::vector<nanodbc::string> values(count);
						for (size_t i = 0; i < count; i++)
							if (!nulls[i]) values[i] = std::get<nanodbc::string>(column[i]);
						s.bind_strings(index, values, nulls.data());
					} else {
						std::vector<T> values(count);
						for (size_t i = 0; i < count; i++)
							if (!nulls[i]) values[i] = std::get<T>(column[i]);
						s.bind(index, values.data(), count, nulls.data());
					}
				}, *first_non_null);
			}
			index++;
		}
	}

	db_value get(nanodbc::result& r, short col) {
		if (r.is_null(col))
			return std::monostate{};
		switch (r.column_c_datatype(col)) {
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
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
		throw std::runtime_error(std::format("Type \"{}\" is missing is mappin g", r.column_c_datatype(col)));
	}
}

void filter(db_data& data, nanodbc::string column, std::function<bool(db_value&)> fn) {
	if (data.empty()) return;
	auto values = data[column];
	std::vector<int> to_delete{};
	for (size_t i = 0; i< values.size(); i++) {
		if (!fn(values[i]))
			to_delete.push_back(i);
	}
	size_t offset = 0;
	for (size_t idx : to_delete) {
		for (auto& [key, col_values] : data) {
			col_values.erase(col_values.begin() + idx - offset);
		}
		offset++;
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
	std::span<const db_value> params
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
		data[result.column_name(i)];
		//spdlog::debug("Column {} type: {}", i, result.column_datatype(i));
	}
	while (result.next()) {
		for (short i = 0; i < result.columns(); i++) {
			try {
				auto column_name = result.column_name(i);
				if (!result.is_null(i)) {
					auto value = result.get<nanodbc::string>(i);
					data[column_name].push_back(db_value(value));
				} else {
					data[column_name].push_back({});
				}
			}
			catch (const std::runtime_error& e) {
				spdlog::error("{}", e.what());
			}

		}
	}
	return data;
}

db_value source::select_one(
	const nanodbc::string& query,
	std::span<const db_value> params
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
		return db_value{};
	}
	if (!result.is_null(0)) {
		return get(result, 0);
		//return result.get<nanodbc::string>(0);
	}
	return db_value{};
}

void source::exec(
	const nanodbc::string& query,
	std::span<const db_value> params
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
	if (data.begin()->second.size() == 0)
		return;
	auto count = data.begin()->second.size();
	nanodbc::statement insert_statement(_connection, query);
	::bind(insert_statement, data, columns);
	nanodbc::transact(insert_statement, count);
}

source::~source() {
	
}
