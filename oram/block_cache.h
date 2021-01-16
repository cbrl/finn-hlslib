#pragma once

#include <cstdint>

template<typename ORAM>
struct ORAMBlockCache {
  typename ORAM::Block& get(ORAM& oram, size_t blk, uint8_t* server_data) {
    if (block_num != blk) {
      oram.read(blk, block.data(), server_data);
      block_num = blk;
    }
    return block;
  }

  typename ORAM::Block block;
  size_t block_num;
};
