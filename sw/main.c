#include "stdio.h"
#include "string.h"
#include "math.h"
#include "stdint.h"

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
#define PROCESSORS 2
//the barrier synchronization objects
uint32_t barrier_counter=0; 
uint32_t barrier_lock; 
uint32_t barrier_sem; 
//the mutex object to control global summation
uint32_t lock;  
//print synchronication semaphore (print in core order)
uint32_t print_sem[PROCESSORS]; 
//global summation variable
float pi_over_4 = 0;

int main(unsigned hart_id) {

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
	}

	/////////////////////////
	// calculate local sum //
	/////////////////////////
	unsigned total_terms = 5000;
	unsigned nc = 2;

	unsigned num_terms_per_thread = (total_terms + nc - 1) / nc;
	unsigned begin_idx = num_terms_per_thread * hart_id;
	unsigned end_idx;

	if (hart_id == nc - 1) {
		end_idx = total_terms;
	} else {
		end_idx = begin_idx + num_terms_per_thread;
	}

	double factor = begin_idx % 2 ? -1.0 : 1.0;

	double local_sum = 0;
	for (int i = begin_idx; i < end_idx; i++, factor = -factor) {
		local_sum += factor / (2 * i + 1);
	}

	/////////////////////////////////////////
	// accumulate local sum to shared data //
	/////////////////////////////////////////
	sem_wait(&lock);
	pi_over_4 += local_sum;
	printf("core%d, current partial Pi = %f, current accumulated Pi = %f\n", hart_id, local_sum*4, pi_over_4*4);
	sem_post(&lock);

	////////////////////////////
	// barrier to synchronize //
	////////////////////////////
	//Wait for all threads to finish
	barrier(&barrier_sem, &barrier_lock, &barrier_counter, PROCESSORS);

	if (hart_id == 0) {  // Core 0 print first and then others
		printf("core%d, combined, Pi = %f\n", hart_id, (pi_over_4) * 4);
		printf("core%d, partial Pi = %f\n", hart_id, local_sum*4);

		sem_post(&print_sem[1]);  // Allow Core 1 to print
	} else {
		for (int i = 1; i < PROCESSORS; ++i) {
			sem_wait(&print_sem[i]); 
			printf("core%d, partial Pi = %f\n", hart_id, local_sum*4);
			sem_post(&print_sem[i + 1]);  // Allow next Core to print
		}
	}

	return 0;
}
