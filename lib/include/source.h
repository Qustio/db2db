#include <cstddef>
#include <functional>
#include <initializer_list>
#include <filesystem>
#include <type_traits>
#include <variant>
#include <algorithm>
#include <span>
#include <format>
#include <ranges>
#include <nanodbc/nanodbc.h>
#include <vector>
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
#endif
#include <sqlext.h>


namespace nanodbc {
	inline bool operator==(const nanodbc::date& a, const nanodbc::date& b) {
		return a.year == b.year && a.month == b.month && a.day == b.day;
	}

	inline bool operator==(const nanodbc::time& a, const nanodbc::time& b) {
		return a.hour == b.hour && a.sec == b.sec && a.min == b.min;
	}

	inline bool operator==(const nanodbc::timestamp& a, const nanodbc::timestamp& b) {
		return a.year == b.year
			&& a.month == b.month
			&& a.day == b.day
			&& a.hour == b.hour
			&& a.sec == b.sec
			&& a.min == b.min;
	}
}

struct case_insensitive_hash {
	size_t operator()(const nanodbc::string& s) const {
		auto lower_s = s;
		std::transform(
			lower_s.begin(),
			lower_s.end(),
			lower_s.begin(),
			[](unsigned char c) { return std::tolower(c); }
		);
		return std::hash<nanodbc::string>{}(lower_s);
	}
};

struct case_insensitive_equal {
	bool operator()(const nanodbc::string& a, const nanodbc::string& b) const {
		return std::equal(
			a.begin(), a.end(),
			b.begin(), b.end(),
			[](auto ca, auto cb) {
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
	nanodbc::timestamp
>;


template<typename T>
struct typed_column_{
	using type = T;
	std::vector<T> data;
	std::vector<unsigned char> nulls;
	auto operator==(const typed_column_&) const -> bool = default;
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

	// Prepend typed_column<T> to the rest variant
	template<typename Column, typename RestVariant>
	struct prepend;

	template<typename Column, typename... Rs>
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

	db_column(std::initializer_list<const char*> init) {
		typed_column_<nanodbc::string> col;
		for (const char* s : init)
			col.data.emplace_back(s);
		col.nulls.assign(init.size(), 0);
		data = std::move(col);
	}

	template<typename T>
	db_column(std::initializer_list<T> init) {
		typed_column_<T> col;
		col.data.assign(init);
		col.nulls.assign(init.size(), 0);
		data = std::move(col);
	}

	auto operator[](size_t i ) const -> db_type {
		return std::visit([i](const auto& data) -> db_type {
			return data.data[i];
		}, data);
	}
	
	auto operator==(const db_column& other) const -> bool {
		return data == other.data;
	}

	auto operator==(std::span<const db_type> other) const -> bool {
		return std::visit([&](const auto& col) -> bool {
			using T = typename std::decay_t<decltype(col)>::type;
			return std::ranges::all_of(
				std::views::zip_transform([] (const T& data, const auto& nulls, const auto& other) -> bool {
					if (nulls)
						return std::holds_alternative<std::monostate>(other);
					const auto* val = std::get_if<T>(&other);
					return val && *val == data;
				}, col.data, col.nulls, other
				),
				std::identity{}
			);
			return true;
		}, data);
	}

	[[nodiscard]]
	auto size() const -> size_t {
		return std::visit([](const auto& data) -> size_t {
			return data.data.size();
		}, data);
	}
};

using db_data = std::unordered_map<nanodbc::string, db_column, case_insensitive_hash, case_insensitive_equal>;

bool operator==(const db_data &lhs, const db_data &rhs);

// template<>
// struct std::formatter<db_value> {
// 	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

// 	auto format(const db_value& v, std::format_context& ctx) const {
// 		return std::visit([&](const auto& val) -> std::format_context::iterator {
// 			using T = std::decay_t<decltype(val)>;
// 			if constexpr (std::is_same_v<T, std::monostate>)
// 				return std::format_to(ctx.out(), "NULL");
// 			else if constexpr (std::is_same_v<T, nanodbc::string>)
// 				return std::format_to(ctx.out(), "{}", std::string(val.begin(), val.end()));
// 			else if constexpr (std::is_same_v<T, std::monostate>)
// 				return std::format_to(ctx.out(), "NULL");
// 			else if constexpr (std::is_same_v<T, nanodbc::date>)
// 				return std::format_to(
// 					ctx.out(),
// 					"{}-{:02}-{:02}",
// 					val.year, val.month, val.day
// 				);
// 			else if constexpr (std::is_same_v<T, nanodbc::time>)
// 				return std::format_to(
// 					ctx.out(),
// 					"{:02}:{:02}:{:02}",
// 					val.hour, val.min, val.sec
// 				);
// 			else if constexpr (std::is_same_v<T, nanodbc::timestamp>)
// 				return std::format_to(
// 					ctx.out(),
// 					"{}-{:02}-{:02} {:02}:{:02}:{:02}",
// 					val.year, val.month, val.day, val.hour, val.min, val.sec
// 				);
// 			else
// 				return std::vformat_to(ctx.out(), {}, std::make_format_args(val));
// 		}, v);
// 	}
// };

void filter(db_data& data, nanodbc::string column, std::function<bool(db_type&)> fn);

auto operator|(db_data& data, auto fn) {
	return fn(data);
}

template<typename T>
concept DbBindable = requires(nanodbc::statement & stmt, const T & val, short i) {
	stmt.bind(i, &val);
};

class source {
public:
	source(const nanodbc::string& connection_string);
	static source from_file(std::filesystem::path filename);
	db_data select(
		const nanodbc::string& query,
		std::span<const db_type> params = {}
	);
	db_data select(
		const nanodbc::string& query,
		std::initializer_list<db_type> columns
	) {
		return select(query, std::span{columns.begin(), columns.end()});
	}
	db_type select_one(
		const nanodbc::string& query,
		std::initializer_list<db_type> params
	) {
		return select_one(query, std::span{params.begin(), params.end()});
	}
	db_type select_one(
		const nanodbc::string& query,
		std::span<const db_type> params = {}
	);
	void exec(
		const nanodbc::string& query,
		std::span<const db_type> params = {}
	);
	void insert(
		const nanodbc::string& query,
		const db_data& data,
		std::span<const nanodbc::string> columns
	);
	void insert(
		const nanodbc::string& query,
		const db_data& data,
		std::initializer_list<nanodbc::string> columns
	) {
		insert(query, data, std::span{columns.begin(), columns.end()});
	}
	~source();
protected:
	nanodbc::connection _connection;
};
