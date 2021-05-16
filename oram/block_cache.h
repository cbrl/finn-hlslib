#pragma once

#include <cstdint>
#include <type_traits>


template<typename ORAM, size_t Levels>
class ORAMBlockCache {
public:
  ORAMBlockCache() {
    #pragma HLS inline
    for (size_t i = 0; i < Levels; ++i) {
      block_num[Levels] = std::numeric_limits<size_t>::max();
    }
  }

  typename ORAM::Block& get(ORAM& oram, size_t blk, uint8_t* server_data) {
    const size_t level = blk % Levels;

    if (block_num[level] != blk) {
      oram.read(blk, block.data(), server_data);
      block_num[level] = blk;
    }

    return block;
  }

private:
  size_t block_num[Levels];
  typename ORAM::Block block[Levels];
};

// Partial specialization for case where Levels = 1
template<typename ORAM>
class ORAMBlockCache<ORAM, 1> {
public:
  typename ORAM::Block& get(ORAM& oram, size_t blk, uint8_t* server_data) {
    if (block_num != blk) {
      oram.read(blk, block.data(), server_data);
      block_num = blk;
    }

    return block;
  }

private:
  size_t block_num = std::numeric_limits<size_t>::max();
  typename ORAM::Block block;
};
