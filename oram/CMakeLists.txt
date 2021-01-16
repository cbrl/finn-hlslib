cmake_minimum_required(VERSION 3.8)
project(testbench)

if(WIN32)
	set(VIVADO_PATH C:/Xilinx/Vivado/2020.1)
else()
	set(VIVADO_PATH /home/hyperion/Xilinx/Vivado/2020.1)
endif()

add_executable(${PROJECT_NAME} top.cpp test_bench.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${VIVADO_PATH}/include)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_14)
