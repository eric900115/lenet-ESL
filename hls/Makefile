### Define constants ###
OPTIMIZE_FLAG=-O3
DEBUG_FLAG=-g
USER_DEFINE_FLAGS+=-D NATIVE_SYSTEMC
USER_DEFINE_FLAGS+=-D CLOCK_PERIOD=10.0

CXXFLAGS+=$(OPTIMIZE_FLAG) $(DEBUG_FLAG)
CXXFLAGS+=$(USER_DEFINE_FLAGS)
CXXFLAGS+=-MMD -MP -O3
CXXFLAGS+=-fpermissive

SYSTEMC_HOME?=$(HOME)/usr/local/systemc-2.3.1
SYSTEMC_LIB = lib-linux64

LDLIBS+= -lsystemc
LDFLAGS+= -Wl,-rpath,$(SYSTEMC_HOME)/$(SYSTEMC_LIB)

### If you want to keep the compilation/linking errors ###
ifeq ($(ERRLOG), 1)
COMPILER_ERROR = 2> compiler.error
LINKER_ERROR = 2> linker.error
else
COMPILER_ERROR = 
LINKER_ERROR = 
endif

#Included headers 
INCLUDE_DIRS = -I. 
INCLUDE_DIRS += -I./common
INCLUDE_DIRS += -I$(SYSTEMC_HOME)/include

#Included libraries
LIB_DIRS = -L. 
LIB_DIRS += -L$(SYSTEMC_HOME)/$(SYSTEMC_LIB)

### Definitions ###
executives=lenet
output=out.bmp
sources= main.cpp Testbench.cpp Lenet.cpp System.cpp
objects= $(subst .cpp,.o,$(sources))
dependencies= $(subst .cpp,.d,$(sources))
	
### what to do? ###
all: lenet

lenet: $(objects)
	$(CXX) $(LDFLAGS) $(LIB_DIRS) $(objects) -o $@ $(LINKER_ERROR) $(LDLIBS) 

$(objects):%.o:%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@ $(COMPILER_ERROR)

-include $(sources:.cpp=.d)

### phony ###
run: lenet
	./lenet lena_std_short.bmp out.bmp

${output}: run

check: ${output}
	diff ${output} golden/lena_std_short_sobel.bmp

clean: 
	rm -f $(executives) $(objects) $(dependencies) $(COMPILER_ERROR) $(LINKER_ERROR) ${output}
