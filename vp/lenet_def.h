#ifndef LENET_DEF_H_
#define LENET_DEF_H_

#define CLOCK_PERIOD 10

#define MAX_IMAGE_BUFFER_LENTH 1024
#define THRESHOLD 90

// lenet parameters
const int DMA_TRANS = 64;

// Lenet Filter inner transport addresses
// Used between blocking_transport() & do_filter()
const int LENET_FILTER_R_ADDR = 0x00000000;
const int LENET_FILTER_RESULT_ADDR = 0x00000004;

const int LENET_FILTER_RS_R_ADDR   = 0x00000000;
const int LENET_FILTER_RS_W_WIDTH  = 0x00000004;
const int LENET_FILTER_RS_W_HEIGHT = 0x00000008;
const int LENET_FILTER_RS_W_DATA   = 0x0000000C;
const int LENET_FILTER_RS_RESULT_ADDR = 0x00800000;


union word {
  int sint;
  unsigned int uint;
  unsigned char uc[4];
};

#endif
