#include <initializer_list>
#include <string_view>
#include <filesystem>
#include <variant>
#include <algorithm>
#include <span>
#include <nanodbc/nanodbc.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
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

using db_value = std::variant<
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
using db_data = std::unordered_map<nanodbc::string, std::vector<db_value>, case_insensitive_hash, case_insensitive_equal>;

void filter(db_data& data, nanodbc::string column, std::function<bool(db_value&)> fn);

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
		std::span<const db_value> params = {}
	);
	db_data select(
		const nanodbc::string& query,
		std::initializer_list<db_value> columns
	) {
		return select(query, std::span{columns.begin(), columns.end()});
	}
	db_value select_one(
		const nanodbc::string& query,
		std::initializer_list<db_value> params
	) {
		return select_one(query, std::span{params.begin(), params.end()});
	}
	db_value select_one(
		const nanodbc::string& query,
		std::span<const db_value> params = {}
	);
	void exec(
		const nanodbc::string& query,
		std::span<const db_value> params = {}
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