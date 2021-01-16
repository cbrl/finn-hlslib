#include "top.h"
#include "fpga_path_oram.h"
#include "memory/fpga_resource_pool.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <random>
#include <vector>

#define ORAM_IDBLOCK_SIZE (ORAM_BLOCK_SIZE + sizeof(uint64_t))
#define ORAM_BUCKET_COUNT ((1ull << (ORAM_HEIGHT+1)) - 1)
#define ORAM_BLOCK_COUNT (ORAM_BUCKET_SIZE * ORAM_BUCKET_COUNT)
#define ORAM_SERVER_SIZE (ORAM_BLOCK_COUNT * ORAM_IDBLOCK_SIZE)


static uint8_t g_server_data[ORAM_SERVER_SIZE];


void ORAMInit() {
	ORAMController(static_cast<uint32_t>(ProgramMode::InitORAM), 0, 0, nullptr, g_server_data);
}

void ORAMWrite(uint64_t blk_id, uint8_t* blk_data) {
	ORAMController(static_cast<uint32_t>(ProgramMode::AccessORAM), static_cast<uint32_t>(ORAMOp::Write), blk_id, blk_data, g_server_data);
}

void ORAMRead(uint64_t blk_id, uint8_t* blk_data) {
	ORAMController(static_cast<uint32_t>(ProgramMode::AccessORAM), static_cast<uint32_t>(ORAMOp::Read), blk_id, blk_data, g_server_data);
}


void test_oram() {
	std::cout << "Initializing ORAM" << std::endl;
	ORAMInit();


	// Generate block data
	//--------------------------------------------------------------------------------
	std::random_device rd;
	//std::mt19937 gen{rd()};
	std::mt19937 gen{0xDEADBEEF};

	std::uniform_int_distribution<uint64_t> addr_dist{0, ORAM_BLOCK_COUNT-1};
	//std::uniform_int_distribution<uint8_t> val_dist{0, 255};

	std::unordered_map<uint64_t, std::array<uint8_t, ORAM_BLOCK_SIZE>> input_map;

	std::cout << "Generating inputs" << std::endl;
	for (int i = 0; i < 50; ++i) {
		const uint64_t blk_id = addr_dist(gen);
		auto& block = input_map[blk_id];
		// for (uint8_t& n : block) {
		// 	n = val_dist(gen);
		// }
		block.fill(blk_id);
	}


	// Write blocks
	//--------------------------------------------------------------------------------
	std::cout << "Writing data" << std::endl;
	for (auto& entry : input_map) {
		ORAMWrite(entry.first, entry.second.data());
	}


	// Read and validate blocks
	//--------------------------------------------------------------------------------
	std::cout << "Reading data" << std::endl;
	size_t failures = 0;
	size_t successes = 0;
	const bool print = false;

	std::array<uint8_t, ORAM_BLOCK_SIZE> oram_data;
	for (const auto& entry : input_map) {
		if (print) std::cout << "Fetching value at key " << entry.first << std::endl;
		ORAMRead(entry.first, oram_data.data());

		bool success = true;
		for (size_t i = 0; i < oram_data.size(); ++i) {
			if (oram_data[i] != entry.second[i]) {
				success = false;
				break;
			}
		}

		if (success) {
			if (print) std::cout << "  Test succeeded" << std::endl;
			successes += 1;
		}
		else {
			if (print) {
				std::cout << "  Test failed." << std::endl;
				std::cout << "    Expected: ";
				for (const uint8_t n : entry.second) {
					std::cout << static_cast<uint16_t>(n) << ' ';
				}
				std::cout << std::endl << "    Got: ";
				for (const uint8_t n : oram_data) {
					std::cout << static_cast<uint16_t>(n) << ' ';
				}
				std::cout << std::endl;
			}
			failures += 1;
		}
	}

	std::cout << "Successful tests: " << successes << "\nFailed tests: " << failures << std::endl;
}


void test_btree() {
	// Generate input data
	//--------------------------------------------------------------------------------
	std::random_device rd;
	std::mt19937 gen{rd()};

	std::uniform_int_distribution<uint32_t> key_dist{0, 100};
	std::uniform_int_distribution<uint64_t> val_dist{0, 64};

	std::unordered_map<uint32_t, uint64_t> input_map;

	std::cout << "Generating inputs" << std::endl;
	for (int i = 0; i < 5; ++i) {
		input_map[key_dist(gen)] = val_dist(gen);
	}


	// Write data
	//--------------------------------------------------------------------------------
	std::cout << "Writing data" << std::endl;
	for (const auto& entry : input_map) {
		ORAMController(static_cast<uint32_t>(ProgramMode::BinaryTreeWrite), entry.first, entry.second, nullptr, nullptr);
	}


	// Read and validate data
	//--------------------------------------------------------------------------------
	std::cout << "Reading data" << std::endl;
	size_t failures = 0;
	size_t successes = 0;
	for (const auto& entry : input_map) {
		std::cout << "Fetching value at key " << entry.first << " (expected: " << entry.second << ')' << std::endl;

		uint64_t val = 0;
		ORAMController(static_cast<uint32_t>(ProgramMode::BinaryTreeRead), entry.first, 0, reinterpret_cast<uint8_t*>(&val), nullptr);

		if (val != entry.second) {
			std::cout << "  Test failed. Got " << val << std::endl;
			failures += 1;
		}
		else {
			std::cout << "  Test succeeded" << std::endl;
			successes += 1;
        }
	}

	std::cout << "Successful tests: " << successes << "\nFailed tests: " << failures << std::endl;
}


int main() {
	//test_btree();
	test_oram();

	return 0;
}
