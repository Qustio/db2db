#include "source.h"

#include "nanodbc/nanodbc.h"
#include "nanodbc_optional.cpp"
#include <boost/container/vector.hpp>
#include <cstdint>
#include <format>
#include <fstream>
#include <ranges>
#include <spdlog/spdlog.h>
#include <sqlext.h>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace {
	void bind(nanodbc::statement &s, short index, const db_type &value) {
		std::visit(
			[&](const auto &v) -> auto {
				if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::monostate>)
					s.bind_null(index);
				else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>)
					s.bind(index, v.data());
				else
					s.bind(index, &v);
			},
			value
		);
	}

	auto row_count(const db_data &data) -> size_t {
		return std::visit(
			[](const auto &col) -> auto { return col.data.size(); },
			data.begin()->second.data
		);
	}

	auto bind(
		nanodbc::statement &s,
		const db_data &data,
		std::span<const nanodbc::string> columns
	) {
		short index = 0;
		for (const auto &column_name : columns) {
			auto it = data.find(column_name);
			if (it == data.end())
				throw std::runtime_error(std::format("No column named \"{}\"", column_name));
			const auto &column = it->second;
			auto count = row_count(data);

			std::visit(
				[&](const auto &c) {
					using T = std::decay_t<decltype(c)>;
					std::string col_name(column_name.begin(), column_name.end());
					if constexpr (std::is_same_v<typename T::type, nanodbc::string>) {
						size_t max_len = 0;
						for (const auto &str : c.data)
							max_len = (std::max)(max_len, str.size());
						size_t alloc = max_len * count * sizeof(nanodbc::string::value_type);
						spdlog::info("bind [{}]: {} strings, max_len={}, ~{} MB", col_name, count, max_len, alloc / (1024 * 1024));
						s.bind_strings(
							index, c.data,
							reinterpret_cast<const bool *>(c.nulls.data())
						);
					} else {
						spdlog::info("bind [{}]: {} x {} bytes", col_name, count, sizeof(typename T::type));
						s.bind(
							index, c.data.data(), count,
							reinterpret_cast<const bool *>(c.nulls.data())
						);
					}
				},
				column.data
			);
			index++;
		}
	}

	auto get(nanodbc::result &r, short col) -> db_type {
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
		throw std::runtime_error(
			std::format("Type \"{}\" is missing is mapping", r.column_c_datatype(col))
		);
	}

	auto make_column_storage(int c_type) -> db_column_ {
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
} // namespace

auto operator==(const db_data &lhs, const db_data &rhs) -> bool {
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

auto db_data_size(const db_data &v) -> size_t {
	size_t bytes = sizeof(v);
	bytes += v.bucket_count() * sizeof(void *);
	for (const auto &[key, vec] : v) {
		bytes += sizeof(key) + key.capacity() * sizeof(decltype(key)::value_type);
		bytes += vec.capacity();
	}
	return bytes;
}

void filter(
	db_data &data, nanodbc::string column, std::function<bool(db_type &)> fn
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
		values.data
	);
	size_t offset = 0;
	for (auto &[key, col_values] : data) {
		std::visit(
			[&to_delete](auto &data) {
				for (size_t i : to_delete)
					data.nulls[i] = 1;
			},
			col_values.data
		);
	}
}

source::source(const nanodbc::string &connection_string)
	: _connection(connection_string) {};

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
	return {nanodbc::string(cs.begin(), cs.end())};
}

auto source::select(const nanodbc::string &query, std::span<const db_type> params) -> db_data {
	db_data data{};
	auto s = nanodbc::statement(_connection, query);
	for (const auto &[i, param] : std::views::enumerate(params)) {
		::bind(s, i, param);
	}
	auto result = s.execute();
	for (int i = 0; i < result.columns(); i++) {
		data[result.column_name(i)].data =
			make_column_storage(result.column_c_datatype(i));
	}
	while (result.next()) {
		for (short i = 0; i < result.columns(); i++) {
			try {
				auto value = ::get(result, i);
				std::visit(
					[&value](auto &column) {
						using T = typename std::decay_t<decltype(column)>::type;
						auto *val = std::get_if<T>(&value);
						column.nulls.push_back(val ? 0 : 1);
						column.data.push_back(val ? std::move(*val) : T{});
					},
					data[result.column_name(i)].data
				);
			} catch (const std::runtime_error &e) {
				spdlog::error("{}", e.what());
			}
		}
	}
	return data;
}

