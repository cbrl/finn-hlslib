/******************************************************************************
 *  Copyright (c) 2019, Xilinx, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1.  Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2.  Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *  3.  Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION). HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

/*******************************************************************************
 *
 *  Authors: Giulio Gambardella <giuliog@xilinx.com>
 *           Thomas B. Preusser <thomas.preusser@utexas.edu>
 *             Marie-Curie Fellow, Xilinx Ireland, Grant Agreement No. 751339
 *           Christoph Doehring <cdoehrin@xilinx.com>
 *
 *  @file activations.hpp
 *
 *  Library of templated HLS classes for BNN deployment. 
 *  This file lists a set of classes used to implement  
 *  threshold memory in neural network. 
 *
 *  This project has received funding from the European Union's Framework
 *  Programme for Research and Innovation Horizon 2020 (2014-2020) under
 *  the Marie Skłodowska-Curie Grant Agreement No. 751339.
 *
 *******************************************************************************/

#ifndef ACTIVATIONS_HPP
#define ACTIVATIONS_HPP

#include <type_traits>
#include "interpret.hpp"
#include "deinterleave.h"

/**
 * General contract for activation functions.
 *
 * This class itself has no formal significance for the implementation
 * of the MVAU. Implementations of activation functions are encouraged
 * to implement it nonetheless to guarantee appropriate function
 * signatures.
 */
template<typename TA, typename TO>
class Activation {
public:
  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

  /**
   * Compute the activation of the passed accumulator value accu in row idx.
   */
  TO activate(unsigned const  nf, unsigned const  pe, TA const &accu) const;
};

/**
 * A no-op activation that simply outputs the computed accumulator
 * output as the final result.
 */
template<typename T>
class PassThroughActivation : public Activation<T, T> {
public:
  T activate(unsigned const  nf, unsigned const  pe, T const &accu) const {
#pragma HLS inline
    return  accu;
  }
};

template<typename T>
class ORAMPassThroughActivation : public Activation<T, T> {
public:
  template<unsigned NF, unsigned NumTH>
  T activate(unsigned const  nf, unsigned const  pe, T const &accu) const {
#pragma HLS inline
    return  accu;
  }
};

/**
 * Use a simple global threshold comparison as activation function.
 *
 * The constant threshold is initialized at construction.
 * The default comparison returns true if the threshold value is
 * smaller than the passed accumulator value.
 */
template<typename TA, typename Compare = std::less<TA>>
class ThresholdActivation : public Activation<TA, bool> {
  TA const  m_threshold;
public:
  ThresholdActivation(TA const &threshold) : m_threshold(threshold) {
#pragma HLS inline
  }

public:
  bool activate(unsigned const  nf, unsigned const  pe, TA const &accu) const {
#pragma HLS inline
    return  Compare()(m_threshold, accu);
  }
};

/**
 * Use a simple per-row threshold comparison as activation function.
 *
 * The thresholds are taken from an array indexed by output row.
 * It is currently public to allow direct initialization and
 * to make its name accessible for top-level HLS pragmas.
 *
 * The default comparison returns true if the threshold value defined for
 * the indexed row is smaller than the passed accumulator value.
 */
template<unsigned NF, unsigned PE, unsigned NumTH, 
	 typename TA, typename TR, int ActVal = 0, typename Compare = std::less<TA>>
class ThresholdsActivation {
public:
  TA m_thresholds[PE][NF][NumTH];
  
public:
  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

public:
  TR activate(unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    TR result=ActVal;
	for(unsigned int i=0; i< NumTH; i++){
#pragma HLS unroll
      result+=Compare()(m_thresholds[pe][nf][i], accu);
    }
    return result;
  }
};


template<typename TA, unsigned NumThresh, typename TR, int ActVal = 0, typename Compare = std::less<TA>>
class ORAMThresholdsActivationBuf {
public:
  TA m_thresholds[NumThresh];

