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
 *******************************************************************************/

/*******************************************************************************
 *
 *  Authors: Giulio Gambardella <giuliog@xilinx.com>
 *           Thomas B. Preusser <thomas.preusser@utexas.edu>
 *             Marie-Curie Fellow, Xilinx Ireland, Grant Agreement No. 751339
 *           Christoph Doehring <cdoehrin@xilinx.com>
 *
 *  @file weights.hpp
 *
 *  Library of templated HLS classes for BNN deployment. 
 *  This file lists a set of classes used to implement  
 *  weights in neural network. 
 *
 *  This project has received funding from the European Union's Framework
 *  Programme for Research and Innovation Horizon 2020 (2014-2020) under
 *  the Marie Skłodowska-Curie Grant Agreement No. 751339.
 *
 *******************************************************************************/

#ifndef WEIGHTS_HPP
#define WEIGHTS_HPP

#include <ap_int.h>
#include <array>
#include <type_traits>
#include "deinterleave.h"


/**
 * \brief      A binary weight storage adapter that translates the internal 
 * organization optimized for storage to the generalized access by the MVAU.
 *
 * \tparam     SIMD   Number of input columns (channels) computed in parallel
 * \tparam     PE     Number of output rows (channels) computed in parallel
 * \tparam     TILES  3rd dimension of the weights matrix
 */
template<unsigned SIMD, unsigned PE, unsigned TILES>
class BinaryWeights {
 public:
  ap_uint<SIMD>  m_weights[PE][TILES];

 private:
  /**
   * Temporary container for the tile index to implement the
   * memory access in pe -> tile order.
   */
  class TileIndex {
    BinaryWeights const &m_par;
    unsigned      const  m_idx;

   public:
    TileIndex(BinaryWeights const &par, unsigned const  idx)
      : m_par(par), m_idx(idx) {
#pragma HLS inline
    }

   public:
    ap_uint<SIMD> operator[](unsigned const  pe) const {
#pragma HLS inline
      return  m_par.m_weights[pe][m_idx];
    }
  };

 public:
  TileIndex weights(unsigned const  tile) const {
#pragma HLS inline
    return  TileIndex(*this, tile);
  }
};


template<unsigned WeightSize, unsigned NumWeights>
class ORAMBinaryWeightsBuf {
public:
  ap_uint<WeightSize> m_weights[NumWeights];

  ORAMBinaryWeightsBuf() {
    #pragma HLS inline
  }

 private:
  class TileIndex {
    ORAMBinaryWeightsBuf const &m_par;
    unsigned             const  m_idx;

   public:
    TileIndex(ORAMBinaryWeightsBuf const &par, unsigned const  idx)
      : m_par(par), m_idx(idx) {
#pragma HLS inline
    }

   public:
    template<unsigned SIMD, unsigned TILES>
    ap_uint<SIMD> get(unsigned const  pe) const {
#pragma HLS inline
      return ap_uint<SIMD>(m_par.m_weights[(pe * TILES) + m_idx]);
    }
  };

 public:
  TileIndex weights(unsigned const  tile) const {
#pragma HLS inline
    return  TileIndex(*this, tile);
  }
};


template<typename ORAM, typename ATU, unsigned Layer, unsigned SIMD, unsigned PE, unsigned TILES>
class ORAMBinaryWeights {
 public:
  unsigned cached_block;
  typename ORAM::Block cache;

  ORAM& oram;
  const ATU& atu;

  ORAMBinaryWeights(ORAM& oram, const ATU& atu) : oram(oram), atu(atu) {
    #pragma HLS inline
  }

 private:
  /**
   * Temporary container for the tile index to implement the
   * memory access in pe -> tile order.
   */
  class TileIndex {
    ORAMBinaryWeights &m_par;
    unsigned const     m_idx;

   public:
    TileIndex(ORAMBinaryWeights &par, unsigned const idx)
      : m_par(par), m_idx(idx) {
#pragma HLS inline
    }

   public:
    ap_uint<SIMD> get(unsigned const  pe, uint8_t* server_data) const {
#pragma HLS inline
      const std::pair<size_t, size_t> block_byte = m_par.atu.index_to_block(Layer, pe, m_idx);
      const size_t element_size = m_par.atu.element_size(Layer);

      if (block_byte.first != m_par.cached_block) {
        m_par.oram.read(block_byte.first, m_par.cache.data(), server_data);
        m_par.cached_block = block_byte.first;
      }

      ap_uint<SIMD> val = 0;
      for (size_t i = 0; i < element_size; ++i) {
        #pragma HLS pipeline
        val |= ap_uint<SIMD>(m_par.cache[block_byte.second + i]) << (i * 8);
      }

      return val;
    }
  };

 public:
  TileIndex weights(unsigned const  tile) {
#pragma HLS inline
    return  TileIndex(*this, tile);
  }
};

