#ifndef LENET_H_
#define LENET_H_
#include <systemc>
#define PE_SIZE 2
using namespace sc_core;

#ifndef NATIVE_SYSTEMC
#include <cynw_p2p.h>
#endif

#include "filter_def.h"

class Lenet: public sc_module
{
public:
	sc_in_clk i_clk;
	sc_in < bool >  i_rst;
#ifndef NATIVE_SYSTEMC
	cynw_p2p< sc_dt::sc_int<32> >::out o_result;
	cynw_p2p< sc_dt::sc_int<32> >::in i_img;
#else
	sc_fifo_out< sc_dt::sc_int<32> > o_result;
	sc_fifo_in< sc_dt::sc_int<32> > i_img;
#endif

	SC_HAS_PROCESS( Lenet );
	Lenet( sc_module_name n );
	~Lenet();
private:
	void do_filter();
	void Conv1();
	void Conv2();
	void Fused_Conv1();
	void Fused_Conv1_1();
	void Fused_Conv1_2();
	void Fused_Conv2();
	void Fused_Conv2_1();
	void Fused_Conv2_2();
	void Fused_LineBuf_Conv1();
	void Fused_LineBuf_Conv2();
	void Fused_Buf_Conv1();
	void Fused_Buf_Conv2();
	void FC1();
	void FC1_1();
	void FC1_2();
	void FC2();
	void FC2_1();
	void FC2_2();
	void FC3();
	void FC1_PE();
	void FC2_PE();
	void FC3_PE();

	sc_dt::sc_int<8> input_img[32*32];
	sc_dt::sc_int<8> conv1_out[1][6][28][28];
	sc_dt::sc_int<8> maxpool_out[1][6][14][14];
	sc_dt::sc_int<8> conv2_out[1][16][10][10];
	sc_dt::sc_int<8> maxpool2_out[1][16][5][5];
	sc_dt::sc_int<8> fc1_in[400];
	sc_dt::sc_int<8> fc2_in[120];
	sc_dt::sc_int<8> fc3_in[84];
	sc_dt::sc_int<32> fc3_out[10];
	sc_dt::sc_int<8> buffer_1[1176];
	sc_dt::sc_int<8> buffer_2[1176];
	sc_dt::sc_int<8> buffer_3[1176];
	sc_dt::sc_int<8> buffer_4[1176];
	sc_dt::sc_int<8> line_buffer[6][32];
	sc_dt::sc_int<8> line_buffer_2[6][6][14];
	sc_dt::sc_int<8> buffer_conv_act[6][6][6];
	sc_dt::sc_int<8> buffer_conv_weight[6][5][5];
	sc_dt::sc_int<32> FC_ACC[PE_SIZE];
	sc_dt::sc_int<32> CONV_ACC[6];
};
#endif
