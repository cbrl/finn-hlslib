#pragma once

#include <array>
#include <cstdint>


namespace util {
template<typename T>
T ceil_int_div(T num, T denom) {
    #pragma HLS inline
	return (num / denom) + ((num % denom) != 0);
}
}


template<size_t Layers>
class WeightAddressTranslator {
public:
	WeightAddressTranslator(
		size_t block_size,
		std::array<size_t, Layers> SIMD,
		std::array<size_t, Layers> WT,
		std::array<size_t, Layers> PE,
		std::array<size_t, Layers> TILES,
		size_t block_offset = 0
	) : TILES(TILES) {
        #pragma HLS inline

		for (size_t i = 0; i < Layers; ++i) {
            #pragma HLS unroll

			element_sizes[i] = util::ceil_int_div<size_t>(WT[i] * SIMD[i], 8);
			elements_per_block[i] = block_size / element_sizes[i];

			const size_t element_count = PE[i] * TILES[i];
			const size_t block_count   = util::ceil_int_div(element_count, elements_per_block[i]);

			block_counts[i] = block_count;
			start_blocks[i] = (i == 0) ? block_offset : (start_blocks[i-1] + block_counts[i-1]);

		}
	}

	std::pair<size_t, size_t> index_to_block(size_t layer, size_t pe, size_t tile) const {
        #pragma HLS inline

		const size_t this_element = (pe * TILES[layer]) + tile;
		const size_t block_num    = start_blocks[layer] + (this_element / elements_per_block[layer]);
		const size_t block_offset = element_sizes[layer] * (this_element % elements_per_block[layer]);

		return {block_num, block_offset};
	}

    size_t element_size(size_t layer) const noexcept {
        #pragma HLS inline
        return element_sizes[layer];
    }

	size_t block_elements(size_t layer) const noexcept {
		#pragma HLS inline
		return elements_per_block[layer];
	}

	size_t start_block(size_t layer) const noexcept {
		#pragma HLS inline
		return start_blocks[layer];
	}

	size_t block_count(size_t layer) const noexcept {
		#pragma HLS inline
		return block_counts[layer];
	}

private:

	std::array<size_t, Layers> TILES;
	std::array<size_t, Layers> element_sizes;
	std::array<size_t, Layers> elements_per_block;
	std::array<size_t, Layers> start_blocks;
	std::array<size_t, Layers> block_counts;
};


template<size_t Layers>
class ThresholdAddressTranslator {
public:
	ThresholdAddressTranslator(
		size_t BlockSize,
		std::array<size_t, Layers> NF,
		std::array<size_t, Layers> PE,
		std::array<size_t, Layers> NumTH,
		std::array<size_t, Layers> TA,
		size_t block_offset = 0
	) : NF(NF), NumTH(NumTH) {
        #pragma HLS inline

		for (size_t i = 0; i < Layers; ++i) {
            #pragma HLS unroll

			element_sizes[i] = util::ceil_int_div<size_t>(TA[i], 8);
			elements_per_block[i] = BlockSize / element_sizes[i];

			const size_t element_count = PE[i] * NF[i] * NumTH[i];
			const size_t block_count   = util::ceil_int_div(element_count , elements_per_block[i]);

			block_counts[i] = block_count;
			start_blocks[i] = (i == 0) ? block_offset : (start_blocks[i-1] + block_counts[i-1]);
		}
	}

	std::pair<size_t, size_t> index_to_block(size_t layer, size_t pe, size_t nf, size_t numth) const {
        #pragma HLS inline

		const size_t this_element = (pe * NF[layer] * NumTH[layer]) + (nf * NumTH[layer]) + numth;
		const size_t block_num    = start_blocks[layer] + (this_element / elements_per_block[layer]);
		const size_t block_offset = element_sizes[layer] * (this_element % elements_per_block[layer]);

		return {block_num, block_offset};
	}

    size_t element_size(size_t layer) const noexcept {
        #pragma HLS inline
        return element_sizes[layer];
    }

	size_t block_elements(size_t layer) const noexcept {
		#pragma HLS inline
		return elements_per_block[layer];
	}

	size_t start_block(size_t layer) const noexcept {
		#pragma HLS inline
		return start_blocks[layer];
	}

	size_t block_count(size_t layer) const noexcept {
		#pragma HLS inline
		return block_counts[layer];
	}

private:

	std::array<size_t, Layers> NF;
	std::array<size_t, Layers> NumTH;
	std::array<size_t, Layers> element_sizes;
	std::array<size_t, Layers> elements_per_block;
	std::array<size_t, Layers> start_blocks;
	std::array<size_t, Layers> block_counts;
};
