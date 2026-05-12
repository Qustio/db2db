#include <functional>

template<typename T>
class scope_guard {
private:
	T value_;
	std::function<void(const T&)> cleanup_;
public:
	scope_guard(T value, std::function<void(const T&)> cleanup)
		: value_(std::move(value)), cleanup_(std::move(cleanup)) {}

	scope_guard(const scope_guard&) = delete;
	scope_guard& operator=(const scope_guard&) = delete;
	scope_guard(scope_guard&&) = default;
	scope_guard& operator=(scope_guard&&) = default;

	~scope_guard() {
		if (cleanup_) {
			cleanup_(value_);
		}
	}

	const T& get() const {
		return value_;
	}

	T& get() {
		return value_;
	}
};