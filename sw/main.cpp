#include "stdio.h"
#include "string.h"
#include "math.h"
#include "stdint.h"
#include "image.h"
#include "assert.h"
#include "irq.h"
//#define DMA_HANDLER_DEBUG 1

static void set_next_timer_interrupt() {
	assert (mtime && mtimecmp);
	*mtimecmp = *mtime + 1000;  // 1000 timer ticks, corresponds to 1 MS delay with current CLINT configuration
}

unsigned int num_ticks = 0;

bool acc_completed[4] = {false};

int cnt = 0;

int total_error = 0;

bool _is_using_dma = true;

void timer_irq_handler() {
	set_next_timer_interrupt();
	++num_ticks;
}

void acc_irq_handler_0() {
	acc_completed[0] = 1;

	#ifdef DMA_HANDLER_DEBUG
		uint32_t hart_id;
		asm volatile ("csrr %0, mhartid" : "=r"(hart_id));
		printf("dma handle(0) %u\n", hart_id);
	#endif
}

void acc_irq_handler_1() {
	acc_completed[1] = 1;

	#ifdef DMA_HANDLER_DEBUG
		uint32_t hart_id;
		asm volatile ("csrr %0, mhartid" : "=r"(hart_id));
		printf("dma handle(1) %u\n", hart_id);
	#endif
}

void acc_irq_handler_2() {
	acc_completed[2] = 1;

	#ifdef DMA_HANDLER_DEBUG
		uint32_t hart_id;
		asm volatile ("csrr %0, mhartid" : "=r"(hart_id));
		printf("dma handle(2) %u\n", hart_id);
	#endif
}

void acc_irq_handler_3() {
	acc_completed[3] = 1;

	#ifdef DMA_HANDLER_DEBUG
		uint32_t hart_id;
		asm volatile ("csrr %0, mhartid" : "=r"(hart_id));
		printf("dma handle(3) %u\n", hart_id);
	#endif
}

void init_acc_irq_handler() {
	register_interrupt_handler(6, acc_irq_handler_0);
	register_interrupt_handler(8, acc_irq_handler_1);
	register_interrupt_handler(9, acc_irq_handler_2);
	register_interrupt_handler(10, acc_irq_handler_3);
}

int sem_init (uint32_t *__sem, uint32_t count) __THROW
{
  *__sem=count;
  return 0;
}

int sem_wait (uint32_t *__sem) __THROW
{
  uint32_t value, success; //RV32A
  __asm__ __volatile__("\
L%=:\n\t\
     lr.w %[value],(%[__sem])            # load reserved\n\t\
     beqz %[value],L%=                   # if zero, try again\n\t\
     addi %[value],%[value],-1           # value --\n\t\
     sc.w %[success],%[value],(%[__sem]) # store conditionally\n\t\
     bnez %[success], L%=                # if the store failed, try again\n\t\
"
    : [value] "=r"(value), [success]"=r"(success)
    : [__sem] "r"(__sem)
    : "memory");
  return 0;
}

int sem_post (uint32_t *__sem) __THROW
{
  uint32_t value, success; //RV32A
  __asm__ __volatile__("\
L%=:\n\t\
     lr.w %[value],(%[__sem])            # load reserved\n\t\
     addi %[value],%[value], 1           # value ++\n\t\
     sc.w %[success],%[value],(%[__sem]) # store conditionally\n\t\
     bnez %[success], L%=                # if the store failed, try again\n\t\
"
    : [value] "=r"(value), [success]"=r"(success)
    : [__sem] "r"(__sem)
    : "memory");
  return 0;
}

int barrier(uint32_t *__sem, uint32_t *__lock, uint32_t *counter, uint32_t thread_count) {
	sem_wait(__lock);
	if (*counter == thread_count - 1) { //all finished
		*counter = 0;
		sem_post(__lock);
		for (int j = 0; j < thread_count - 1; ++j) sem_post(__sem);
	} else {
		(*counter)++;
		sem_post(__lock);
		sem_wait(__sem);
	}
	return 0;
}

//Total number of cores
//static const int PROCESSORS = 2;
#define PROCESSORS 4
//the barrier synchronization objects
uint32_t barrier_counter=0; 
uint32_t barrier_lock; 
uint32_t barrier_sem; 
uint32_t print_lock;
uint32_t dma_lock; 
//the mutex object to control global summation
uint32_t lock;  
//print synchronication semaphore (print in core order)
uint32_t print_sem[PROCESSORS]; 
//global summation variable
float pi_over_4 = 0;

// Lenet ACC
static char* const  Lenet_START_ADDR[4] = {
	reinterpret_cast<char* const>(0x73000000),
	reinterpret_cast<char* const>(0x74000000),
	reinterpret_cast<char* const>(0x75000000),
	reinterpret_cast<char* const>(0x76000000)
};

