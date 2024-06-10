#ifndef SYSTEM_H_
#define SYSTEM_H_
#include <systemc>
using namespace sc_core;

#include "Testbench.h"
#ifndef NATIVE_SYSTEMC
#include "Lenet_wrap.h"
#else
#include "Lenet.h"
#endif

class System: public sc_module
{
public:
	SC_HAS_PROCESS( System );
	System( sc_module_name n);
	~System();
private:
  Testbench tb;
#ifndef NATIVE_SYSTEMC
	Lenet_wrapper lenet;
#else
	Lenet lenet;
#endif
	sc_clock clk;
	sc_signal<bool> rst;
#ifndef NATIVE_SYSTEMC
	cynw_p2p< sc_dt::sc_int<32> > img;
	cynw_p2p< sc_dt::sc_int<32> > result;
#else
	sc_fifo< sc_dt::sc_int<32> > img;
	sc_fifo< sc_dt::sc_int<32> > result;
#endif

	std::string _output_bmp;
};
#endif
