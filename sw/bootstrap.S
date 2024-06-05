.globl _start
.globl main

.equ SYSCALL_ADDR, 0x02010000
.equ PLIC_ENABLED_IRQ_ADDR, 0x40002000

# NOTE: this will exit the whole simulation, i.e. stop all harts
.macro SYS_EXIT, exit_code
li   a7, 93
li   a0, \exit_code
li   t0, SYSCALL_ADDR
csrr a6, mhartid
sw   a6, 0(t0)
.endm


# NOTE: each core will start here with execution
_start:

# initialize global pointer (see crt0.S of the RISC-V newlib C-library port)
.option push
.option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
.option pop

csrr a0, mhartid   # return a core specific number 0 or 1
li t0, 0
beq a0, t0, core0
li t0, 1
beq a0, t0, core1
li t0, 2
beq a0, t0, core2
li t0, 3
beq a0, t0, core3
core0:
  la sp, stack0_end  # code executed only by core0
  j setup_interrupts
core1:
  la sp, stack1_end  # code executed only by core1
  j end
core2:
  la sp, stack2_end  # code executed only by core2
  j end
core3:
  la sp, stack3_end  # code executed only by core3
  j end

setup_interrupts:
  # Set up the mtvec register to point to the interrupt handler
  la t0, level_0_interrupt_handler
  csrw mtvec, t0
  li t1, 0x888
  csrw mie, t1
  csrwi mstatus, 8
  li t0, PLIC_ENABLED_IRQ_ADDR
  li t1, -1
  sw t1, 0(t0)
  sw t1, 4(t0)
  j end

end:
  csrr a0, mhartid 
  jal main

# wait until all two cores have finished
la t0, exit_counter
li t1, 1
li t2, 3
amoadd.w a0, t1, 0(t0)

1:
  blt a0, t2, 1b

# call exit (SYS_EXIT=93) with exit code 0 (argument in a0)
SYS_EXIT 0


/*
 * Interrupt handler for non-nested interrupts. Only selected registers are stored/re-stored, i.e. those not preserved on function calls.
 */
#define STORE    sw
#define LOAD     lw
#define REGBYTES 4

.align 4
level_0_interrupt_handler:
// store execution context on the stack (register content)
addi    sp, sp, -REGBYTES * 32
STORE	x1, 0x0(sp)
STORE	x4, 3 * REGBYTES(sp)
STORE	x5, 4 * REGBYTES(sp)
STORE	x6, 5 * REGBYTES(sp)
STORE	x7, 6 * REGBYTES(sp)
STORE	x10, 9 * REGBYTES(sp)
STORE	x11, 10 * REGBYTES(sp)
STORE	x12, 11 * REGBYTES(sp)
STORE	x13, 12 * REGBYTES(sp)
STORE	x14, 13 * REGBYTES(sp)
STORE	x15, 14 * REGBYTES(sp)
STORE	x16, 15 * REGBYTES(sp)
STORE	x17, 16 * REGBYTES(sp)
STORE	x28, 27 * REGBYTES(sp)
STORE	x29, 28 * REGBYTES(sp)
STORE	x30, 29 * REGBYTES(sp)
STORE	x31, 30 * REGBYTES(sp)

// load interrupt/trap reason and call external C function to handle it
csrr    a0, mcause
jal     level_1_interrupt_handler

// re-store the saved context
LOAD	x1, 0x0(sp)
LOAD	x4, 3 * REGBYTES(sp)
LOAD	x5, 4 * REGBYTES(sp)
LOAD	x6, 5 * REGBYTES(sp)
LOAD	x7, 6 * REGBYTES(sp)
LOAD	x10, 9 * REGBYTES(sp)
LOAD	x11, 10 * REGBYTES(sp)
LOAD	x12, 11 * REGBYTES(sp)
LOAD	x13, 12 * REGBYTES(sp)
LOAD	x14, 13 * REGBYTES(sp)
LOAD	x15, 14 * REGBYTES(sp)
LOAD	x16, 15 * REGBYTES(sp)
LOAD	x17, 16 * REGBYTES(sp)
LOAD	x28, 27 * REGBYTES(sp)
LOAD	x29, 28 * REGBYTES(sp)
LOAD	x30, 29 * REGBYTES(sp)
LOAD	x31, 30 * REGBYTES(sp)
addi	sp, sp, REGBYTES * 32
mret

.align 8
stack0_begin:
.zero 32768
stack0_end:

.align 8
stack1_begin:
.zero 32768
stack1_end:

.align 8
stack2_begin:
.zero 32768
stack2_end:

.align 8
stack3_begin:
.zero 32768
stack3_end:

exit_counter:
.word 0
