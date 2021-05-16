#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include <ap_int.h>

#include "memory/fpga_resource_pool.h"
#include "memory/ap_array.h"
#include "util.h"


enum class ORAMOp : uint8_t {
	Read  = 0,
	Write = 1
};

template<uint8_t HeightL, uint32_t BlockSizeB, uint8_t BucketSizeZ = 4>
class FPGAPathORAM2 {
public:
	static constexpr uint64_t bucket_count  = (1ull << (HeightL + 1)) - 1;
	static constexpr uint8_t  height_L      = HeightL;
	static constexpr uint32_t block_size_B  = BlockSizeB;
	static constexpr uint8_t  bucket_size_Z = BucketSizeZ;
	static constexpr uint64_t block_count_N = BucketSizeZ * bucket_count;

	// An integer with the least number of bits required to address all blocks
	using client_block_id = ap_uint<util::ceil_int_log2(block_count_N)>;

	// An integer with the least number of bits required to address all buckets
	using client_bucket_id = ap_uint<util::ceil_int_log2(bucket_count)>;

	// An integer with the least number of bits required to address a leaf node
	using client_leaf_id = ap_uint<HeightL>;

	using Block = ap_array<uint8_t, BlockSizeB>;

	struct IDBlock {
		static constexpr uint64_t invalid_block = -1;
		uint64_t id = invalid_block;
		Block data;
	};

	using Bucket = ap_array<IDBlock, BucketSizeZ>;


	FPGAPathORAM2() = default;

	void initRNG(uint64_t rng_init) {
		rng = xorshift64{rng_init};
	}

	void initServerMem(uint8_t* server_data) {
		const uint64_t id_size = SIZEOF_MEMBER(IDBlock, id);

		for (uint64_t block = 0; block < block_count_N; ++block) {
			const uint64_t offset = block * (id_size + BlockSizeB);

			//memcpy(server_data + offset, &IDBlock::invalid_block, SIZEOF_MEMBER(IDBlock, id));
			for (uint8_t i = 0; i < id_size; ++i) {
				#pragma HLS pipeline
				*(server_data + offset + i) = static_cast<uint8_t>(IDBlock::invalid_block >> (i*8));
			}
		}
		for (uint64_t i = 0; i < block_count_N; ++i) {
			position_map[i] = randomPath();
		}
	}

	void read(client_block_id blk, uint8_t* blk_data, uint8_t* server_data) {
		access(ORAMOp::Read, blk, blk_data, server_data);
	}

	void write(client_block_id blk, const uint8_t* blk_data, uint8_t* server_data) {
		access(ORAMOp::Write, blk, const_cast<uint8_t*>(blk_data), server_data); //won't actually modify b
	}

	void access(ORAMOp op, client_block_id blk, uint8_t* blk_data, uint8_t* server_data) {
		const client_leaf_id leaf = position_map[blk];
		position_map[blk] = randomPath();

		readPath(leaf, server_data);

		switch (op) {
			case ORAMOp::Read: {
				if (stash.contains(blk)) {
					const auto& stash_block = stash.at(blk);
					//memcpy(blk_data, stash_block.data(), BlockSizeB);
					for (uint32_t i = 0; i < BlockSizeB; ++i) {
						#pragma HLS pipeline
						blk_data[i] = stash_block[i];
					}
				}
				break;
			}

			case ORAMOp::Write: {
				const auto it_bool = stash.emplace_empty(blk);

				if (it_bool.first != stash.end()) {
					auto& stash_block = it_bool.first.access(stash);
					//memcpy(stash_block.data(), blk_data, BlockSizeB);
					for (uint32_t i = 0; i < BlockSizeB; ++i) {
						#pragma HLS pipeline
						stash_block[i] = blk_data[i];
					}
				}
				break;
			}

			default: break;
		}

		writePath(leaf, server_data);
	}

private:

	void readPath(client_leaf_id leaf, uint8_t* server_data) {
		for (uint8_t l = 0; l <= HeightL; ++l) {
			Bucket bucket;
			readBucket(bucket, getNodeOnPath(leaf, l), server_data);
			stashBucket(bucket);			
		}
	}

	void writePath(client_leaf_id leaf, uint8_t* server_data) {
		for (int16_t l = HeightL; l >= 0; --l) {
			const client_bucket_id node = getNodeOnPath(leaf, static_cast<uint8_t>(l));

			client_block_id valid_blocks[(HeightL+1) * BucketSizeZ];
			const uint8_t valid_cnt = getIntersectingBlocks(valid_blocks, leaf, static_cast<uint8_t>(l));

			Bucket bucket;
			unstashBucket(bucket, valid_blocks, valid_cnt);
			writeBucket(bucket, node, server_data);
		}
	}

