#include "top.h"
#include "memory/fpga_binary_tree.h"
#include "fpga_path_oram2.h"
//#include "fpga_path_oram.h"


static BinaryTree<uint32_t, uint64_t, 3> btree_test;
static FPGAPathORAM2<ORAM_HEIGHT, ORAM_BLOCK_SIZE, ORAM_BUCKET_SIZE> oram;


void ORAMController(uint32_t program_mode, uint32_t oram_op, uint64_t block_addr, uint8_t* block_data, uint8_t* server_data) {
	// signals to be mapped to the AXI Lite slave port
	#pragma HLS INTERFACE s_axilite port=return bundle=control
	#pragma HLS INTERFACE s_axilite port=program_mode bundle=control
	//#pragma HLS INTERFACE s_axilite port=init bundle=control
	#pragma HLS INTERFACE s_axilite port=oram_op bundle=control
	#pragma HLS INTERFACE s_axilite port=block_addr bundle=control

	// signals to be mapped to the AXI master port (hostmem)
	#pragma HLS INTERFACE m_axi offset=slave port=block_data bundle=hostmem depth=1024
	#pragma HLS INTERFACE s_axilite port=block_data bundle=control depth=1024

	#pragma HLS INTERFACE m_axi offset=slave port=server_data bundle=hostmem depth=65016
	#pragma HLS INTERFACE s_axilite port=server_data bundle=control depth=65016


	switch (static_cast<ProgramMode>(program_mode)) {
		case ProgramMode::InitORAM: {
			oram.initRNG(ORAM_RNG_INIT);
			oram.initServerMem(server_data);
			break;
		}

		case ProgramMode::AccessORAM: {
			oram.access(static_cast<ORAMOp>(oram_op), block_addr, block_data, server_data);
			break;
		}

		case ProgramMode::BinaryTreeRead: {
			const auto it = btree_test.find(oram_op);
			if (it != btree_test.end()) {
				auto& val = it.access(btree_test).second;

				//memcpy(block_data, &it->second, sizeof(uint64_t));
				for (int i = 0; i < sizeof(uint64_t); ++i) {
					#pragma HLS pipeline
					block_data[i] = static_cast<uint8_t>(val >> (i*8));
				}
			}
			break;
		}

		case ProgramMode::BinaryTreeWrite: {
			const auto it_bool = btree_test.insert({oram_op, block_addr});
			if (!it_bool.second && (it_bool.first != btree_test.end())) {
				it_bool.first.access(btree_test).second = block_addr;
			}
			break;
		}

		default: break;
	}
}
