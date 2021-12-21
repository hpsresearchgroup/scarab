/* Copyright 2020 HPS/SAFARI Research Groups
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PIN_EXEC_UTILS_H__
#define PIN_EXEC_UTILS_H__

#include <cinttypes>
#include <stdio.h>
#include <unordered_map>

#undef UNUSED
#undef WARNING

#include "pin.H"

#undef UNUSED
#undef WARNING

#define ADDR_MASK(x) ((x)&0x0000FFFFFFFFFFFFULL)

#ifdef DEBUG_PRINT
#define DBG_PRINT(uid, start_print_uid, end_print_uid, ...)  \
  do {                                                       \
    if((uid >= start_print_uid) && (uid <= end_print_uid)) { \
      printf("PIN DEBUG: " __VA_ARGS__);                     \
    }                                                        \
  } while(0)
#else
#define DBG_PRINT(...) \
  do {                 \
  } while(0)
#endif

#define ENABLE_ASSERTIONS true

#define ASSERTM(proc_id, cond, args...)                                     \
  do {                                                                      \
    if(ENABLE_ASSERTIONS && !(cond)) {                                      \
      fflush(stdout);                                                       \
      fprintf(stderr, "\n");                                                \
      fprintf(stderr, "%s:%d: ASSERT FAILED (P=%u):  ", __FILE__, __LINE__, \
              proc_id);                                                     \
      fprintf(stderr, "%s\n", #cond);                                       \
      fprintf(stderr, "%s:%d: ASSERT FAILED (P=%u):  ", __FILE__, __LINE__, \
              proc_id);                                                     \
      fprintf(stderr, ##args);                                              \
      exit(15);                                                             \
    }                                                                       \
  } while(0)

struct MemState {
  ADDRINT mem_addr;
  UINT32  mem_size;
  VOID*   mem_data_ptr;

  MemState() : mem_size(0), mem_data_ptr(NULL) {}

  void resize(UINT32 new_mem_size) {
    // we need to free the old mem_data_ptr if we need to reallocate a longer
    // one
    if(new_mem_size > mem_size) {
      if(mem_data_ptr) {
        free(mem_data_ptr);
      }

      mem_data_ptr = malloc(new_mem_size);
    }
  }

  void init(ADDRINT _mem_addr, UINT32 _mem_size) {
    resize(_mem_size);

    mem_addr = _mem_addr;
    mem_size = _mem_size;
  }

  ~MemState() {
    if(mem_data_ptr) {
      free(mem_data_ptr);
    }
  }
};

struct ProcState {
  UINT64    uid;
  MemState* mem_state_list = NULL;
  UINT      num_mem_state;
  CONTEXT   ctxt;
  bool      unretireable_instruction;
  bool      wrongpath;
  bool      wrongpath_nop_mode;
  ADDRINT   wpnm_eip;

  ProcState() : mem_state_list(NULL), num_mem_state(0) {}

  void init(UINT64 _uid, bool _u_i, bool _wrongpath, bool _wrongpath_nop_mode,
            ADDRINT _wpnm_eip, UINT _num_mem_state) {
    uid                      = _uid;
    unretireable_instruction = _u_i;
    wrongpath                = _wrongpath;
    wrongpath_nop_mode       = _wrongpath_nop_mode;
    wpnm_eip                 = _wpnm_eip;

    if(_num_mem_state > num_mem_state) {
      if(NULL != mem_state_list) {
        free(mem_state_list);
      }

      mem_state_list = (MemState*)malloc(_num_mem_state * sizeof(MemState));
    }

    num_mem_state = _num_mem_state;
  }

  ~ProcState() {
    if(NULL != mem_state_list) {
      free(mem_state_list);
    }
  }
};

template <typename T, int INIT_CAPACITY>
class CirBuf {
  T*    cir_buf;
  INT64 cir_buf_head_index = 0;
  INT64 cir_buf_tail_index = -1;
  INT64 cir_buf_size       = 0;
  INT64 cir_buf_capacity   = 0;

  void check_cir_buf_invariant() {
    ASSERTM(0, (cir_buf_tail_index - cir_buf_head_index + 1) == cir_buf_size,
            "cir_buf head(%lld), tail(%lld), size(%lld), and capacity(%lld) "
            "inconsistent\n",
            (long long int)cir_buf_head_index,
            (long long int)cir_buf_tail_index, (long long int)cir_buf_size,
            (long long int)cir_buf_capacity);
    ASSERTM(0, cir_buf_size >= 0, "cir_buf size is negative (%lld)\n",
            (long long int)cir_buf_size);
    ASSERTM(0, cir_buf_size <= cir_buf_capacity,
            "cir_buf size(%lld) exceeds capacity(%lld)\n",
            (long long int)cir_buf_size, (long long int)cir_buf_capacity);
  }

  void double_capacity() {
    int doubled_cir_buf_capacity = cir_buf_capacity * 2;
    T*  doubled_cir_buf = (T*)calloc(doubled_cir_buf_capacity, sizeof(T));
    ASSERTM(0, NULL != doubled_cir_buf,
            "calloc for cir_buf failed while resizing\n");

    // copy all the old entries over
    for(int i = 0; i < cir_buf_size; i++) {
      // the new list gets shifted so that the head starts at index 0
      doubled_cir_buf[i] = (*this)[cir_buf_head_index + i];
    }

    free(cir_buf);

    cir_buf            = doubled_cir_buf;
    cir_buf_capacity   = doubled_cir_buf_capacity;
    cir_buf_head_index = 0;
    cir_buf_tail_index = cir_buf_size - 1;
  }

  void check_capacity() {
    // if cir_buf isn't big enough, allocate a new one that's
    // twice as big
    if(cir_buf_size >= cir_buf_capacity) {
      double_capacity();
    }
  }

 public:
  CirBuf() {
    cir_buf_capacity = INIT_CAPACITY;
    cir_buf          = (T*)calloc(cir_buf_capacity, sizeof(T));
    ASSERTM(0, NULL != cir_buf, "initial calloc for cir_buf failed\n");
  }

  INT64 get_head_index() const { return cir_buf_head_index; }

  INT64 get_tail_index() const { return cir_buf_tail_index; }

  INT64 get_size() const { return cir_buf_size; }

  bool empty() const { return (0 == cir_buf_size); }

  T& operator[](INT64 index) {
    ASSERTM(0, index >= get_head_index(),
            "accessing invalid index %lld when head is %lld\n",
            (long long int)index, (long long int)get_head_index());
    ASSERTM(0, index <= get_tail_index(),
            "accessing invalid index %lld when tail is %lld\n",
            (long long int)index, (long long int)get_tail_index());
    return cir_buf[index % cir_buf_capacity];
  }

  T& get_tail() { return (*this)[get_tail_index()]; }

  INT64 remove_from_cir_buf_head() {
    cir_buf_head_index++;
    cir_buf_size--;

    check_cir_buf_invariant();

    return cir_buf_head_index;
  }


  INT64 remove_from_cir_buf_tail() {
    cir_buf_tail_index--;
    cir_buf_size--;

    check_cir_buf_invariant();

    return cir_buf_tail_index;
  }

  void append_to_cir_buf() {
    check_capacity();

    cir_buf_tail_index++;
    cir_buf_size++;

    check_cir_buf_invariant();
  }
};

class Address_Tracker {
 public:
  void insert(ADDRINT new_address) {
    tracked_addresses.insert({new_address, true});
  }

  bool contains(ADDRINT address) {
    return tracked_addresses.count(address) > 0;
  }

 private:
  // Using std::unordered_map instead of std::unordered_set because PinCRT is
  // incomplete.
  std::unordered_map<ADDRINT, bool> tracked_addresses;
};

enum Magic_Inst_Type { NOT_MAGIC = 0, SCARAB_START = 1, SCARAB_END = 2 };


#endif  // PIN_EXEC_UTILS_H__
