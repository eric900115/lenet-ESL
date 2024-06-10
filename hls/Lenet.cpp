#include <cmath>
#ifndef NATIVE_SYSTEMC
#include "stratus_hls.h"
#endif

#include "Lenet.h"

#define BATCH_SIZE 1
#define CONV1_OUT_CHANNEL 6
#define CONV1_OUT_H 28
#define CONV1_OUT_W 28
#define CONV1_KERNEL_SIZE 5
#define CONV1_IN_CHANNEL 1
#define CONV1_IN_H 32
#define CONV1_IN_W 32
#define MAXPOOL_OUT_H 14
#define MAXPOOL_OUT_W 14
#define CONV2_OUT_CHANNEL 16
#define CONV2_OUT_H 10
#define CONV2_OUT_W 10
#define CONV2_KERENL 5
#define CONV2_IN_CHANNEL 6
#define MAXPOOL2_OUT_H 5
#define MAXPOOL2_OUT_W 5

Lenet::Lenet( sc_module_name n ): sc_module( n )
{
#ifndef NATIVE_SYSTEMC
  HLS_FLATTEN_ARRAY(FC_ACC);
  HLS_FLATTEN_ARRAY(CONV_ACC);
  #ifdef CONV_BUFF
    HLS_FLATTEN_ARRAY(buffer_conv_act);
    HLS_FLATTEN_ARRAY(buffer_conv_weight);
  #endif
#endif
	SC_THREAD( do_filter );
	sensitive << i_clk.pos();
	dont_initialize();
	reset_signal_is(i_rst, false);
        
#ifndef NATIVE_SYSTEMC
  i_img.clk_rst(i_clk, i_rst);
  o_result.clk_rst(i_clk, i_rst);
#endif
}

Lenet::~Lenet() {}

void Lenet::do_filter() {
	{
#ifndef NATIVE_SYSTEMC
		HLS_DEFINE_PROTOCOL("main_reset");
    i_img.reset();
		o_result.reset();
#endif
		wait();
	}
	while (true) {
    for(unsigned int i = 0; i < (32*32)/4; i++) {
      sc_dt::sc_uint<32> img;
#ifndef NATIVE_SYSTEMC
			{
				HLS_DEFINE_PROTOCOL("input");
				img = i_img.get();
				wait();
		  }
      {
        input_img[4*i] = img.range(7, 0);
        input_img[4*i+1] = img.range(15, 8);
        input_img[4*i+2] = img.range(23, 16);
        input_img[4*i+3] = img.range(31, 24);
        /*buffer_1[4*i] = img.range(7, 0);
        buffer_1[4*i+1] = img.range(15, 8);
        buffer_1[4*i+2] = img.range(23, 16);
        buffer_1[4*i+3] = img.range(31, 24);*/
      }
#else
		  img = i_img.read();
      {
        input_img[4*i] = img.range(7, 0);
        input_img[4*i+1] = img.range(15, 8);
        input_img[4*i+2] = img.range(23, 16);
        input_img[4*i+3] = img.range(31, 24);
        /*buffer_1[4*i] = img.range(7, 0);
        buffer_1[4*i+1] = img.range(15, 8);
        buffer_1[4*i+2] = img.range(23, 16);
        buffer_1[4*i+3] = img.range(31, 24);*/
      }
#endif      
    }
    #ifdef CONV_BUFF
      Fused_Buf_Conv1();
      Fused_Buf_Conv2();
    #else
      Fused_Conv1();
      Fused_Conv2();
      /*{
        HLS_PIPELINE_LOOP(SOFT_STALL, 1, "Loop");
        Fused_Conv1_1();
        Fused_Conv1_2();
      }
      {
        HLS_PIPELINE_LOOP(SOFT_STALL, 1, "Loop");
        Fused_Conv2_1();
        Fused_Conv2_2();
      }*/ // 6832450ns 17764.4area
    #endif
    
    #if defined(FC_PE_UNROLL)
      FC1_PE();
      FC2_PE();
      FC3_PE();
    #else
      {
        //HLS_PIPELINE_LOOP(SOFT_STALL, 1, "Loop");
        HLS_UNROLL_LOOP(ON);
        FC1_1();
        FC1_2();
      }
      {
        //HLS_PIPELINE_LOOP(SOFT_STALL, 1, "Loop");
        HLS_UNROLL_LOOP(ON);
        FC2_1();
        FC2_2();
      }
      FC3();
    #endif

    for(int i = 0; i < 10; i++) {
      sc_dt::sc_int<32> out = fc3_out[i];
#ifndef NATIVE_SYSTEMC
      {
        HLS_DEFINE_PROTOCOL("output");
        o_result.put(out);
        wait();
      }
#else
		  o_result.write(out);
#endif
    }
	}
}

