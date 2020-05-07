#ifndef DEINTERLEAVE_HPP
#define DEINTERLEAVE_HPP

#include <ap_int.h>
#include <type_traits>
#include <climits>


//--------------------------------------------------------------------------------
// Deinterleaving Bits
//--------------------------------------------------------------------------------
//
// These functions return the value interleaved in bits [0, 2, 4, 6, etc...]
//
//--------------------------------------------------------------------------------

template<template<int> class APType, int N>
typename std::enable_if<N == 2, APType<N>>::type
deinterleave(APType<N> value) {
#pragma HLS inline
    return static_cast<ap_int<1>>(value);
}

template<template<int> class APType, int N>
typename std::enable_if<(N > 2) && (N <= 4), APType<N>>::type
deinterleave(APType<N> value) {
#pragma HLS inline
    value = value & 0x5;
    value = (value | (value >> 1)) & 0x3;

    return value;
}

template<template<int> class APType, int N>
typename std::enable_if<(N > 4) && (N <= 8), APType<N>>::type
deinterleave(APType<N> value) {
#pragma HLS inline
    value = value & 0x55;
    value = (value | (value >> 1)) & 0x33;
    value = (value | (value >> 2)) & 0x0F;

    return value;
}

template<template<int> class APType, int N>
typename std::enable_if<(N > 8) && (N <= 16), APType<N>>::type
deinterleave(APType<N> value) {
#pragma HLS inline
    value = value & 0x5555;
    value = (value | (value >> 1)) & 0x3333;
    value = (value | (value >> 2)) & 0x0F0F;
    value = (value | (value >> 4)) & 0x00FF;

    return value;
}

template<template<int> class APType, int N>
typename std::enable_if<(N > 16) && (N <= 32), APType<N>>::type
deinterleave(APType<N> value) {
#pragma HLS inline
    value = value & 0x55555555;
    value = (value | (value >> 1)) & 0x33333333;
    value = (value | (value >> 2)) & 0x0F0F0F0F;
    value = (value | (value >> 4)) & 0x00FF00FF;
    value = (value | (value >> 8)) & 0x0000FFFF;

    return value;
}

template<template<int> class APType, int N>
typename std::enable_if<(N > 32) && (N <= 64), APType<N>>::type
deinterleave(APType<N> value) {
#pragma HLS inline
    value = value & 0x5555555555555555;
    value = (value | (value >> 1))  & 0x3333333333333333;
    value = (value | (value >> 2))  & 0x0F0F0F0F0F0F0F0F;
    value = (value | (value >> 4))  & 0x00FF00FF00FF00FF;
    value = (value | (value >> 8))  & 0x0000FFFF0000FFFF;
    value = (value | (value >> 16)) & 0x00000000FFFFFFFF;

    return value;
}


template<template<int> class APType, int N>
APType<N/2> deinterleave_pattern(APType<N> value, ap_uint<N> pattern) {
#pragma HLS inline
    APType<N/2> output = 0;
    int out_bit = 0;

    for (int i = 0; i < N; ++i) {
#pragma HLS unroll
        if (pattern[i]) {
            output[out_bit] = value[i];
            out_bit += 1;
        }
    }

    return output;
}

#endif //DEINTERLEAVE_HPP