//template<unsigned Layer, unsigned SIMD, unsigned PE, unsigned TILES>
//class ORAMBinaryWeights {
// public:
//  ap_uint<SIMD>  m_weights[PE][TILES];
//
// private:
//  /**
//   * Temporary container for the tile index to implement the
//   * memory access in pe -> tile order.
//   */
//  class TileIndex {
//    ORAMBinaryWeights const &m_par;
//    unsigned          const  m_idx;
//
//   public:
//    TileIndex(ORAMBinaryWeights const &par, unsigned const  idx)
//      : m_par(par), m_idx(idx) {
//#pragma HLS inline
//    }
//
//   public:
//    ap_uint<SIMD> operator[](unsigned const  pe) const {
//#pragma HLS inline
//      return  m_par.m_weights[pe][m_idx];
//    }
//  };
//
// public:
//  TileIndex weights(unsigned const  tile) const {
//#pragma HLS inline
//    return  TileIndex(*this, tile);
//  }
//
//  template<typename ORAM, typename ATU>
//  void loadParameters(ORAM& oram, const ATU& atu, uint8_t* block_cache, uint8_t* server_data) {
//    const size_t element_size = atu.element_size(Layer);
//    std::pair<size_t, size_t> block_byte;
//
//    for (unsigned pe = 0; pe < PE; ++pe) {
//      for (unsigned tile = 0; tile < TILES; ++tile) {
//        block_byte = atu.index_to_block(Layer, pe, tile);
//        oram.read(block_byte.first, block_cache, server_data);
//
//        m_weights[pe][tile] = 0;
//        for (size_t i = 0; i < element_size; ++i) {
//          #pragma HLS pipeline
//          m_weights[pe][tile] |= ap_uint<SIMD>(block_cache[block_byte.second + i]) << (i * 8);
//        }
//      }
//    }
//  }
//};


template<unsigned SIMD, unsigned PE, unsigned TILES>
class TMRBinaryWeights {
 public:
  ap_uint<SIMD>  m_weights[3][PE][TILES];

 private:
  /**
   * Temporary container for the tile index to implement the
   * memory access in pe -> tile order.
   */
  class TileIndex {
    TMRBinaryWeights &m_par;
    unsigned         const  m_idx;

   public:
    TileIndex(TMRBinaryWeights &par, unsigned const  idx)
      : m_par(par), m_idx(idx) {
#pragma HLS inline
    }

   public:
    ap_uint<SIMD> operator[](unsigned const  pe) {
#pragma HLS inline
      // Get the module values
      const ap_uint<SIMD> x = m_par.m_weights[0][pe][m_idx];
      const ap_uint<SIMD> y = m_par.m_weights[1][pe][m_idx];
      const ap_uint<SIMD> z = m_par.m_weights[2][pe][m_idx];

      // Take the common 2 of the 3 values
      const ap_uint<SIMD> val = (x & y) | (y & z) | (x & z);

      // Correct potential error
      m_par.m_weights[0][pe][m_idx] = val;
      m_par.m_weights[1][pe][m_idx] = val;
      m_par.m_weights[2][pe][m_idx] = val;

      return val;
    }
  };

 public:
  TileIndex weights(unsigned const  tile) {
#pragma HLS inline
    return  TileIndex(*this, tile);
  }
};


/**
 * \brief      A fixeed point weight storage adapter that translates the internal 
 * organization optimized for storage to the generalized access by the MVAU.
 *
 * \tparam     SIMD   Number of input columns (channels) computed in parallel
 * \tparam     WT     Datatype of the weights
 * \tparam     PE     Number of output rows (channels) computed in parallel
 * \tparam     TILES  3rd dimension of the weights matrix
 */
template<unsigned SIMD, typename WT ,unsigned PE, unsigned TILES>
class FixedPointWeights {
 public:
  ap_uint<SIMD*WT::width>  m_weights[PE][TILES];

 private:
  /**
   * Temporary container for the tile index to implement the
   * memory access in pe -> tile order.
   */
  class TileIndex {
    FixedPointWeights const &m_par;
    unsigned          const  m_idx;

   public:
    TileIndex(FixedPointWeights const &par, unsigned const  idx)
      : m_par(par), m_idx(idx) {
#pragma HLS inline
    }

   public:
    std::array<WT,SIMD> operator[](unsigned const  pe) const {
#pragma HLS inline
      std::array<WT,SIMD> temp;
	  for(unsigned int i=0; i<SIMD; i++) {
#pragma HLS unroll
        ap_int<WT::width> local_temp;
        local_temp = m_par.m_weights[pe][m_idx]((i+1)*WT::width-1, i*WT::width);
        WT value = *reinterpret_cast<WT*>(&local_temp);
        temp[i] = value;
      }
      return  temp;
    }
  };

 public:
  TileIndex weights(unsigned const  tile) const {
#pragma HLS inline
    return  TileIndex(*this, tile);
  }
};

template<unsigned SIMD, typename WT ,unsigned PE, unsigned TILES>
class TMRFixedPointWeights {
 public:
  ap_uint<SIMD*WT::width>  m_weights[3][PE][TILES];

 private:
  /**
   * Temporary container for the tile index to implement the
   * memory access in pe -> tile order.
   */
  class TileIndex {
    TMRFixedPointWeights &m_par;
    unsigned             const  m_idx;

   public:
    TileIndex(TMRFixedPointWeights &par, unsigned const  idx)
      : m_par(par), m_idx(idx) {
#pragma HLS inline
    }

