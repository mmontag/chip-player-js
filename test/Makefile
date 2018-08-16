# Add -I flags if you need them
CXXFLAGS := -O2
SRCS := basics.c Wave_Writer.cpp
SRCS_MEM := basics_mem.c Wave_Writer.cpp
INCLUDES := ../gme/
LIBRARIES := ../build/gme/
TEST_FILES := ../test.nsf  # Add more files here that you want in testsuite

all: demo demo_mem

# We will use LD_PRELOAD later to pick up the right libgme
demo: $(SRCS) Wave_Writer.h
	$(CXX) -I$(INCLUDES)  $(CXXFLAGS) -o $@ $(SRCS) -L$(LIBRARIES) -lgme

demo_mem: $(SRCS_MEM) Wave_Writer.h
	$(CXX) -I$(INCLUDES) $(CXXFLAGS) -o $@ $(SRCS_MEM) -L$(LIBRARIES) -lgme

test: demo demo_mem
	parallel --bar ./test.sh {} ::: $(TEST_FILES)

clean:
	rm -f demo
	rm -f demo_mem
	rm -f new/*.out cur/*.out
	rm -f newm/*.out curm/*.out
	rmdir new cur newm curm

