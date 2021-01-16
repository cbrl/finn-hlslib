#pragma once


#define SIZEOF_MEMBER(cls, member) (sizeof(((cls*)0)->member))


struct xorshift64 final {
	uint64_t generate() {
		uint64_t x = state;
		x ^= x << 13;
		x ^= x >> 7;
		x ^= x << 17;
		return state = x;
	}

	uint64_t state;
};


namespace util {
/*
	template<typename T>
	constexpr size_t integer_log2(T n) {
		size_t result = 0;
		while (n >>= 1) ++result;
		return result;
	}
*/

	template<typename T>
	constexpr size_t integer_log2(T n, size_t result = 0) {
		return ((n >> 1) == 0) ? result : integer_log2((n >> 1), result+1);
	}

	template<typename T>
	constexpr size_t ceil_int_log2(T n) {
		// Add 1 if the number isn't a power of 2
		return ((n & (n-1)) == 0) ? integer_log2(n) : (integer_log2(n) + 1);
	}

	template<typename T>
	constexpr T ceil_div(T lhs, T rhs) {
		return (lhs == 0) ? 0 : (T{1} + ((lhs - 1) / rhs));
	}
}