auto source::select(const nanodbc::string &query, std::initializer_list<db_type> columns) -> db_data {
	return select(query, std::span{columns.begin(), columns.end()});
}

auto source::select_one(const nanodbc::string &query, std::initializer_list<db_type> params) -> db_type {
	return select_one(query, std::span{params.begin(), params.end()});
}
auto source::select_one(const nanodbc::string &query, std::span<const db_type> params) -> db_type {
	db_data data{};
	auto s = nanodbc::statement(_connection, query);
	for (const auto &[i, param] : std::views::enumerate(params)) {
		::bind(s, i, param);
	}
	auto result = s.execute();
	if (!result.next()) {
		return db_type{};
	}
	if (!result.is_null(0)) {
		return get(result, 0);
		// return result.get<nanodbc::string>(0);
	}
	return db_type{};
}

void source::exec(const nanodbc::string &query, std::span<const db_type> params) {
	auto s = nanodbc::statement(_connection, query);
	for (const auto &[i, param] : std::views::enumerate(params)) {
		::bind(s, i, param);
	}
	auto result = s.execute();
	return;
}

void source::insert(
	const nanodbc::string &query,
	const db_data &data,
	std::span<const nanodbc::string> columns
) {
	auto count = row_count(data);
	if (count == 0)
		return;
	nanodbc::statement insert_statement(_connection, query);
	::bind(insert_statement, data, columns);
	nanodbc::transact(insert_statement, count);
}

void source::insert(
	const nanodbc::string &query,
	const db_data &data,
	std::initializer_list<nanodbc::string> columns
) {
	insert(query, data, std::span{columns.begin(), columns.end()});
}

source::~source() = default;

db_column::db_column(std::initializer_list<const char *> init) {
	typed_column_<nanodbc::string> col;
	for (const char *s : init)
		col.data.emplace_back(s);
	col.nulls.assign(init.size(), 0);
	data = std::move(col);
}

auto db_column::operator[](size_t i) const -> db_type {
	return std::visit(
		[i](const auto &data) -> db_type { return data.data[i]; }, data
	);
}

auto db_column::operator==(const db_column &other) const -> bool {
	return data == other.data;
}

auto db_column::operator==(std::span<const db_type> other) const -> bool {
	return std::visit(
		[&](const auto &col) -> bool {
			using T = typename std::decay_t<decltype(col)>::type;
			return std::ranges::all_of(
				std::views::zip_transform(
					[](const T &data, const auto &nulls,
					   const auto &other) -> bool {
						if (nulls)
							return std::holds_alternative<std::monostate>(
								other
							);
						const auto *val = std::get_if<T>(&other);
						return val && *val == data;
					},
					col.data, col.nulls, other
				),
				std::identity{}
			);
			return true;
		},
		data
	);
}

[[nodiscard]]
auto db_column::capacity() const -> size_t {
	return std::visit(
		[](const auto &col) -> size_t {
			using T = typename std::decay_t<decltype(col)>::type;
			auto data_size = col.data.size() + col.data.capacity() * sizeof(T);
			auto nulls_size = col.nulls.size() + col.nulls.capacity() * sizeof(unsigned char);
			// if T is string then add string capacity bytes
			if constexpr (std::is_same_v<T, nanodbc::string>) {
				for (const auto &val : col.data) {
					data_size += val.capacity() * sizeof(nanodbc::string::value_type);
				}
			}
			return data_size + nulls_size;
		},
		data
	);
}

[[nodiscard]]
auto db_column::size() const -> size_t {
	return std::visit(
		[](const auto &data) -> size_t { return data.data.size(); }, data
	);
}

auto nanodbc::operator==(const nanodbc::date &a, const nanodbc::date &b) -> bool {
	return a.year == b.year && a.month == b.month && a.day == b.day;
}

auto nanodbc::operator==(const nanodbc::time &a, const nanodbc::time &b) -> bool {
	return a.hour == b.hour && a.sec == b.sec && a.min == b.min;
}

auto nanodbc::operator==(const nanodbc::timestamp &a, const nanodbc::timestamp &b) -> bool {
	return a.year == b.year && a.month == b.month && a.day == b.day &&
		   a.hour == b.hour && a.sec == b.sec && a.min == b.min;
}
