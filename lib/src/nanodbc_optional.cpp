#include <optional>
#include <nanodbc/nanodbc.h>

template<typename ColType>
concept ValidColType =
	std::convertible_to<ColType, short> ||
	std::convertible_to<ColType, const nanodbc::string&>;

template<typename T>
concept HasCStr = requires(const T & t) {
	{ t.c_str() } -> std::convertible_to<const char*>;
};

template<typename T, ValidColType ColType>
std::optional<T> get_optional(const nanodbc::result& result, ColType column) {
	if (result.is_null(column))
		return std::nullopt;
	else
		return result.get<T>(column);
}

template<typename T, ValidColType ColType>
void bind_optional(nanodbc::statement& statement, ColType column, const std::optional<T>& value) {
	if (value.has_value()) {
		if constexpr (HasCStr<T>) {
			const auto* str_ptr = value->c_str();
			statement.bind(column, str_ptr);
		}
		else {
			statement.bind(column, &(*value));
		}
	} else {
		statement.bind_null(column);
	}
}