   public:
    std::array<WT,SIMD> operator[](unsigned const  pe) {
#pragma HLS inline
      std::array<WT,SIMD> temp;
      
      // Get the module values
      const ap_uint<SIMD*WT::width> x = m_par.m_weights[0][pe][m_idx];
      const ap_uint<SIMD*WT::width> y = m_par.m_weights[1][pe][m_idx];
      const ap_uint<SIMD*WT::width> z = m_par.m_weights[2][pe][m_idx];

      // Take the common 2 of the 3 values
      const ap_uint<SIMD*WT::width> val = (x & y) | (y & z) | (x & z);

      // Correct potential error
      m_par.m_weights[0][pe][m_idx] = val;
      m_par.m_weights[1][pe][m_idx] = val;
      m_par.m_weights[2][pe][m_idx] = val;

	  for(unsigned int i=0; i<SIMD; i++) {
#pragma HLS unroll
        ap_int<WT::width> local_temp;
        local_temp = val((i+1)*WT::width-1, i*WT::width);
        WT value = *reinterpret_cast<WT*>(&local_temp);
        temp[i] = value;
      }
      return  temp;
    }
  };

 public:
  TileIndex weights(unsigned const  tile) {
#pragma HLS inline
    return  TileIndex(*this, tile);
  }
};

// Interleaving doesn't fully work with odd TILES, so
// the last element would not be interleaved.
template<unsigned SIMD, typename WT ,unsigned PE, unsigned TILES>
class InterleavedFixedPointWeights {
  static_assert(TILES > 1, "InterleavedFixedPointWeights only works with TILES > 1");

 public:
  ap_uint<SIMD*WT::width>  m_weights[PE][TILES];

 private:
  /**
   * Temporary container for the tile index to implement the
   * memory access in pe -> tile order.
   */
  class TileIndex {
    InterleavedFixedPointWeights const &m_par;
    unsigned          const  m_idx;

   public:
    TileIndex(InterleavedFixedPointWeights const &par, unsigned const  idx)
      : m_par(par), m_idx(idx) {
#pragma HLS inline
    }

   public:
    std::array<WT,SIMD> operator[](unsigned const  pe) const {
#pragma HLS inline
      std::array<WT,SIMD> temp;
	  for(unsigned int i=0; i<SIMD; i++) {
#pragma HLS unroll
        const ap_uint<SIMD*WT::width> weight = get_weight(pe);

        ap_int<WT::width> local_temp;
        local_temp = weight((i+1)*WT::width-1, i*WT::width);
        WT value = *reinterpret_cast<WT*>(&local_temp);
        temp[i] = value;
      }
      return  temp;
    }

   private:
    template<typename T = ap_uint<SIMD*WT::width>>
    typename std::enable_if<(TILES % 2) == 0, T>::type
    get_weight(unsigned const  pe) const {
#pragma HLS inline
      return interleaved_weight(pe);
    }

    template<typename T = ap_uint<SIMD*WT::width>>
    typename std::enable_if<(TILES % 2) != 0, T>::type
    get_weight(unsigned const  pe) const {
#pragma HLS inline
      return (m_idx < (TILES - 1)) ? interleaved_weight(pe) : m_par.m_weights[pe][m_idx];
    }

    ap_uint<SIMD*WT::width> interleaved_weight(unsigned const  pe) const {
#pragma HLS inline
      const unsigned idx0 = m_idx & (~unsigned(1));
      const ap_uint<SIMD*WT::width> x = m_par.m_weights[pe][idx0];
      const ap_uint<SIMD*WT::width> y = m_par.m_weights[pe][idx0 + 1];

      const ap_uint<2*SIMD*WT::width> val = (x, y);//(x << (SIMD*WT::width)) | y;

      if ((m_idx & 1) == 0) {
        const ap_uint<SIMD*WT::width> weight = deinterleave(val);
        return weight;
      }
      else {
        const ap_uint<SIMD*WT::width> weight = deinterleave(val >> 1);
        return weight;
      }
    }
  };

 public:
  TileIndex weights(unsigned const  tile) const {
#pragma HLS inline
    return  TileIndex(*this, tile);
  }
};


template<size_t Layer, unsigned SIMD, typename WT ,unsigned PE, unsigned TILES, typename Weights, typename ORAM, typename ATU>
void loadORAMWeights(Weights& weights, ORAM& oram, const ATU& atu, uint8_t* block_cache, uint8_t* server_data) {
  const size_t element_size = atu.element_size(Layer);
  std::pair<size_t, size_t> block_byte;

  for (unsigned pe = 0; pe < PE; ++pe) {
    for (unsigned tile = 0; tile < TILES; ++tile) {
      block_byte = atu.index_to_block(Layer, pe, tile);
      oram.read(block_byte.first, block_cache, server_data);

      weights.m_weights[pe][tile] = 0;
      for (size_t i = 0; i < element_size; ++i) {
        #pragma HLS pipeline
        weights.m_weights[pe][tile] |= ap_uint<SIMD*WT::width>(block_cache[block_byte.second + i]) << (i * 8);
      }
    }
  }
}

#endif
