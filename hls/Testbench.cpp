#include <cstdio>
#include <cstdlib>
using namespace std;

#include "Testbench.h"

Testbench::Testbench(sc_module_name n) : sc_module(n) {
  SC_THREAD(feed_img);
  sensitive << i_clk.pos();
  dont_initialize();
  SC_THREAD(fetch_result);
  sensitive << i_clk.pos();
  dont_initialize();
}

Testbench::~Testbench() {
	//cout<< "Max txn time = " << max_txn_time << endl;
	//cout<< "Min txn time = " << min_txn_time << endl;
	//cout<< "Avg txn time = " << total_txn_time/n_txn << endl;
	cout << "Total run time = " << total_run_time << endl;
}

void Testbench::feed_img() {
	n_txn = 0;
	max_txn_time = SC_ZERO_TIME;
	min_txn_time = SC_ZERO_TIME;
	total_txn_time = SC_ZERO_TIME;

#ifndef NATIVE_SYSTEMC
	o_img.reset();
#endif
	o_rst.write(false);
	wait(5);
	o_rst.write(true);
	wait(1);
	total_start_time = sc_time_stamp();

  for(int i = 0; i < (32*32/4) ; i++) {
    sc_dt::sc_int<32> img_out;
    img_out.range(7, 0) = input_img[4*i];
    img_out.range(15, 8) = input_img[4*i+1];
    img_out.range(23, 16) = input_img[4*i+2];
    img_out.range(31, 24) = input_img[4*i+3];
#ifndef NATIVE_SYSTEMC
		o_img.put(img_out);
#else
		o_img.write(img_out);
#endif
  }
}

void Testbench::fetch_result() {
  unsigned int y;
  int total;
#ifndef NATIVE_SYSTEMC
	i_result.reset();
#endif
	wait(5);
	wait(1);
  for (y = 0; y != 10; ++y) {
#ifndef NATIVE_SYSTEMC
	  total = i_result.get();
#else
		total = i_result.read();
#endif
    cout << total << "\n";
  }
	total_run_time = sc_time_stamp() - total_start_time;
  sc_stop();
}