	uint8_t getIntersectingBlocks(client_block_id (&valid_blocks)[(HeightL+1) * BucketSizeZ], client_leaf_id leaf, uint8_t height) {
		const client_bucket_id node = getNodeOnPath(leaf, height);
		uint8_t valid_idx = 0;

		for (auto it = stash.handles().begin(); it != stash.handles().end(); ++it) {
			const client_block_id block_id = it.access(stash.handles());

			if (getNodeOnPath(position_map[block_id], height) == node) {
				valid_blocks[valid_idx] = block_id;
				valid_idx++;
			}
		}

		return valid_idx;
	}

	client_bucket_id getNodeOnPath(uint64_t leaf, uint8_t height) {
		leaf += bucket_count / 2;

		for (int16_t l = HeightL - 1; l >= static_cast<int16_t>(height); --l) {
			leaf = ((leaf+1) / 2) - 1;
		}

		return leaf;
	}

	void stashBucket(const Bucket& bucket) {
		for (uint8_t z = 0; z < BucketSizeZ; ++z) {
			const IDBlock& block = bucket[z];

			if (block.id != IDBlock::invalid_block) {
				const auto it_bool = stash.emplace_empty(block.id);

				// Copy block to stash
				if (it_bool.first != stash.end()) {
					it_bool.first.access(stash) = block.data;
				}
			}
		}
	}

	void unstashBucket(Bucket& bucket, client_block_id* valid_blocks, uint8_t valid_cnt) {
		for (uint8_t z = 0; z < std::min(valid_cnt, BucketSizeZ); ++z) {
			IDBlock& block = bucket[z];
			block.id = valid_blocks[z];
			block.data = stash.at(block.id);

			stash.erase(block.id);
		}

		for (uint8_t z = valid_cnt; z < BucketSizeZ; ++z) {
			IDBlock& block = bucket[z];
			block.id = IDBlock::invalid_block;
		}
	}

	void readBucket(Bucket& out, client_bucket_id index, uint8_t* server_data) {
		#pragma HLS inline
		const uint64_t block_idx = static_cast<uint64_t>(index) * BucketSizeZ;
		for (uint8_t i = 0; i < BucketSizeZ; ++i) {
			//#pragma HLS loop_flatten
			readBlock(out[i], block_idx + i, server_data);
		}
	}

	void writeBucket(const Bucket& in, client_bucket_id index, uint8_t* server_data) {
		#pragma HLS inline
		const uint64_t block_idx = static_cast<uint64_t>(index) * BucketSizeZ;
		for (uint8_t i = 0; i < BucketSizeZ; ++i) {
			//#pragma HLS loop_flatten
			writeBlock(in[i], block_idx + i, server_data);
		}
	}


	void readBlock(IDBlock& out, uint64_t index, uint8_t* server_data) {
		const uint64_t id_size = SIZEOF_MEMBER(IDBlock, id);
		const uint64_t offset = index * (id_size + BlockSizeB);

		//memcpy(&out.id, server_data + offset, SIZEOF_MEMBER(IDBlock, id));
		out.id = 0;
		for (uint8_t i = 0; i < id_size; ++i) {
			#pragma HLS pipeline
			out.id |= static_cast<uint64_t>(*(server_data + offset + i)) << (i*8);
		}

		//memcpy(out.data.data(), server_data + (offset + id_size), BlockSizeB);
		for (int i = 0; i < BlockSizeB; ++i) {
			#pragma HLS pipeline
			out.data[i] = *(server_data + offset + id_size + i);
		}
		
	}

	void writeBlock(const IDBlock& in, uint64_t index, uint8_t* server_data) {
		const uint64_t id_size = SIZEOF_MEMBER(IDBlock, id);
		const uint64_t offset = index * (id_size + BlockSizeB);

		//memcpy(server_data + offset, &in.id, SIZEOF_MEMBER(IDBlock, id));
		for (uint8_t i = 0; i < id_size; ++i) {
			#pragma HLS pipeline
			*(server_data + offset + i) = static_cast<uint8_t>(in.id >> (i*8));
		}

		//memcpy(server_data + (offset + id_size), in.data.data(), BlockSizeB);
		for (int i = 0; i < BlockSizeB; ++i) {
			#pragma HLS pipeline
			*(server_data + offset + id_size + i) = in.data[i];
		}
		
	}

	client_leaf_id randomPath() {
		#pragma HLS inline
		return rng.generate() % (1ull << HeightL);
	}


	client_leaf_id position_map[block_count_N];
	ResourcePool<client_block_id, Block, (util::ceil_int_log2(block_count_N) << 2)> stash; //size: HeightL * BucketSizeZ ?

	xorshift64 rng;
};
