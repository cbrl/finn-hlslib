#ifndef DEINTERLEAVE_HPP
#define DEINTERLEAVE_HPP

#include <ap_int.h>
#include <type_traits>
#include <climits>


template<template<int> class APType, int N>
typename std::enable_if<N == 2, APType<N>>::type
deinterleave(APType<N> input) {
#pragma HLS inline
    return static_cast<ap_int<1>>(input);
}


template<template<int> class APType, int N>
typename std::enable_if<(N > 2) && (N <= 4), APType<N>>::type
deinterleave(APType<N> input) {
#pragma HLS inline
    input = input & 0x5;
    input = (input | (input >> 1)) & 0x3;

    return input;
}


template<template<int> class APType, int N>
typename std::enable_if<(N > 4) && (N <= 8), APType<N>>::type
deinterleave(APType<N> input) {
#pragma HLS inline
    input = input & 0x55;
    input = (input | (input >> 1)) & 0x33;
    input = (input | (input >> 2)) & 0x0F;

    return input;
}


template<template<int> class APType, int N>
typename std::enable_if<(N > 8) && (N <= 16), APType<N>>::type
deinterleave(APType<N> input) {
#pragma HLS inline
    input = input & 0x5555;
    input = (input | (input >> 1)) & 0x3333;
    input = (input | (input >> 2)) & 0x0F0F;
    input = (input | (input >> 4)) & 0x00FF;

    return input;
}


template<template<int> class APType, int N>
typename std::enable_if<(N > 16) && (N <= 32), APType<N>>::type
deinterleave(APType<N> input) {
#pragma HLS inline
    input = input & 0x55555555;
    input = (input | (input >> 1)) & 0x33333333;
    input = (input | (input >> 2)) & 0x0F0F0F0F;
    input = (input | (input >> 4)) & 0x00FF00FF;
    input = (input | (input >> 8)) & 0x0000FFFF;

    return input;
}


template<template<int> class APType, int N>
typename std::enable_if<(N > 32) && (N <= 64), APType<N>>::type
deinterleave(APType<N> input) {
#pragma HLS inline
    input = input & 0x5555555555555555;
    input = (input | (input >> 1))  & 0x3333333333333333;
    input = (input | (input >> 2))  & 0x0F0F0F0F0F0F0F0F;
    input = (input | (input >> 4))  & 0x00FF00FF00FF00FF;
    input = (input | (input >> 8))  & 0x0000FFFF0000FFFF;
    input = (input | (input >> 16)) & 0x00000000FFFFFFFF;

    return input;
}


//TODO: Get template version of this working
ap_fixed<24, 16> deinterleave(ap_fixed<24, 16> input) {
#pragma HLS inline
    //ap_uint<24> converted = input.range();
    //ap_fixed<24, 16> deinterleaved;
    //deinterleaved.range() = deinterleave(converted);
    //return deinterleaved;
    
    ap_uint<24> output = deinterleave(*reinterpret_cast<ap_uint<24>*>(&input));
    return *reinterpret_cast<ap_fixed<24, 16>*>(&output);
}

#endif //DEINTERLEAVE_HPP