  ORAMThresholdsActivationBuf() {
    #pragma HLS inline
  }
  
public:
  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

public:
  template<unsigned NF, unsigned NumTH>
  TR activate(unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    TR result=ActVal;
	for(unsigned int i=0; i< NumTH; i++){
#pragma HLS unroll
      result+=Compare()(TA(m_thresholds[(pe * NF * NumTH) + (nf * NumTH) + i]), accu);
    }
    return result;
  }
};


template<typename ORAM, typename ATU, unsigned Layer,
  unsigned NF, unsigned PE, unsigned NumTH, typename TA, typename TR,
  int ActVal = 0, typename Compare = std::less<TA>>
class ORAMThresholdsActivation {
public:
  unsigned cached_block;
  typename ORAM::Block cache;

  ORAM& oram;
  const ATU& atu;
  
public:
  ORAMThresholdsActivation(ORAM& oram, const ATU& atu) : oram(oram), atu(atu) {
    #pragma HLS inline
  }

  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

public:
  TR activate(unsigned const  nf, unsigned const  pe,  TA const &accu, uint8_t* server_data) {
#pragma HLS inline
    TR result=ActVal;

    ap_uint<TA::width> val;
    std::pair<size_t, size_t> block_byte;
    const size_t element_size = atu.element_size(Layer);

	  for(unsigned int i=0; i< NumTH; i++){
#pragma HLS unroll
      block_byte = atu.index_to_block(Layer, pe, nf, i);

      if (block_byte.first != cached_block) {
        oram.read(block_byte.first, cache.data(), server_data);
        cached_block = block_byte.first;
      }

      val = 0;
      for (int i = 0; i < element_size; ++i) {
        #pragma HLS pipeline
        val |= ap_uint<TA::width>(cache[block_byte.second + i]) << (i * 8);
      }

      result+=Compare()(*reinterpret_cast<TA*>(&val), accu);
    }
    return result;
  }
};

/*
template<unsigned Layer, unsigned NF, unsigned PE, unsigned NumTH, 
	 typename TA, typename TR, int ActVal = 0, typename Compare = std::less<TA>>
class ORAMThresholdsActivation {
public:
  TA m_thresholds[PE][NF][NumTH];
  
public:
  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

public:
  TR activate(unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    TR result=ActVal;
	for(unsigned int i=0; i< NumTH; i++){
#pragma HLS unroll
      result+=Compare()(m_thresholds[pe][nf][i], accu);
    }
    return result;
  }

  template<typename ORAM, typename ATU>
  void loadParameters(ORAM& oram, const ATU& atu, uint8_t* block_cache, uint8_t* server_data) {
    const size_t element_size = atu.element_size(Layer);
    std::pair<size_t, size_t> block_byte;

    ap_uint<TA::width> val;

    for (unsigned pe = 0; pe < PE; ++pe) {
      for (unsigned nf = 0; nf < NF; ++nf) {
        for (unsigned numth; numth < NumTH; ++numth) {
          block_byte = atu.index_to_block(Layer, pe, nf, numth);
          oram.read(block_byte.first, block_cache, server_data);

          for (size_t i = 0; i < element_size; ++i) {
            #pragma HLS pipeline
            val |= ap_uint<TA::width>(block_cache[block_byte.second + i]) << (i * 8);
          }
          m_thresholds[pe][nf][numth] = *reinterpret_cast<TA*>(&val);
        }
      }
    }
  }
};
*/

template<unsigned NF, unsigned PE, unsigned NumTH, 
	 typename TA, typename TR, int ActVal = 0, typename Compare = std::less<TA>>
class TMRThresholdsActivation {
public:
  TA m_thresholds[3][PE][NF][NumTH];
  
public:
  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

public:
  TR activate(unsigned const  nf, unsigned const  pe,  TA const &accu) {
#pragma HLS inline
    TR result=ActVal;
	for(unsigned int i=0; i< NumTH; i++){
#pragma HLS unroll
      // Get module values
      const TA x = m_thresholds[0][pe][nf][i];
      const TA y = m_thresholds[1][pe][nf][i];
      const TA z = m_thresholds[2][pe][nf][i];

      // Take the common 2 of the 3 values
      const TA thresh = (x & y) | (y & z) | (x & z);

      // Correct potential error
      m_thresholds[0][pe][nf][i] = thresh;
      m_thresholds[1][pe][nf][i] = thresh;
      m_thresholds[2][pe][nf][i] = thresh;

      result+=Compare()(thresh, accu);
    }
    return result;
  }
};