static char* const  Lenet_READ_ADDR[4] = {
	reinterpret_cast<char* const>(0x73000004),
	reinterpret_cast<char* const>(0x74000004),
	reinterpret_cast<char* const>(0x75000004),
	reinterpret_cast<char* const>(0x76000004)
};

// DMA 
static volatile uint32_t * const DMA_SRC_ADDR  = (uint32_t * const)0x70000000;
static volatile uint32_t * const DMA_DST_ADDR  = (uint32_t * const)0x70000004;
static volatile uint32_t * const DMA_LEN_ADDR  = (uint32_t * const)0x70000008;
static volatile uint32_t * const DMA_OP_ADDR   = (uint32_t * const)0x7000000C;
static volatile uint32_t * const DMA_STAT_ADDR = (uint32_t * const)0x70000010;
static const uint32_t DMA_OP_MEMCPY = 1;

union word {
  int sint;
  unsigned int uint;
  unsigned char uc[4];
};

void write_data_to_ACC(char* ADDR, unsigned char* buffer, int len){
  if(_is_using_dma){  
    // Using DMA 
    *DMA_SRC_ADDR = (uint32_t)(buffer);
    *DMA_DST_ADDR = (uint32_t)(ADDR);
    *DMA_LEN_ADDR = len;
    *DMA_OP_ADDR  = DMA_OP_MEMCPY;
  }else{
    // Directly Send
    memcpy(ADDR, buffer, sizeof(unsigned char)*len);
  }
}
void read_data_from_ACC(char* ADDR, unsigned char* buffer, int len){
  if(_is_using_dma){
    // Using DMA 
    *DMA_SRC_ADDR = (uint32_t)(ADDR);
    *DMA_DST_ADDR = (uint32_t)(buffer);
    *DMA_LEN_ADDR = len;
    *DMA_OP_ADDR  = DMA_OP_MEMCPY;
  }else{
    // Directly Read
    memcpy(buffer, ADDR, sizeof(unsigned char)*len);
  }
}

int answer[] = {
	-20015,
	-13807,
	-11529,
	-10469,
	-13066,
	-11872,
	-23042,
	9605,
	-15948,
	-7051
};

int main(unsigned hart_id) {

	unsigned char local_buffer[10*4];

	word data;
	
	int cur_id;

	int local_error = 0;

	////////////////////////////////
	// register interrupt handler //
	////////////////////////////////
	if(hart_id == 0) {
		register_timer_interrupt_handler(timer_irq_handler);
		set_next_timer_interrupt();
		init_acc_irq_handler();
	}

	/////////////////////////////
	// thread and barrier init //
	/////////////////////////////
	if (hart_id == 0) {
		// create a barrier object with a count of PROCESSORS
		sem_init(&barrier_lock, 1);
		sem_init(&barrier_sem, 0); //lock all cores initially
		for(int i=0; i< PROCESSORS; ++i){
			sem_init(&print_sem[i], 0); //lock printing initially
		}
		// Create mutex lock
		sem_init(&lock, 1);
		sem_init(&dma_lock, 1);
		sem_init(&print_lock, 1);
	}

	if(hart_id == 0) {
		printf("start ticks : %d", num_ticks);
	}

	///////////////////////
	// Lenet Acclerator //
	//////////////////////

	//if(hart_id < 1) {
	while(cnt < 10) {
		sem_wait(&lock);
		cur_id = cnt;
		cnt++;
		sem_post(&lock);

		sem_wait(&dma_lock);
		write_data_to_ACC(Lenet_START_ADDR[hart_id], input_img, 1024);
		sem_post(&dma_lock);

		while(!acc_completed[hart_id]);
		acc_completed[hart_id] = 0;

		sem_wait(&dma_lock);
		read_data_from_ACC(Lenet_READ_ADDR[hart_id], local_buffer, 40);
		sem_post(&dma_lock);

		for(int i = 0; i < 10; i++) {
			data.uc[0] = local_buffer[i*4+0];
			data.uc[1] = local_buffer[i*4+1];
			data.uc[2] = local_buffer[i*4+2];
			data.uc[3] = local_buffer[i*4+3];
			if(answer[i] != data.sint) {
				local_error++;
			}
		}
	}		

	sem_wait(&lock);
	total_error += local_error;
	sem_post(&lock);

	////////////////////////////
	// barrier to synchronize //
	////////////////////////////
	//Wait for all threads to finish
	barrier(&barrier_sem, &barrier_lock, &barrier_counter, PROCESSORS);

	if (hart_id == 0) {  // Core 0 print first and then others
		printf("num_ticks %d\n", num_ticks);
		printf("total error num = %d\n", total_error);
		printf("core%d has %d errors\n", hart_id, local_error);
		sem_post(&print_sem[1]);  // Allow Core 1 to print
	} else {
		sem_wait(&print_sem[hart_id]); 
		printf("core%d has %d errors\n", hart_id, local_error);
		sem_post(&print_sem[hart_id + 1]);  // Allow next Core to print
	}

	return 0;
}
