#*******************************************************************************
# Copyright 2015 Cadence Design Systems, Inc.
# All Rights Reserved.
#
#*******************************************************************************
#
# Stratus Project File
#
############################################################
# Project Parameters
############################################################
#
# Technology Libraries
#
set LIB_PATH "[get_install_path]/share/stratus/techlibs/GPDK045/gsclib045_svt_v4.4/gsclib045/timing"
set LIB_LEAF "slow_vdd1v2_basicCells.lib"
use_tech_lib    "$LIB_PATH/$LIB_LEAF"

#
# Global synthesis attributes.
#
set_attr clock_period           10.0
set_attr message_detail         3 
#set_attr default_input_delay    0.1
#set_attr sched_aggressive_1 on
#set_attr unroll_loops on
#set_attr flatten_arrays all 
#set_attr timing_aggression 0
#set_attr default_protocol true

#
# Simulation Options
#
### 1. You may add your own options for C++ compilation here.
set_attr cc_options             "-DCLOCK_PERIOD=10.0 -g"
#enable_waveform_logging -vcd
set_attr end_of_sim_command "make saySimPassed"

#
# Testbench or System Level Modules
#
### 2. Add your testbench source files here.
define_system_module ../main.cpp
define_system_module ../Testbench.cpp
define_system_module ../System.cpp

#
# SC_MODULEs to be synthesized
#
### 3. Add your design source files here (to be high-level synthesized).
define_hls_module Lenet ../Lenet.cpp

### 4. Define your HLS configuration (arbitrary names, BASIC and DPA in this example). 
define_hls_config Lenet BASIC
define_hls_config Lenet DPA       --dpopt_auto=op,expr
define_hls_config Lenet BASIC_FLATTEN -DFLATTEN=1
define_hls_config Lenet BASIC_CONV_BUFF -DCONV_BUFF=1
define_hls_config Lenet BASIC_FC_PIPE1 -DFC_II=1
define_hls_config Lenet BASIC_FC_PIPE2 -DFC_II=2
define_hls_config Lenet BASIC_FC_PIPE3 -DFC_II=3
define_hls_config Lenet BASIC_FC_PIPE4 -DFC_II=4
define_hls_config Lenet BASIC_CONV_PIPE1 -DCONV_II=1 -DFC_II=1
define_hls_config Lenet BASIC_CONV_PIPE2 -DCONV_II=2 -DFC_II=1
define_hls_config Lenet BASIC_CONV_PIPE4 -DCONV_II=4 -DFC_II=1
define_hls_config Lenet BASIC_CONV_PIPE8 -DCONV_II=8 -DFC_II=1
define_hls_config Lenet BASIC_CONV_UNROLL -DCONV_UNROLL=1 -DFC_II=1

define_hls_config Lenet BASIC_FC_PE -DFC_PE_UNROLL=1
define_hls_config Lenet BASIC_FC_PE1 -DFC_PE_UNROLL=1
define_hls_config Lenet BASIC_FC_PE2 -DFC_PE_UNROLL=1
define_hls_config Lenet BASIC_FC_PE4 -DFC_PE_UNROLL=1
define_hls_config Lenet BASIC_FC_PE8 -DFC_PE_UNROLL=1
define_hls_config Lenet BASIC_FC_PE16 -DFC_PE_UNROLL=1

#define_hls_config Lenet BASIC_CONV_KERNEL_PIPE1 -DCONV_KERNEL_PIPE=1 -DCONV_II=1 -DFC_II=1



set IMAGE_DIR           ".."
set IN_FILE_NAME        "${IMAGE_DIR}/lena_std_short.bmp"
set OUT_FILE_NAME				"out.bmp"

### 5. Define simulation configuration for each HLS configuration
### 5.1 The behavioral simulation (C++ only).
define_sim_config B -argv "$IN_FILE_NAME $OUT_FILE_NAME"
### 5.2 The Verilog simulation for HLS config "BASIC". 
define_sim_config V_BASIC "Lenet RTL_V BASIC" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
### 5.3 The Verilog simulation for HLS config "DPA". 
define_sim_config V_DPA "Lenet RTL_V DPA" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
### 5.4 The Verilog simulation for HLS config "BASIC_FLATTEN". 
define_sim_config V_BASIC_FLATTEN "Lenet RTL_V BASIC_FLATTEN" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_BASIC_CONV_BUFF "Lenet RTL_V BASIC_CONV_BUFF" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_FC_PIPE1 "Lenet RTL_V BASIC_FC_PIPE1" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_FC_PIPE2 "Lenet RTL_V BASIC_FC_PIPE2" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_FC_PIPE3 "Lenet RTL_V BASIC_FC_PIPE3" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_FC_PIPE4 "Lenet RTL_V BASIC_FC_PIPE4" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_CONV_PIPE1 "Lenet RTL_V BASIC_CONV_PIPE1" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_CONV_PIPE2 "Lenet RTL_V BASIC_CONV_PIPE2" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_CONV_PIPE4 "Lenet RTL_V BASIC_CONV_PIPE4" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_CONV_PIPE8 "Lenet RTL_V BASIC_CONV_PIPE8" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_CONV_UNROLL "Lenet RTL_V BASIC_CONV_UNROLL" -argv "$IN_FILE_NAME $OUT_FILE_NAME"

define_sim_config V_BASIC_FC_PE "Lenet RTL_V BASIC_FC_PE" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_BASIC_FC_PE1 "Lenet RTL_V BASIC_FC_PE1" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_BASIC_FC_PE2 "Lenet RTL_V BASIC_FC_PE2" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_BASIC_FC_PE4 "Lenet RTL_V BASIC_FC_PE4" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_BASIC_FC_PE8 "Lenet RTL_V BASIC_FC_PE8" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
define_sim_config V_BASIC_FC_PE16 "Lenet RTL_V BASIC_FC_PE16" -argv "$IN_FILE_NAME $OUT_FILE_NAME"
