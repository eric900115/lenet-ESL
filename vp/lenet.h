#ifndef LENET_H_
#define LENET_H_
#include <systemc>
#include <cmath>
#include <iomanip>
using namespace sc_core;

#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "lenet_def.h"
#include "lenet_weight.h"

struct Lenet : public sc_module {
  tlm_utils::simple_target_socket<Lenet> tsock;

  sc_fifo<bool> compute_start_fifo;
  sc_fifo< sc_dt::sc_int<8> > data_fifo[64];
  sc_fifo< sc_dt::sc_int<32> > o_result[10];

  int input_img[32*32];
  int input_img_0[32*32];
  int input_img_1[32*32];
  int conv1_out[1][6][28][28];
  int maxpool_out[1][6][14][14];
  int conv2_out[1][16][10][10];
  int maxpool2_out[1][16][5][5];
  int fc1_in[400];
  int fc2_in[120];
  int fc3_in[84];
  int fc3_out[10];

  uint32_t irq_number = 0;
  interrupt_gateway *plic = 0;

  SC_HAS_PROCESS(Lenet);

  Lenet(sc_module_name n, uint32_t irq_number): 
    sc_module(n), 
    tsock("t_skt"), 
    base_offset(0),
    irq_number(irq_number)
  {
    tsock.register_b_transport(this, &Lenet::blocking_transport);
    SC_THREAD(do_filter);
    SC_THREAD(write_buffer);
  }

  ~Lenet() {
	}

  unsigned int base_offset;

  void do_filter(){
    { wait(CLOCK_PERIOD, SC_NS); }
    while (true) {
      bool compute_start = compute_start_fifo.read();
      //cout << "=========================\n";
      //cout << "compute start\n";
      //cout << "COMPUTE START:" << sc_time_stamp() << "\n";
      wait(CLOCK_PERIOD, SC_NS);
      if(compute_start) {
        Conv1();
        Conv2();
        FC1();
        FC2();
        FC3();
        write_result();
        plic->gateway_trigger_interrupt(irq_number);
        wait(6743000, SC_NS);
        //cout << "COMPUTE DONE:" << sc_time_stamp() << "\n";
        wait(CLOCK_PERIOD, SC_NS);
      }
    }
  }

  void write_buffer() {

    wait(CLOCK_PERIOD, SC_NS);

    while(true) {
      //cout << "Wrtite Data to Buffer : ";
      //cout << "DMA Start:" << sc_time_stamp() << "\n";
      for(int i = 0; i < (32*32/64); i++) {
        for(int j = 0; j < 64; j++) {
          sc_dt::sc_int<8> data = data_fifo[j].read();
          input_img[i * 64 + j] = data;
        }
        wait(CLOCK_PERIOD*64, SC_NS);
      }
      wait(CLOCK_PERIOD, SC_NS);
      //cout << "DMA DONE:" << sc_time_stamp() << "\n";
      compute_start_fifo.write(1);
      wait(CLOCK_PERIOD, SC_NS);
    }
  }

  void write_result() {
    for(int i = 0; i < 10; i++) {
      o_result[i].write(fc3_out[i]);
    }
  }

  void blocking_transport(tlm::tlm_generic_payload &payload, sc_core::sc_time &delay){
    wait(delay);
    // unsigned char *mask_ptr = payload.get_byte_enable_ptr();
    auto len = payload.get_data_length();
    tlm::tlm_command cmd = payload.get_command();
    sc_dt::uint64 addr = payload.get_address();
    unsigned char *data_ptr = payload.get_data_ptr();

    addr -= base_offset;
    
    word buffer[10];

    switch (cmd) {
      case tlm::TLM_READ_COMMAND:
        switch (addr) {
          case LENET_FILTER_RESULT_ADDR:
            for(int i = 0; i < 10; i++) {
              buffer[i].sint = o_result[i].read();
            }
            break;
          default:
            std::cerr << "READ Error! Lenet::blocking_transport: address 0x"
                      << std::setfill('0') << std::setw(8) << std::hex << addr
                      << std::dec << " is not valid" << std::endl;
        }
        for(int i = 0; i < 10; i++) {
          data_ptr[4*i+0] = buffer[i].uc[0];
          data_ptr[4*i+1] = buffer[i].uc[1];
          data_ptr[4*i+2] = buffer[i].uc[2];
          data_ptr[4*i+3] = buffer[i].uc[3];
        }
        break;
      case tlm::TLM_WRITE_COMMAND:        
        for(int i = 0; i < 64; i++)
          data_fifo[i].write((sc_dt::sc_int<8>)data_ptr[i]);
        break;
      case tlm::TLM_IGNORE_COMMAND:
        payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
      default:
        payload.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
      }
      payload.set_response_status(tlm::TLM_OK_RESPONSE); // Always OK
  }