// Interleaving doesn't fully work with odd NF, so
// the last element would not be interleaved.
template<unsigned NF, unsigned PE, unsigned NumTH, 
	 typename TA, typename TR, int ActVal = 0, typename Compare = std::less<TA>>
class InterleavedThresholdsActivation {
  static_assert(NF > 1, "InterleavedThresholdsActivation only works with NF > 1");

public:
  TA m_thresholds[PE][NF][NumTH];

public:
  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

public:
  template<typename T = TR>
  typename std::enable_if<(NF % 2) == 0, T>::type
  activate(unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    TR result=ActVal;
    activate_impl(result, nf, pe, accu);
    return result;
  }

  template<typename T = TR>
  typename std::enable_if<(NF % 2) != 0, T>::type
  activate(unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    TR result=ActVal;
    if (nf < (NF - 1)) {
      activate_impl(result, nf, pe, accu);
    }
    else {
      for(unsigned int i = 0; i < NumTH; ++i) {
#pragma HLS unroll
        result += Compare()(m_thresholds[pe][nf][i], accu);
      }
    }
    return result;
  }

private:
  void activate_impl(TR &result, unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    unsigned const nf0 = nf & (~unsigned(1));

    for(unsigned int i = 0; i < NumTH; ++i) {
  #pragma HLS unroll
      const ap_uint<TA::width> x = *reinterpret_cast<const ap_uint<TA::width>*>(&m_thresholds[pe][nf0][i]);
      const ap_uint<TA::width> y = *reinterpret_cast<const ap_uint<TA::width>*>(&m_thresholds[pe][nf0 + 1][i]);

      const ap_uint<2*TA::width> val = (x, y);

      if ((nf & 1) == 0) {
        const ap_uint<TA::width> thresh = deinterleave(val);
        result += Compare()(*reinterpret_cast<const TA*>(&thresh), accu);
      }
      else {
        const ap_uint<TA::width> thresh = deinterleave(val >> 1);
        result += Compare()(*reinterpret_cast<const TA*>(&thresh), accu);
      }
    }
  }
};


// Interleaving doesn't fully work with odd NF, so
// the last element would not be interleaved.
template<unsigned NF, unsigned PE, unsigned NumTH, 
	 typename TA, typename TR, uint64_t InterleavePattern,
   int ActVal = 0, typename Compare = std::less<TA>>
class ResilientInterleavedThresholdsActivation {
  static_assert(NF > 1, "InterleavedThresholdsActivation only works with NF > 1");

public:
  TA m_thresholds[PE][NF][NumTH];

public:
  TA init(unsigned const  nf, unsigned const  pe) const {
#pragma HLS inline
    return  TA(0);
  }

public:
  template<typename T = TR>
  typename std::enable_if<(NF % 2) == 0, T>::type
  activate(unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    TR result=ActVal;
    activate_impl(result, nf, pe, accu);
    return result;
  }

  template<typename T = TR>
  typename std::enable_if<(NF % 2) != 0, T>::type
  activate(unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    TR result=ActVal;
    if (nf < (NF - 1)) {
      activate_impl(result, nf, pe, accu);
    }
    else {
      for(unsigned int i = 0; i < NumTH; ++i) {
#pragma HLS unroll
        result += Compare()(m_thresholds[pe][nf][i], accu);
      }
    }
    return result;
  }

private:
  void activate_impl(TR &result, unsigned const  nf, unsigned const  pe,  TA const &accu) const {
#pragma HLS inline
    unsigned const nf0 = nf & (~unsigned(1));

    for(unsigned int i = 0; i < NumTH; ++i) {
  #pragma HLS unroll
      const ap_uint<TA::width> x = *reinterpret_cast<const ap_uint<TA::width>*>(&m_thresholds[pe][nf0][i]);
      const ap_uint<TA::width> y = *reinterpret_cast<const ap_uint<TA::width>*>(&m_thresholds[pe][nf0 + 1][i]);

      const ap_uint<2*TA::width> val = (x, y);//(x << TA::width) | y;

      if ((nf & 1) == 0) {
        const ap_uint<TA::width> thresh = deinterleave_pattern(val, ap_uint<2*TA::width>(InterleavePattern));
        result += Compare()(*reinterpret_cast<const TA*>(&thresh), accu);
      }
      else {
        ap_uint<TA::width> thresh = deinterleave_pattern(val, ap_uint<2*TA::width>(~InterleavePattern));
        thresh.reverse();
        result += Compare()(*reinterpret_cast<const TA*>(&thresh), accu);
      }
    }
  }
};


