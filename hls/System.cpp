#include "System.h"
System::System( sc_module_name n ): sc_module( n ), 
	tb("tb"), lenet("lenet"), clk("clk", CLOCK_PERIOD, SC_NS), rst("rst")
{
	tb.i_clk(clk);
	tb.o_rst(rst);
	lenet.i_clk(clk);
	lenet.i_rst(rst);
	tb.i_result(result);
	tb.o_img(img);
	lenet.o_result(result);
	lenet.i_img(img);
}

System::~System() {}