  void Conv1() {
    
    int batch_size = 1, out_channel = 6, out_H = 28, out_W = 28;
    int kernel_size = 5, input_channel = 1;
    int p_sum = 0, h, w;

    for(int n = 0; n < batch_size; n++) {
      for(int m = 0; m < out_channel; m++) {
        for(int p = 0; p < out_H; p++) {
          for(int q = 0; q < out_W; q++) {
            p_sum = 0;
            for(int r = 0; r < kernel_size; r++){
              for(int s = 0; s < kernel_size; s++) {
                for(int c = 0; c < input_channel; c++) {
                  h = p + r;
                  w = q + s;
                  p_sum += input_img[h * 32 + w] * conv1_weight[m][c][r][s];
                }
              }
            }
            conv1_out[n][m][p][q] = (((p_sum * 130) >> 16) > 0) ? ((p_sum * 130) >> 16) : 0;
          }
        }
      }
    }

    int maxpool_out_H = 14;
    int maxpool_out_W = 14;
    int max_val;

    for(int n = 0; n < batch_size; n++) {
      for(int c = 0; c < out_channel; c++) {
        for(int h = 0; h < maxpool_out_H; h++) {
          for(int w = 0; w < maxpool_out_W; w++) {
            max_val = conv1_out[n][c][h*2][w*2];
            for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
              for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
                if(max_val < conv1_out[n][c][h_][w_]) {
                  max_val = conv1_out[n][c][h_][w_];
                }
              }
            }
            maxpool_out[n][c][h][w] = max_val;
          }
        }
      }
    }
  }

  void Conv2() {

    int batch_size = 1, out_channel = 16, out_H = 10, out_W = 10;
    int kernel_size = 5, input_channel = 6;
    int p_sum = 0, h, w;

    for(int n = 0; n < batch_size; n++) {
      for(int m = 0; m < out_channel; m++) {
        for(int p = 0; p < out_H; p++) {
          for(int q = 0; q < out_W; q++) {
            p_sum = 0;
            for(int c = 0; c < input_channel; c++) {
            for(int r = 0; r < kernel_size; r++){
              for(int s = 0; s < kernel_size; s++) {
              
                  h = p + r;
                  w = q + s;
                  p_sum += maxpool_out[n][c][h][w] * conv2_weight[m][c][r][s];
                }
              }
            }
            conv2_out[n][m][p][q] = (((p_sum * 89) >> 16) > 0) ? ((p_sum * 89) >> 16) : 0;
          }
        }
      }
    }

    int maxpool_out_H = 5;
    int maxpool_out_W = 5;
    int max_val;

    for(int n = 0; n < batch_size; n++) {
      for(int c = 0; c < out_channel; c++) {
        for(int h = 0; h < maxpool_out_H; h++) {
          for(int w = 0; w < maxpool_out_W; w++) {
            max_val = conv2_out[n][c][h*2][w*2];
            for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
              for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
                if(max_val < conv2_out[n][c][h_][w_]) {
                  max_val = conv2_out[n][c][h_][w_];
                }
              }
            }
            maxpool2_out[n][c][h][w] = max_val;
            fc1_in[c*25 + h*5 + w] = max_val;
          }
        }
      }
    }
  }

  void FC1() {
    int p_sum;

    for(int c = 0; c < 120; c++) {
      p_sum = 0;
      for(int w = 0; w < 400; w++) {
        p_sum += fc1_in[w] * fc1_weight[c][w];
      }
      fc2_in[c] = ((p_sum * 335) >> 16) > 0 ? ((p_sum * 335) >> 16) : 0;
    }
  }

  void FC2() {
    int p_sum;

    for(int c = 0; c < 84; c++) {
      p_sum = 0;
      for(int w = 0; w < 120; w++) {
        p_sum += fc2_in[w] * fc2_weight[c][w];
      }
      fc3_in[c] = ((p_sum * 318) >> 16) > 0 ? ((p_sum * 318) >> 16) : 0;
    }
  }

  void FC3() {
    int p_sum;

    for(int c = 0; c < 10; c++) {
      p_sum = 0;
      for(int w = 0; w < 84; w++) {
        p_sum += fc3_in[w] * fc3_weight[c][w];
      }
      fc3_out[c] = p_sum;
      //cout << p_sum << "\n";
    }
  }
};
#endif