/*void Lenet::Conv1() {

  int p_sum = 0, h, w;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV1_OUT_CHANNEL; m++) {
      for(int p = 0; p < CONV1_OUT_H; p++) {
        for(int q = 0; q < CONV1_OUT_W; q++) {
          p_sum = 0;
          for(int r = 0; r < CONV1_KERNEL_SIZE; r++){
            for(int s = 0; s < CONV1_KERNEL_SIZE; s++) {
              for(int c = 0; c < CONV1_IN_CHANNEL; c++) {
                h = p + r;
                w = q + s;
                p_sum += buffer_1[h*32 + w] * conv1_weight[m][c][r][s];
              }
            }
          }
          //conv1_out[n][m][p][q] = (((p_sum * 130) >> 16) > 0) ? ((p_sum * 130) >> 16) : 0;
          buffer_2[n*CONV1_OUT_CHANNEL*CONV1_OUT_H*CONV1_OUT_W + m*CONV1_OUT_H*CONV1_OUT_W + p*CONV1_OUT_W + q] = (((p_sum * 130) >> 16) > 0) ? ((p_sum * 130) >> 16) : 0;
        }
      }
    }
  }

  int max_val;

  cout << "MAXPOOL1\n";

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int c = 0; c < CONV1_OUT_CHANNEL; c++) {
      for(int h = 0; h < MAXPOOL_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL_OUT_W; w++) {
          //max_val = conv1_out[n][c][h*2][w*2];
          max_val = buffer_2[n*CONV1_OUT_CHANNEL*CONV1_OUT_H*CONV1_OUT_W + c*CONV1_OUT_H*CONV1_OUT_W + h*2*CONV1_OUT_W + w*2];
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              //if(max_val < conv1_out[n][c][h_][w_]) {
              sc_dt::sc_int<8> val = buffer_2[n*CONV1_OUT_CHANNEL*CONV1_OUT_H*CONV1_OUT_W + c*CONV1_OUT_H*CONV1_OUT_W + h_*CONV1_OUT_W + w_];
              if(max_val < val) {
                max_val = val;
              }
            }
          }
          //maxpool_out[n][c][h][w] = max_val;
          buffer_1[n*CONV1_OUT_CHANNEL*MAXPOOL_OUT_H*MAXPOOL_OUT_W + c*MAXPOOL_OUT_H*MAXPOOL_OUT_W + h*MAXPOOL_OUT_W + w] = max_val;
        }
      }
    }
  }
}*/

