OBJECTS  = main.o bootstrap.o irq.o
CFLAGS   = -march=rv32ima -mabi=ilp32
LDFLAGS  = -nostartfiles -Wl,--no-relax

#VP       = $(VPPATH)/tiny32-mc
VP_FLAGS = --intercept-syscalls --error-on-zero-traphandler=true 

include ../Makefile.common
