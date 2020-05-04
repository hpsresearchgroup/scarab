COMMON_LIB_DIR = ../../pin_lib
PIN_EXEC_DIR = ..
SCARAB_DIR = $(abspath ../../..)
PIN_EXEC_TOOL_PATH = $(abspath $(PIN_EXEC_DIR)/obj-intel64/pin_exec.so)
TEST_BINARY = test_binary
SIMPLE_LOOP_BINARY = $(abspath ./simple_loop)
NOP_MODE_RET_BINARY = $(abspath ./nop_mode_ret)
NOP_MODE_NONRET_DIRECT_BINARY = $(abspath ./nop_mode_nonret_direct)
NOP_MODE_NONRET_INDIRECT_BINARY = $(abspath ./nop_mode_nonret_indirect)
NOP_MODE_NOT_TAKEN_BINARY = $(abspath ./nop_mode_not_taken)
NOP_MODE_BAD_STORE_BINARY = $(abspath ./nop_mode_bad_store)
WRONG_PATH_EXCEPTION_BINARY = $(abspath ./wrong_path_exception)
C_PROGRAM_WITH_UD2_BINARY = $(abspath ./c_program_with_ud2)
SIMPLE_C_PROGRAM = $(abspath ./simple_c_program)
JUMP_NEAR_BOUNDARY_BINARY = $(abspath ./jump_near_boundary)

CC_FLAGS := -lgtest -lpthread -std=c++14  -I$(SCARAB_DIR) 
CC_FLAGS += -L $(COMMON_LIB_DIR)/obj -lpin_fe
CC_FLAGS += -DPIN_EXEC_TOOL_PATH=\"$(PIN_EXEC_TOOL_PATH)\"
CC_FLAGS += -DSIMPLE_LOOP_BINARY=\"$(SIMPLE_LOOP_BINARY)\"
CC_FLAGS += -DNOP_MODE_RET_BINARY=\"$(NOP_MODE_RET_BINARY)\"
CC_FLAGS += -DNOP_MODE_NONRET_DIRECT_BINARY=\"$(NOP_MODE_NONRET_DIRECT_BINARY)\"
CC_FLAGS += -DNOP_MODE_NONRET_INDIRECT_BINARY=\"$(NOP_MODE_NONRET_INDIRECT_BINARY)\"
CC_FLAGS += -DNOP_MODE_NOT_TAKEN_BINARY=\"$(NOP_MODE_NOT_TAKEN_BINARY)\"
CC_FLAGS += -DNOP_MODE_BAD_STORE_BINARY=\"$(NOP_MODE_BAD_STORE_BINARY)\"
CC_FLAGS += -DWRONG_PATH_EXCEPTION_BINARY=\"$(WRONG_PATH_EXCEPTION_BINARY)\"
CC_FLAGS += -DC_PROGRAM_WITH_UD2_BINARY=\"$(C_PROGRAM_WITH_UD2_BINARY)\"
CC_FLAGS += -DSIMPLE_C_PROGRAM=\"$(SIMPLE_C_PROGRAM)\"
CC_FLAGS += -DJUMP_NEAR_BOUNDARY_BINARY=\"$(JUMP_NEAR_BOUNDARY_BINARY)\"

ASSEMBLY_SOURCES=$(wildcard *.s)
C_SOURCES=$(wildcard *.c)
CC_SOURCES=$(wildcard *.cc)
CC_HEADERS=$(wildcard *.h)
TEST_BINARIES=$(patsubst %.s,%,$(ASSEMBLY_SOURCES)) $(patsubst %.c,%,$(C_SOURCES))

all: $(TEST_BINARY) $(TEST_BINARIES) pin_exec ## Builds and runs all the unit tests 
	@echo $(TEST_BINARIES)
	./$(TEST_BINARY)

%:%.s
	as -o $@.o $^
	ld -o $@ $@.o -Ttext=400000
	rm $@.o

%:%.c
	gcc -static -O2 -o $@ $^

$(TEST_BINARY): $(CC_SOURCES) $(CC_HEADERS) pin_lib ## Builds the unit test binary
	g++ $(CC_SOURCES) -o $@ $(CC_FLAGS)

pin_lib:
	make -C $(COMMON_LIB_DIR)

pin_exec:
	make -C $(PIN_EXEC_DIR) pin_exec

clean:
	rm -f $(TEST_BINARY) $(TEST_BINARIES)
