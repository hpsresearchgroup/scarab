COMMON_LIB_DIR = ../../pin_lib
PIN_EXEC_DIR = ..
SCARAB_DIR = $(abspath ../../..)
PIN_EXEC_TOOL_PATH = $(abspath $(PIN_EXEC_DIR)/obj-intel64/rollback.so)
TEST_BINARY = test_binary
SIMPLE_LOOP = simple_loop

CC_FLAGS := -lgtest -std=c++14  -I$(SCARAB_DIR) -L $(COMMON_LIB_DIR)/obj -lpin_fe -DPIN_EXEC_TOOL_PATH=\"$(PIN_EXEC_TOOL_PATH)\" -DSIMPLE_LOOP=\"$(SIMPLE_LOOP)\"

ASSEMBLY_SOURCES=$(wildcard *.s)
CC_SOURCES=$(wildcard *.cc)
CC_HEADERS=$(wildcard *.h)
TEST_BINARIES=$(patsubst %.s,%,$(ASSEMBLY_SOURCES))

all: $(TEST_BINARY) $(TEST_BINARIES) pin_exec ## Builds and runs all the unit tests 
	./$(TEST_BINARY)

%:%.s
	as -o $@.o $^
	ld -o $@ $@.o
	rm $@.o

$(TEST_BINARY): $(CC_SOURCES) $(CC_HEADERS) pin_lib ## Builds the unit test binary
	g++ $(CC_SOURCES) -o $@ $(CC_FLAGS)

pin_lib:
	make -C $(COMMON_LIB_DIR)

pin_exec:
	make -C $(PIN_EXEC_DIR)

clean:
	rm test_binary
