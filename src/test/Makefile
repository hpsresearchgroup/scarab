
BASH := /usr/bin/bash
GTEST_FLAGS := -lgtest -std=c++14
COMMON_LIB_DIR = ../pin/pin_lib
MSG_FLAGS :=-L $(COMMON_LIB_DIR)/obj -lpin_fe

NUM_CLIENTS := 1
TEST_SOCKET_FILE := \"./temp_socket.tmp\"

TARGET_PATH=obj

SCARAB_PATH=../
SCARAB_CCFILES=$(SCARAB_PATH)/pin_exec_driven_fe.cc $(SCARAB_PATH)/pin_trace_read.cc
SCARAB_CFILES=$(SCARAB_PATH)/hash_lib.c $(SCARAB_PATH)/malloc_lib.c $(SCARAB_PATH)/utils.c $(SCARAB_PATH)/debug_print.c $(SCARAB_PATH)/enum.c $(SCARAB_PATH)/isa.c
SCARAB_OBJS= $(patsubst $(SCARAB_PATH)/%.cc,$(TARGET_PATH)/%.o,$(SCARAB_CCFILES)) $(patsubst $(SCARAB_PATH)/%.c,$(TARGET_PATH)/%.o,$(SCARAB_CFILES))


.PHONY: gtest message_test server_client_test run_server_client_test scarab_dummy_client_test pin_lib clean objdir

objdir:
	mkdir -p obj

%.out:%.s
	as -o $@.o $^
	ld -o $@ $@.o
	rm $@.o

pin_lib: $(wildcard $(COMMON_LIB_DIR)/*.cc)
	make -C $(COMMON_LIB_DIR) NO_STAT=-DNO_STAT PIN_COMPILE=-DGTEST_COMPILE

gtest:
	make message_test
	make run_server_client_test

$(TARGET_PATH)/%.o:%.cc
	g++ -std=c++14 -c $^ -o $@ -DNO_STAT -DGTEST_COMPILE -DNUM_CLIENTS=$(NUM_CLIENTS)

$(TARGET_PATH)/%.o:%.c
	gcc -c $^ -o $@ -DNO_STAT -DGTEST_COMPILE -DNUM_CLIENTS=$(NUM_CLIENTS)

$(TARGET_PATH)/%.o:$(SCARAB_PATH)/%.cc
	g++ -std=c++14 -c $^ -o $@ -DNO_STAT -DGTEST_COMPILE

$(TARGET_PATH)/%.o:$(SCARAB_PATH)/%.c
	gcc -c $^ -o $@ -DNO_STAT -DGTEST_COMPILE

#scarab_dummy_client_test: $(TARGET_PATH)/test_main.o $(TARGET_PATH)/scarab_dummy_client_test.o $(TARGET_PATH)/dummy_globals.o $(SCARAB_OBJS)
scarab_dummy_client_test: test_main.cc scarab_dummy_client_test.cc dummy_globals.c $(SCARAB_OBJS)
	make pin_lib
	g++ $(GTEST_FLAGS) -lpthread $^ -o obj/scarab_dummy_client_test $(MSG_FLAGS) -DNO_STAT -DGTEST_COMPILE -DNUM_CLIENTS=$(NUM_CLIENTS) 

run_scarab_dummy_client_test: scarab_dummy_client_test
	./obj/scarab_dummy_client_test

message_test: test_main.cc message_queue_interface_lib_test.cc
	make pin_lib
	g++ $(GTEST_FLAGS) $^ -o message_test $(MSG_FLAGS)
	./message_test

server_client_test: test_main.cc server_client_socket_test.cc
	make pin_lib
	g++ $(GTEST_FLAGS) $^ -o server_test -DSERVER_TEST -DTEST_SOCKET_FILE=$(TEST_SOCKET_FILE) -DNUM_CLIENTS=$(NUM_CLIENTS) $(MSG_FLAGS)
	g++ $(GTEST_FLAGS) $^ -o client_test $(MSG_FLAGS) -DTEST_SOCKET_FILE=$(TEST_SOCKET_FILE)

run_server_client_test: server_client_test
	./server_test& $(BASH) -c 'for i in `seq 1 $(NUM_CLIENTS)`; do ./client_test& done'

clean:
	-rm message_test
	-rm server_test
	-rm client_test
	make -C $(COMMON_LIB_DIR) clean
	-rm *.out
	-rm -rf obj
