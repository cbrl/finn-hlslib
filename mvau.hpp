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
 *  Authors: Giulio Gambardella <giuliog\xilinx.com>
 *           Thomas B. Preusser <thomas.preusser\utexas.edu>
 *             Marie-Curie Fellow, Xilinx Ireland, Grant Agreement No. 751339
 *           Christoph Doehring <cdoehrin\xilinx.com>
 *
 *  \file mvau.hpp
 *
 *  This file lists a templated funtion used to implement  
 *  Matrix-Vector-Activation Unit
 *
 *  This project has received funding from the European Union's Framework
 *  Programme for Research and Innovation Horizon 2020 (2014-2020) under
 *  the Marie Skłodowska-Curie Grant Agreement No. 751339.
 *
 *******************************************************************************/

#ifndef MVAU_HPP
#define MVAU_HPP

#include "hls_stream.h"

#include "mac.hpp"
#include "interpret.hpp"

/**
 * \brief Matrix vector activate function
 *
 * The function performs the multiplication between a weigth matrix and the input activation vector,
 * accumulating the results and then applying an activation function on the accumulated result.
 *
 * 
 * \tparam MatrixW    Width of the input matrix
 * \tparam MatrixH    Heigth of the input matrix
 * \tparam SIMD       Number of input columns computed in parallel
 * \tparam PE         Number of output rows computed in parallel
 * \tparam TSrcI      DataType of the input activation (as used in the MAC)
 * \tparam TDstI      DataType of the output activation (as generated by the activation)
 * \tparam TWeightI   DataType of the weights (as used in the MAC)
 * \tparam TI         DataType of the input stream - safely deducible from the paramaters
 * \tparam TO         DataType of the output stream - safely deducible from the paramaters
 * \tparam TW         DataType of the weights matrix - safely deducible from the paramaters
 * \tparam TA         DataType of the activation class (e.g. thresholds) - safely deducible from the paramaters
 * \tparam R          Datatype for the resource used for FPGA implementation of the MAC  - safely deducible from the paramaters
 *
 * \param in          Input stream
 * \param out         Output stream
 * \param weights     Weights matrix (currently supports BinaryWeights or FixedPointWeights)
 * \param activation  Activation class
 * \param reps        Number of time the function has to be repeatedly executed (e.g. number of images)
 * \param r           Resource type for the hardware implementation of the MAC block
 */
template<
  unsigned MatrixW, unsigned MatrixH, unsigned SIMD, unsigned PE,
  typename TSrcI = Identity, typename TDstI = Identity, typename TWeightI = Identity,
  typename TI, typename TO, typename TW, typename TA, typename R
>
void Matrix_Vector_Activate_Batch(hls::stream<TI> &in,
				  hls::stream<TO> &out,
				  TW  const &weights,
				  TA  const &activation,
				  int const  reps,
				  R const &r) {

  // how many different rows each neuron will compute
  // alternatively: number of vertical matrix chunks
  unsigned const  NF = MatrixH / PE;

  // how many synapse groups each row is split into
  // alternatively: number of horizontal matrix chunks
  unsigned const  SF = MatrixW / SIMD;

  // input vector buffers
  TI  inputBuf[SF];
#pragma HLS ARRAY_PARTITION variable=inputBuf complete dim=1

  decltype(activation.init(0,0))  accu[PE];
#pragma HLS ARRAY_PARTITION variable=accu complete dim=0

  unsigned  nf   = 0;
  unsigned  sf   = 0;
  unsigned  tile = 0; // invariant: tile = nf*SF + sf

  // everything merged into a common iteration space (one "big" loop instead
  // of smaller nested loops) to get the pipelinening the way we want
  unsigned const TOTAL_FOLD = NF * SF;
  for(unsigned  i = 0; i < reps * TOTAL_FOLD; i++) {
#pragma HLS PIPELINE II=1
    TI  inElem;
    if(nf == 0) {
      // read input from stream
      inElem = in.read();
      // store in appropriate buffer for reuse
      inputBuf[sf] = inElem;
    }
    else {
      // reuse buffered input
      inElem = inputBuf[sf];
    }

    // Threshold Initialisation
    if(sf == 0) {
      for(unsigned  pe = 0; pe < PE; pe++) {
#pragma HLS UNROLL
	    accu[pe] = activation.init(nf, pe);
      }
    }

    // compute matrix-vector product for each processing element
    auto const &w = const_cast<typename std::remove_const<TW>::type&>(weights).weights(tile);
    for(unsigned  pe = 0; pe < PE; pe++) {
#pragma HLS UNROLL
      auto const  wgt = TWeightI()(const_cast<typename std::remove_const<typename std::remove_reference<decltype(w)>::type>::type&>(w)[pe]);
      auto const  act = TSrcI()(inElem);
      accu[pe] = mac<SIMD>(accu[pe], wgt, act, r);
    }

    // keep track of which folded synapse/neuron we are processing
    ++tile;
    if(++sf == SF) {
      // produce output and clear accumulators
      auto  outElem = TDstI().template operator()<TO>();
      for (unsigned  pe = 0; pe < PE; pe++) {
#pragma HLS UNROLL
	    outElem[pe] = const_cast<typename std::remove_const<TA>::type&>(activation).activate(nf, pe, accu[pe]);
	    //std::cout << "out " << outElem[pe] << std::endl;
      }

      out.write(outElem);

      // next folded neuron or image
      sf = 0;
      if(++nf == NF) {
	    nf   = 0;
	    tile = 0;
      }
    }
  }
}

#endif