void Lenet::Fused_Conv1() {

  int p_sum = 0;

  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV1_OUT_CHANNEL; m++) {
      for(int h = 0; h < MAXPOOL_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL_OUT_W; w++) {
          max_val = 0;
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int r = 0; r < CONV1_KERNEL_SIZE; r++){
                for(int s = 0; s < CONV1_KERNEL_SIZE; s++) {
                  for(int c = 0; c < CONV1_IN_CHANNEL; c++) {
                    #if defined (CONV_II)
                      HLS_PIPELINE_LOOP(SOFT_STALL, CONV_II, "Loop");
                    #endif
                    #if defined (CONV_UNROLL)
                      HLS_UNROLL_LOOP(ON);
                    #endif
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    sc_dt::sc_int<8> data = input_img[in_h*32 + in_w];
                    sc_dt::sc_int<8> weight =conv1_weight[m][c][r][s];
                    {
                      HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
                      p_sum += data * weight;
                    }
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              } 
            }
          }
          max_val = (((max_val * 130) >> 16) > 0) ? ((max_val * 130) >> 16) : 0;
          buffer_2[n*CONV1_OUT_CHANNEL*MAXPOOL_OUT_H*MAXPOOL_OUT_W + m*MAXPOOL_OUT_H*MAXPOOL_OUT_W + h*MAXPOOL_OUT_W + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Fused_Conv1_1() {

  int p_sum = 0;

  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV1_OUT_CHANNEL; m++) {
      for(int h = 0; h < 10; h++) {
        for(int w = 0; w < MAXPOOL_OUT_W; w++) {
          max_val = 0;
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int r = 0; r < CONV1_KERNEL_SIZE; r++){
                for(int s = 0; s < CONV1_KERNEL_SIZE; s++) {
                  for(int c = 0; c < CONV1_IN_CHANNEL; c++) {
                    #if defined (CONV_II)
                      HLS_PIPELINE_LOOP(SOFT_STALL, CONV_II, "Loop");
                    #endif
                    #if defined (CONV_UNROLL)
                      HLS_UNROLL_LOOP(ON);
                    #endif
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    sc_dt::sc_int<8> data = input_img[in_h*32 + in_w];
                    sc_dt::sc_int<8> weight =conv1_weight[m][c][r][s];
                    {
                      HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
                      p_sum += data * weight;
                    }
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              } 
            }
          }
          max_val = (((max_val * 130) >> 16) > 0) ? ((max_val * 130) >> 16) : 0;
          buffer_2[n*CONV1_OUT_CHANNEL*10*MAXPOOL_OUT_W + m*10*MAXPOOL_OUT_W + h*MAXPOOL_OUT_W + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Fused_Conv1_2() {

  int p_sum = 0;

  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV1_OUT_CHANNEL; m++) {
      for(int h = 6; h < MAXPOOL_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL_OUT_W; w++) {
          max_val = 0;
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int r = 0; r < CONV1_KERNEL_SIZE; r++){
                for(int s = 0; s < CONV1_KERNEL_SIZE; s++) {
                  for(int c = 0; c < CONV1_IN_CHANNEL; c++) {
                    #if defined (CONV_II)
                      HLS_PIPELINE_LOOP(SOFT_STALL, CONV_II, "Loop");
                    #endif
                    #if defined (CONV_UNROLL)
                      HLS_UNROLL_LOOP(ON);
                    #endif
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    sc_dt::sc_int<8> data = input_img[in_h*32 + in_w];
                    sc_dt::sc_int<8> weight =conv1_weight[m][c][r][s];
                    {
                      HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
                      p_sum += data * weight;
                    }
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              } 
            }
          }
          max_val = (((max_val * 130) >> 16) > 0) ? ((max_val * 130) >> 16) : 0;
          buffer_4[(n)*CONV1_OUT_CHANNEL*8*MAXPOOL_OUT_W + m*8*MAXPOOL_OUT_W + h*MAXPOOL_OUT_W + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Fused_Buf_Conv1() {

  int p_sum = 0;

  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV1_OUT_CHANNEL; m++) {

      for(int i = 0; i < CONV1_KERNEL_SIZE; i++) {
        for(int j = 0; j < CONV1_KERNEL_SIZE; j++) {
          HLS_UNROLL_LOOP(ON);
          buffer_conv_weight[0][i][j] = conv1_weight[m][0][i][j];
        }
      }

      for(int h = 0; h < MAXPOOL_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL_OUT_W; w++) {

          if(w == 0) {
            for(int i = 0; i < 6; i++) {
              for(int j = 0; j < 6; j++) {
                HLS_UNROLL_LOOP(ON);
                int in_h = h*2 + i;
                int in_w = w*2 + j;
                buffer_conv_act[0][i][j] = buffer_1[in_h*32 + in_w];
              }
            } 
          }
          else {
            for(int i = 0; i < 6; i++) {
              for(int j = 0; j < 4; j++) {
                HLS_UNROLL_LOOP(ON);
                buffer_conv_act[0][i][j] = buffer_conv_act[0][i][j+2];
              }
            }
            for(int i = 0; i < 6; i++) {
              for(int j = 4; j < 6; j++) {
                HLS_UNROLL_LOOP(ON);
                int in_h = h*2 + i;
                int in_w = w*2 + j;
                buffer_conv_act[0][i][j] = buffer_1[in_h*32 + in_w];
              }
            }
          }

          max_val = 0;
          for(int h_ = 0; h_ < 2; h_++) {
            for(int w_ = 0; w_ < 2; w_++) {
              p_sum = 0;
              for(int r = 0; r < CONV1_KERNEL_SIZE; r++){
                for(int s = 0; s < CONV1_KERNEL_SIZE; s++) {
                  for(int c = 0; c < CONV1_IN_CHANNEL; c++) {
                    HLS_UNROLL_LOOP(ON);
                    p_sum += buffer_conv_act[c][h_ + r][w_ + s] * buffer_conv_weight[c][r][s];
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              } 
            }
          }
          max_val = (((max_val * 130) >> 16) > 0) ? ((max_val * 130) >> 16) : 0;
          buffer_2[n*CONV1_OUT_CHANNEL*MAXPOOL_OUT_H*MAXPOOL_OUT_W + m*MAXPOOL_OUT_H*MAXPOOL_OUT_W + h*MAXPOOL_OUT_W + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Fused_LineBuf_Conv1() {

  int p_sum = 0;

  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV1_OUT_CHANNEL; m++) {
      for(int i = 2; i <= CONV1_KERNEL_SIZE; i++) {
        for(int j = 0; j < CONV1_IN_W; j++) {
          line_buffer[i][j] = buffer_1[(i-2)*32 + j];;
        }
      }
      for(int h = 0; h < MAXPOOL_OUT_H; h++) {
        {
          for(int i = 2; i <= CONV1_KERNEL_SIZE; i++) {
            for(int j = 0; j < CONV1_IN_W; j++) {
              line_buffer[i-2][j] = line_buffer[i][j];
            }
          }

          for(int i = 0; i < CONV1_IN_W; i++) {
            int in_h = h*2 + CONV1_KERNEL_SIZE-1;
            int in_w = i;
            line_buffer[CONV1_KERNEL_SIZE-1][i] = buffer_1[in_h*32 + in_w];
            line_buffer[CONV1_KERNEL_SIZE][i] = buffer_1[(in_h+1)*32 + in_w];
          }
        }
        for(int w = 0; w < MAXPOOL_OUT_W; w++) {
          max_val = 0;
          for(int h_ = 0; h_ < 2; h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int r = 0; r < CONV1_KERNEL_SIZE; r++){
                for(int s = 0; s < CONV1_KERNEL_SIZE; s++) {
                  for(int c = 0; c < CONV1_IN_CHANNEL; c++) {
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    p_sum += line_buffer[in_h][in_w] * conv1_weight[m][c][r][s];
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              } 
            }
          }
          max_val = (((max_val * 130) >> 16) > 0) ? ((max_val * 130) >> 16) : 0;
          buffer_2[n*CONV1_OUT_CHANNEL*MAXPOOL_OUT_H*MAXPOOL_OUT_W + m*MAXPOOL_OUT_H*MAXPOOL_OUT_W + h*MAXPOOL_OUT_W + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Conv2() {

  int p_sum = 0, h, w;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV2_OUT_CHANNEL; m++) {
      for(int p = 0; p < CONV2_OUT_H; p++) {
        for(int q = 0; q < CONV2_OUT_W; q++) {
          p_sum = 0;
          for(int c = 0; c < CONV2_IN_CHANNEL; c++) {
            for(int r = 0; r < CONV2_KERENL; r++){
              for(int s = 0; s < CONV2_KERENL; s++) {
                h = p + r;
                w = q + s;
                sc_dt::sc_int<8> val = buffer_1[n*CONV1_OUT_CHANNEL*MAXPOOL_OUT_H*MAXPOOL_OUT_W + c*MAXPOOL_OUT_H*MAXPOOL_OUT_W + h*MAXPOOL_OUT_W + w];
                sc_dt::sc_int<8> weight = conv2_weight[m][c][r][s];
                p_sum += val * weight;
              }
            }
          }
          sc_dt::sc_int<8> out_act = (((p_sum * 89) >> 16) > 0) ? ((p_sum * 89) >> 16) : 0;
          buffer_2[n*CONV2_OUT_CHANNEL*CONV2_OUT_H*CONV2_OUT_W + m*CONV2_OUT_H*CONV2_OUT_W + q*CONV2_OUT_W + q] = out_act;
        }
      }
    }
  }

  sc_dt::sc_int<8> max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int c = 0; c < CONV2_OUT_CHANNEL; c++) {
      for(int h = 0; h < MAXPOOL2_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL2_OUT_W; w++) {
          max_val = buffer_2[n*CONV2_OUT_CHANNEL*CONV2_OUT_H*CONV2_OUT_W + c*CONV2_OUT_H*CONV2_OUT_W + h*2*CONV2_OUT_W + w*2];
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              sc_dt::sc_int<8> val = buffer_2[n*CONV2_OUT_CHANNEL*CONV2_OUT_H*CONV2_OUT_W + c*CONV2_OUT_H*CONV2_OUT_W + h_*CONV2_OUT_W + w_];
              max_val = (val > max_val) ? val : max_val;
            }
          }
          buffer_1[c*25 + h*5 + w] = max_val;
        }
      }
    }
  }

}

void Lenet::Fused_Conv2() {

  int p_sum = 0;
  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV2_OUT_CHANNEL; m++) {
      for(int h = 0; h < MAXPOOL2_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL2_OUT_W; w++) {
          max_val = 0;
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int c = 0; c < CONV2_IN_CHANNEL; c++) {
                for(int r = 0; r < CONV2_KERENL; r++){
                  for(int s = 0; s < CONV2_KERENL; s++) {
                    #if defined (CONV_II)
                      HLS_PIPELINE_LOOP(SOFT_STALL, CONV_II, "Loop");
                    #endif
                    #if defined (CONV_UNROLL)
                      HLS_UNROLL_LOOP(ON);
                    #endif
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    sc_dt::sc_int<8> val = buffer_2[n*CONV1_OUT_CHANNEL*MAXPOOL_OUT_H*MAXPOOL_OUT_W + c*MAXPOOL_OUT_H*MAXPOOL_OUT_W + in_h*MAXPOOL_OUT_W + in_w];
                    sc_dt::sc_int<8> weight = conv2_weight[m][c][r][s];
                    {
                      HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
                      p_sum += val * weight;
                    }
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              }
            }
          }
          max_val = (((max_val * 89) >> 16) > 0) ? ((max_val * 89) >> 16) : 0;
          if(m >= (CONV2_OUT_CHANNEL/2))
            buffer_3[(m)*25 + h*5 + w - 200] = max_val;
          else
            buffer_1[m*25 + h*5 + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Fused_Conv2_1() {

  int p_sum = 0;
  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV2_OUT_CHANNEL; m++) {
      for(int h = 0; h < 3; h++) {
        for(int w = 0; w < MAXPOOL2_OUT_W; w++) {
          max_val = 0;
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int c = 0; c < CONV2_IN_CHANNEL; c++) {
                for(int r = 0; r < CONV2_KERENL; r++){
                  for(int s = 0; s < CONV2_KERENL; s++) {
                    #if defined (CONV_II)
                      HLS_PIPELINE_LOOP(SOFT_STALL, CONV_II, "Loop");
                    #endif
                    #if defined (CONV_UNROLL)
                      HLS_UNROLL_LOOP(ON);
                    #endif
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    sc_dt::sc_int<8> val = buffer_2[n*CONV1_OUT_CHANNEL*10*MAXPOOL_OUT_W + c*10*MAXPOOL_OUT_W + in_h*MAXPOOL_OUT_W + in_w];
                    sc_dt::sc_int<8> weight = conv2_weight[m][c][r][s];
                    {
                      HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
                      p_sum += val * weight;
                    }
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              }
            }
          }
          max_val = (((max_val * 89) >> 16) > 0) ? ((max_val * 89) >> 16) : 0;
          if(m >= (CONV2_OUT_CHANNEL/2))
            buffer_3[(m)*25 + h*5 + w - 200] = max_val;
          else
            buffer_1[m*25 + h*5 + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Fused_Conv2_2() {

  int p_sum = 0;
  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV2_OUT_CHANNEL; m++) {
      for(int h = 3; h < MAXPOOL2_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL2_OUT_W; w++) {
          max_val = 0;
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int c = 0; c < CONV2_IN_CHANNEL; c++) {
                for(int r = 0; r < CONV2_KERENL; r++){
                  for(int s = 0; s < CONV2_KERENL; s++) {
                    #if defined (CONV_II)
                      HLS_PIPELINE_LOOP(SOFT_STALL, CONV_II, "Loop");
                    #endif
                    #if defined (CONV_UNROLL)
                      HLS_UNROLL_LOOP(ON);
                    #endif
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    sc_dt::sc_int<8> val = buffer_4[n*CONV1_OUT_CHANNEL*8*MAXPOOL_OUT_W + c*MAXPOOL_OUT_H*8 + in_h*MAXPOOL_OUT_W + in_w];
                    sc_dt::sc_int<8> weight = conv2_weight[m][c][r][s];
                    {
                      HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
                      p_sum += val * weight;
                    }
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              }
            }
          }
          max_val = (((max_val * 89) >> 16) > 0) ? ((max_val * 89) >> 16) : 0;
          if(m >= (CONV2_OUT_CHANNEL/2))
            buffer_3[(m)*25 + h*5 + w - 200] = max_val;
          else
            buffer_1[m*25 + h*5 + w] = max_val;
        }
      }
    }
  }
}

/*void Lenet::Fused_PE_Conv2() {

  int p_sum = 0;
  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV2_OUT_CHANNEL; m++) {
      for(int h = 0; h < MAXPOOL2_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL2_OUT_W; w++) {
          max_val = 0;
          for(int h_ = h*2; h_ < (h*2 + 2); h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int c = 0; c < CONV2_IN_CHANNEL; c++) {
                for(int r = 0; r < CONV2_KERENL; r++){
                  #if defined (CONV_II)
                    HLS_PIPELINE_LOOP(SOFT_STALL, CONV_II, "Loop");
                  #endif
                  #if defined (CONV_UNROLL)
                    HLS_UNROLL_LOOP(ON);
                  #endif
                  for(int s = 0; s < CONV2_KERENL; s++) {
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    sc_dt::sc_int<8> val = buffer_2[n*CONV1_OUT_CHANNEL*MAXPOOL_OUT_H*MAXPOOL_OUT_W + c*MAXPOOL_OUT_H*MAXPOOL_OUT_W + in_h*MAXPOOL_OUT_W + in_w];
                    sc_dt::sc_int<8> weight = conv2_weight[m][c][r][s];
                    {
                      HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
                      CONV_ACC[s] += val * weight;
                    }
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              }
            }
          }
          max_val = (((max_val * 89) >> 16) > 0) ? ((max_val * 89) >> 16) : 0;
          buffer_1[m*25 + h*5 + w] = max_val;
        }
      }
    }
  }
}*/

void Lenet::Fused_LineBuf_Conv2() {

  int p_sum = 0;
  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV2_OUT_CHANNEL; m++) {
      for(int k = 0; k < CONV1_OUT_CHANNEL; k++) {
        for(int i = 2; i <= CONV2_KERENL; i++) {
          HLS_UNROLL_LOOP(ON);
          for(int j = 0; j < MAXPOOL_OUT_W; j++) {
            HLS_UNROLL_LOOP(ON);
            line_buffer_2[k][i][j] = buffer_2[k*MAXPOOL_OUT_H*MAXPOOL_OUT_W + (i-2)*MAXPOOL_OUT_W + j];
          }
        }
      }
      for(int h = 0; h < MAXPOOL2_OUT_H; h++) {
        {
          for(int k = 0; k < CONV1_OUT_CHANNEL; k++) {
            HLS_UNROLL_LOOP(ON);
            for(int i = 2; i <= CONV2_KERENL; i++) {
              HLS_UNROLL_LOOP(ON);
              for(int j = 0; j < MAXPOOL_OUT_W; j++) {
                HLS_UNROLL_LOOP(ON);
                line_buffer_2[k][i-2][j] = line_buffer_2[k][i][j];
              }
            }
          }

          for(int k = 0; k < CONV1_OUT_CHANNEL; k++) {
            HLS_UNROLL_LOOP(ON);
            for(int i = 0; i < MAXPOOL_OUT_W; i++) {
              HLS_UNROLL_LOOP(ON);
              int in_h = h*2 + CONV2_KERENL-1;
              int in_w = i;
              line_buffer_2[k][CONV2_KERENL-1][i] = buffer_2[k*MAXPOOL_OUT_H*MAXPOOL_OUT_W + (in_h)*MAXPOOL_OUT_W + in_w];
              line_buffer_2[k][CONV2_KERENL][i] = buffer_2[k*MAXPOOL_OUT_H*MAXPOOL_OUT_W + (in_h+1)*MAXPOOL_OUT_W + in_w];
            }
          }
        }
        for(int w = 0; w < MAXPOOL2_OUT_W; w++) {
          max_val = 0;
          for(int h_ = 0; h_ < 2; h_++) {
            for(int w_ = w*2; w_ < (w*2 + 2); w_++) {
              p_sum = 0;
              for(int c = 0; c < CONV2_IN_CHANNEL; c++) {
                HLS_UNROLL_LOOP(ON);
                for(int r = 0; r < CONV2_KERENL; r++){
                  HLS_UNROLL_LOOP(ON);
                  for(int s = 0; s < CONV2_KERENL; s++) {
                    HLS_UNROLL_LOOP(ON);
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    p_sum += line_buffer_2[c][in_h][in_w] * conv2_weight[m][c][r][s];
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              }
            }
          }
          max_val = (((max_val * 89) >> 16) > 0) ? ((max_val * 89) >> 16) : 0;
          buffer_1[m*25 + h*5 + w] = max_val;
        }
      }
    }
  }
}

void Lenet::Fused_Buf_Conv2() {

  int p_sum = 0;
  int max_val;

  for(int n = 0; n < BATCH_SIZE; n++) {
    for(int m = 0; m < CONV2_OUT_CHANNEL; m++) {

      for(int k = 0; k < CONV1_OUT_CHANNEL; k++) {
        for(int i = 0; i < CONV1_KERNEL_SIZE; i++) {
          for(int j = 0; j < CONV1_KERNEL_SIZE; j++) {
            buffer_conv_weight[k][i][j] = conv2_weight[m][k][i][j];
          }
        }
      }

      for(int h = 0; h < MAXPOOL2_OUT_H; h++) {
        for(int w = 0; w < MAXPOOL2_OUT_W; w++) {
          max_val = 0;

          if(w == 0) {
            for(int k = 0; k < CONV1_OUT_CHANNEL; k++) {
              for(int i = 0; i < 6; i++) {
                for(int j = 0; j < 6; j++) {
                  int in_h = h*2 + i;
                  int in_w = w*2 + j;
                  buffer_conv_act[k][i][j] = buffer_2[k*MAXPOOL_OUT_H*MAXPOOL_OUT_W + in_h*MAXPOOL_OUT_W + in_w];
                }
              } 
            }
          }
          else {
            for(int k = 0; k < CONV1_OUT_CHANNEL; k++) {
              for(int i = 0; i < 6; i++) {
                for(int j = 0; j < 4; j++) {
                  buffer_conv_act[k][i][j] = buffer_conv_act[k][i][j+2];
                }
              }
            }
            for(int k = 0; k < CONV1_OUT_CHANNEL; k++) {
              for(int i = 0; i < 6; i++) {
                for(int j = 4; j < 6; j++) {
                  int in_h = h*2 + i;
                  int in_w = w*2 + j;
                  buffer_conv_act[k][i][j] = buffer_2[k*MAXPOOL_OUT_H*MAXPOOL_OUT_W + in_h*MAXPOOL_OUT_W + in_w];
                }
              }
            }
          }
          for(int h_ = 0; h_ < 2; h_++) {
            for(int w_ = 0; w_ < 2; w_++) {
              p_sum = 0;
              for(int c = 0; c < CONV2_IN_CHANNEL; c++) {
                for(int r = 0; r < CONV2_KERENL; r++){
                  for(int s = 0; s < CONV2_KERENL; s++) {
                    int in_h = h_ + r;
                    int in_w = w_ + s;
                    p_sum += buffer_conv_act[c][in_h][in_w] * buffer_conv_weight[c][r][s];
                  }
                }
              }
              if(max_val < p_sum) {
                max_val = p_sum;
              }
            }
          }
          max_val = (((max_val * 89) >> 16) > 0) ? ((max_val * 89) >> 16) : 0;
          buffer_1[m*25 + h*5 + w] = max_val;
        }
      }
    }
  }
}

/*void Lenet::FC1() {

  int p_sum;

  for(int c = 0; c < 120; c++) {
    p_sum = 0;
    for(int w = 0; w < 400; w++) {
      #if defined (FC_II)
        HLS_PIPELINE_LOOP(SOFT_STALL, FC_II, "Loop");
        //HLS_UNROLL_LOOP(ON);
      #endif
      sc_dt::sc_int<8> data = buffer_1[w];
      sc_dt::sc_int<8> weight = fc1_weight[c][w];
      {
        HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
        p_sum += data * weight;
      }
    }
    //fc2_in[c] = ((p_sum * 335) >> 16) > 0 ? ((p_sum * 335) >> 16) : 0;
    buffer_2[c] = ((p_sum * 335) >> 16) > 0 ? ((p_sum * 335) >> 16) : 0;
  }
}*/

void Lenet::FC1_1() {

  int p_sum;
  for(int c = 0; c < 60; c++) {
    p_sum = 0;
    for(int w = 0; w < 400; w++) {
      #if defined (FC_II)
        HLS_PIPELINE_LOOP(SOFT_STALL, FC_II, "Loop");
        //HLS_UNROLL_LOOP(ON);
      #endif
      sc_dt::sc_int<8> data;
      sc_dt::sc_int<8> weight = fc1_weight[c][w];
      if(w < 200) {
        data = buffer_1[w];
      }
      else {
        data = buffer_3[w-200];
      }
      {
        HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
        p_sum += data * weight;
      }
    }
    //fc2_in[c] = ((p_sum * 335) >> 16) > 0 ? ((p_sum * 335) >> 16) : 0;
    buffer_2[c] = ((p_sum * 335) >> 16) > 0 ? ((p_sum * 335) >> 16) : 0;
  }
}

void Lenet::FC1_2() {
  int p_sum;
  for(int c = 60; c < 120; c++) {
    p_sum = 0;
    for(int w = 0; w < 400; w++) {
      #if defined (FC_II)
        HLS_PIPELINE_LOOP(SOFT_STALL, FC_II, "Loop");
        //HLS_UNROLL_LOOP(ON);
      #endif
      sc_dt::sc_int<8> data;
      sc_dt::sc_int<8> weight = fc1_weight[c][w];
      if(w < 200) {
        data = buffer_1[w];
      }
      else {
        data = buffer_3[w-200];
      }
      {
        HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
        p_sum += data * weight;
      }
    }
    //fc2_in[c] = ((p_sum * 335) >> 16) > 0 ? ((p_sum * 335) >> 16) : 0;
    buffer_4[c-60] = ((p_sum * 335) >> 16) > 0 ? ((p_sum * 335) >> 16) : 0;
  }
}

void Lenet::FC1_PE() {

  for(int c = 0; c < 120; c+=PE_SIZE) {
    for(int i = 0; i < PE_SIZE; i++) {
      HLS_UNROLL_LOOP(ON);
      FC_ACC[i] = 0;
    }
    for(int w = 0; w < 400; w++) {
      for(int k = 0; k < PE_SIZE; k++) {
        HLS_UNROLL_LOOP(ON);
        sc_dt::sc_int<8> data = buffer_1[w];
        sc_dt::sc_int<8> weight = fc1_weight[c+k][w];
        {
          HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
          FC_ACC[k] += data * weight;
        }
      }
    }

    for(int i = 0; i < PE_SIZE; i++) {
      HLS_PIPELINE_LOOP(SOFT_STALL, 1, "Loop");
      if((c+i) < 120)
        buffer_2[c+i] = ((FC_ACC[i] * 335) >> 16) > 0 ? ((FC_ACC[i] * 335) >> 16) : 0;
    }
  }
}

void Lenet::FC2() {

  int p_sum;

  for(int c = 0; c < 84; c++) {
    p_sum = 0;
    for(int w = 0; w < 120; w++) {
      #if defined (FC_II)
        HLS_PIPELINE_LOOP(SOFT_STALL, FC_II, "Loop");
        //HLS_UNROLL_LOOP(ON);
      #endif
      sc_dt::sc_int<8> data;
      if(w < 60) {
        data = buffer_2[w];
      }
      else {
        data = buffer_4[w-60];
      }
      sc_dt::sc_int<8> weight = fc2_weight[c][w];
      {
        HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
        p_sum += data * weight;
      }
    }
    //fc3_in[c] = ((p_sum * 318) >> 16) > 0 ? ((p_sum * 318) >> 16) : 0;
    buffer_1[c] = ((p_sum * 318) >> 16) > 0 ? ((p_sum * 318) >> 16) : 0;
  }
}

void Lenet::FC2_1() {

  int p_sum;

  for(int c = 0; c < 42; c++) {
    p_sum = 0;
    for(int w = 0; w < 120; w++) {
      #if defined (FC_II)
        HLS_PIPELINE_LOOP(SOFT_STALL, FC_II, "Loop");
        //HLS_UNROLL_LOOP(ON);
      #endif
      sc_dt::sc_int<8> data;
      if(w < 60) {
        data = buffer_2[w];
      }
      else {
        data = buffer_4[w-60];
      }
      sc_dt::sc_int<8> weight = fc2_weight[c][w];
      {
        HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
        p_sum += data * weight;
      }
    }
    //fc3_in[c] = ((p_sum * 318) >> 16) > 0 ? ((p_sum * 318) >> 16) : 0;
    buffer_1[c] = ((p_sum * 318) >> 16) > 0 ? ((p_sum * 318) >> 16) : 0;
  }
}

void Lenet::FC2_2() {

  int p_sum;

  for(int c = 42; c < 84; c++) {
    p_sum = 0;
    for(int w = 0; w < 120; w++) {
      #if defined (FC_II)
        HLS_PIPELINE_LOOP(SOFT_STALL, FC_II, "Loop");
        //HLS_UNROLL_LOOP(ON);
      #endif
      sc_dt::sc_int<8> data;
      if(w < 60) {
        data = buffer_2[w];
      }
      else {
        data = buffer_4[w-60];
      }
      sc_dt::sc_int<8> weight = fc2_weight[c][w];
      {
        HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
        p_sum += data * weight;
      }
    }
    //fc3_in[c] = ((p_sum * 318) >> 16) > 0 ? ((p_sum * 318) >> 16) : 0;
    buffer_3[c-42] = ((p_sum * 318) >> 16) > 0 ? ((p_sum * 318) >> 16) : 0;
  }
}

void Lenet::FC2_PE() {

  for(int c = 0; c < 84; c += PE_SIZE) {
    for(int i = 0; i < PE_SIZE; i++) {
      HLS_UNROLL_LOOP(ON);
      FC_ACC[i] = 0;
    }
    for(int w = 0; w < 120; w++) {
      for(int k = 0; k < PE_SIZE; k++) {
        HLS_UNROLL_LOOP(ON);
        if((c+k) < 84) {
          sc_dt::sc_int<8> data = buffer_2[w];
          sc_dt::sc_int<8> weight = fc2_weight[c+k][w];
          {
            HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
            FC_ACC[k] += data * weight;
          }
        }
      }
    }
    for(int i = 0; i < PE_SIZE; i++) {
      HLS_PIPELINE_LOOP(SOFT_STALL, 1, "Loop");
      if((c+i) < 10)
        buffer_1[c+i] = ((FC_ACC[i] * 318) >> 16) > 0 ? ((FC_ACC[i] * 318) >> 16) : 0;
    }
  }
}

void Lenet::FC3() {

  int p_sum;

  for(int c = 0; c < 10; c++) {
    p_sum = 0;
    for(int w = 0; w < 84; w++) {
      #if defined (FC_II)
        HLS_PIPELINE_LOOP(SOFT_STALL, FC_II, "Loop");
        // HLS_UNROLL_LOOP(ON);
      #endif
      sc_dt::sc_int<8> data;
      sc_dt::sc_int<8> weight = fc3_weight[c][w];
      if(w < 42) {
        data = buffer_1[w];
      }
      else {
        data = buffer_3[w-42];
      }
      {
        HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
        p_sum += data * weight;
      }
    }
    fc3_out[c] = p_sum;
    //cout << p_sum << "\n";
  }
}

void Lenet::FC3_PE() {

  for(int c = 0; c < 10; c += PE_SIZE) {
    for(int i = 0; i < PE_SIZE; i++) {
      HLS_UNROLL_LOOP(ON);
      FC_ACC[i] = 0;
    }
    for(int w = 0; w < 84; w++) {
      for(int k = 0; k < PE_SIZE; k++) {
        HLS_UNROLL_LOOP(ON);
        if((c+k) < 10) {
          sc_dt::sc_int<8> data = buffer_1[w];
          sc_dt::sc_int<8> weight = fc3_weight[c+k][w];
          {
            HLS_CONSTRAIN_LATENCY( 0, 1, "MAC");
            FC_ACC[k] += data * weight;
          }
        }
      }
    }

    for(int i = 0; i < PE_SIZE; i++) {
      HLS_PIPELINE_LOOP(SOFT_STALL, 1, "Loop");
      if((c+i) < 10) {
        fc3_out[c+i] = FC_ACC[i];
        //cout << FC_ACC[i] << "\n";
      }
    }
  }
}