#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <nanodbc/nanodbc.h>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include <sqlext.h>

namespace nanodbc {
	auto operator==(const nanodbc::date &a, const nanodbc::date &b) -> bool;

	auto operator==(const nanodbc::time &a, const nanodbc::time &b) -> bool;

	auto operator==(const nanodbc::timestamp &a, const nanodbc::timestamp &b) -> bool;
} // namespace nanodbc

struct case_insensitive_hash {
	auto operator()(const nanodbc::string &s) const -> size_t {
		auto lower_s = s;
		std::ranges::transform(
			lower_s, lower_s.begin(),
			[](unsigned char c) { return std::tolower(c); }
		);
		return std::hash<nanodbc::string>{}(lower_s);
	}
};

struct case_insensitive_equal {
	auto operator()(const nanodbc::string &a, const nanodbc::string &b) const -> bool {
		return std::ranges::equal(
			a, b, [](auto ca, auto cb) {
				return std::tolower(ca) == std::tolower(cb);
			}
		);
	}
};

using db_type = std::variant<
	std::monostate,
	int32_t,
	int64_t,
	uint32_t,
	uint64_t,
	float,
	double,
	nanodbc::string,
	nanodbc::date,
	nanodbc::time,
	nanodbc::timestamp>;

template <typename T>
struct typed_column_ {
	using type = T;
	std::vector<T> data;
	std::vector<unsigned char> nulls;
	auto operator==(const typed_column_ &) const -> bool = default;
};

template <typename Variant>
struct variant_to_columns;

template <typename... Ts>
struct variant_to_columns<std::variant<std::monostate, Ts...>> {
	using type = std::variant<typed_column_<Ts>...>;
};

template <typename T, typename... Ts>
struct variant_to_columns<std::variant<T, Ts...>> {
private:
	using rest = typename variant_to_columns<std::variant<Ts...>>::type;

	template <typename Column, typename RestVariant>
	struct prepend;

	template <typename Column, typename... Rs>
	struct prepend<Column, std::variant<Rs...>> {
		using type = std::variant<Column, Rs...>;
	};

public:
	using type = typename prepend<typed_column_<T>, rest>::type;
};

using db_column_ = variant_to_columns<db_type>::type;

struct db_column {
	db_column_ data;

	db_column() = default;

	db_column(std::initializer_list<const char *> init);

	template <typename T>
	db_column(std::initializer_list<T> init) {
		typed_column_<T> col;
		col.data.assign(init);
		col.nulls.assign(init.size(), 0);
		data = std::move(col);
	}

	auto operator[](size_t i) const -> db_type;

	auto operator==(const db_column &other) const -> bool;

	auto operator==(std::span<const db_type> other) const -> bool;

	[[nodiscard]]
	auto capacity() const -> size_t;

	[[nodiscard]]
	auto size() const -> size_t;
};

using db_data = std::unordered_map<
	nanodbc::string,
	db_column,
	case_insensitive_hash,
	case_insensitive_equal>;

auto operator==(const db_data &lhs, const db_data &rhs) -> bool;

auto db_data_size(const db_data &v) -> size_t;

void filter(
	db_data &data, nanodbc::string column, std::function<bool(db_type &)> fn
);

auto operator|(db_data &data, auto fn) { return fn(data); }

template <typename T>
concept DbBindable = requires(nanodbc::statement &stmt, const T &val, short i) {
	stmt.bind(i, &val);
};

class source {
public:
	source(const nanodbc::string &connection_string);
	static auto from_file(std::filesystem::path filename) -> source;
	auto select(const nanodbc::string &query, std::span<const db_type> params = {}) -> db_data;
	auto select(const nanodbc::string &query, std::initializer_list<db_type> columns) -> db_data;
	auto select_one(const nanodbc::string &query, std::initializer_list<db_type> params) -> db_type;
	auto select_one(const nanodbc::string &query, std::span<const db_type> params = {}) -> db_type;
	void exec(const nanodbc::string &query, std::span<const db_type> params = {});
	void insert(
		const nanodbc::string &query,
		const db_data &data,
		std::span<const nanodbc::string> columns = {},
		size_t batch = 0
	);
	void insert(
		const nanodbc::string &query,
		const db_data &data,
		std::initializer_list<nanodbc::string> columns = {},
		size_t batch = 0
	);
	~source();

protected:
	nanodbc::connection _connection;
};
