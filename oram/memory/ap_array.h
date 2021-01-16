#pragma once

#include <assert.h>
#include <stddef.h>

template<typename T, size_t N>
struct ap_array {
    using value_type      = T;
    using size_type       = size_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    ap_array() = default;
    ap_array(const ap_array&) = default;
    ap_array(ap_array&&) noexcept = default;

    ~ap_array() = default;

    ap_array& operator=(const ap_array&) = default;
    ap_array& operator=(ap_array&&) noexcept = default;

    reference operator[](size_type pos) {
        #pragma HLS inline
        return _data[pos];
    }
    const_reference operator[](size_type pos) const {
        #pragma HLS inline
        return _data[pos];
    }

    reference at(size_type pos) {
        #pragma HLS inline
        assert(pos < size());
        return _data[pos];
    }
    const_reference at(size_type pos) const {
        #pragma HLS inline
        assert(pos < size());
        return _data[pos];
    }

    reference front() {
        #pragma HLS inline
        return _data[0];
    }
    const_reference front() const {
        #pragma HLS inline
        return _data[0];
    }

    reference back() {
        #pragma HLS inline
        return _data[size()-1];
    }
    const_reference back() const {
        #pragma HLS inline
        return _data[size()-1];
    }

    pointer data() {
        #pragma HLS inline
        return _data;
    }
    const_pointer data() const {
        #pragma HLS inline
        return _data;
    }

    constexpr size_type size() const noexcept {
        return N;
    }

    void fill(const T& value) {
        //#pragma HLS ARRAY_PARTITION variable=_data complete dim=1
        for (size_type i = 0; i < size(); ++i) {
            #pragma HLS UNROLL
            _data[i] = value;
        }
    }

    value_type _data[N];
};