template<size_t Layer, unsigned NF, unsigned PE, unsigned NumTH, typename TA, typename Thresholds, typename ORAM, typename ATU>
void loadORAMThresholds(Thresholds& thresh, ORAM& oram, const ATU& atu, uint8_t* block_cache, uint8_t* server_data) {
  const size_t element_size = atu.element_size(Layer);
  std::pair<size_t, size_t> block_byte;

  ap_uint<TA::width> val;

  for (unsigned pe = 0; pe < PE; ++pe) {
    for (unsigned nf = 0; nf < NF; ++nf) {
      for (unsigned numth; numth < NumTH; ++numth) {
        block_byte = atu.index_to_block(Layer, pe, nf, numth);
        oram.read(block_byte.first, block_cache, server_data);

        for (size_t i = 0; i < element_size; ++i) {
          #pragma HLS pipeline
          val |= ap_uint<TA::width>(block_cache[block_byte.second + i]) << (i * 8);
        }
        thresh.m_thresholds[pe][nf][numth] = *reinterpret_cast<TA*>(&val);
      }
    }
  }
}

/**
 * \brief Thresholding function for multiple images
 *
 * The function performs thresholds comparison with input activation vector, 
 * and generating output based on the comparison results
 *
 * \tparam ImgDim         Width and Heigth of the Input Feature Map (assumed square)
 * \tparam NumChannels    Heigth of the input matrix
 * \tparam PE             Number of output rows computed in parallel
 * \tparam TSrcI          DataType of the input activation (as used in the MAC)
 * \tparam TDstI          DataType of the output activation (as generated by the activation)
 * \tparam TI             DataType of the input stream - safely deducible from the paramaters
 * \tparam TO             DataType of the output stream - safely deducible from the paramaters
 * \tparam TA             DataType of the activation class (e.g. thresholds) - safely deducible from the paramaters
 *
 * \param in              Input stream
 * \param out             Output stream
 * \param activation      Activation class
 * \param reps            Number of time the function has to be repeatedly executed (e.g. number of images)
 */
template <
    unsigned ImgDim, unsigned NumChannels, unsigned PE,
    typename TSrcI = Identity, typename TDstI = Identity,
    typename TI, typename TO, typename TA>
void Thresholding_Batch(hls::stream<TI> &in,
                        hls::stream<TO> &out,
                        TA const &activation,
                        int const reps)
{

  // how many different rows each neuron will compute
  // alternatively: number of vertical matrix chunks
  unsigned const NF = NumChannels / PE;

  unsigned nf = 0;
  unsigned tile = 0; // invariant: tile = nf*SF + sf

  // everything merged into a common iteration space (one "big" loop instead
  // of smaller nested loops) to get the pipelinening the way we want
  for (unsigned i = 0; i < reps * ImgDim * ImgDim * NF; i++)
  {
    TI inElem;
    inElem = in.read();
    auto outElem = TDstI().template operator()<TO>();
    for (unsigned pe = 0; pe < PE; pe++)
    {
#pragma HLS UNROLL
      auto const act = TSrcI()(inElem);
      outElem[pe] = activation.activate(nf, pe, act[pe]);
    }
    out.write(outElem);
    if (++nf == NF)
    {
      nf = 0;
    }
  }
}
#endif
