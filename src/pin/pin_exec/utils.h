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
#include <utility>

#include "../pin_lib/gather_scatter_addresses.h"

#undef UNUSED
#undef WARNING

#include "pin.H"

#undef UNUSED
#undef WARNING

#define ADDR_MASK(x) ((x)&0x0000FFFFFFFFFFFFUL)

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

struct Mem_Writes_Info {
  enum class Type { NO_WRITE, ONE_WRITE, MULTI_WRITE, MULTI_WRITE_SCATTER };

  Mem_Writes_Info() : type(Type::NO_WRITE) {}

  Mem_Writes_Info(ADDRINT addr, uint32_t size) :
      type(Type::ONE_WRITE), single_write_addr(addr), single_write_size(size) {}

  Mem_Writes_Info(const PIN_MULTI_MEM_ACCESS_INFO* const multi_mem_access_info,
                  CONTEXT* ctxt, bool is_scatter) :
      type(is_scatter ? Type::MULTI_WRITE_SCATTER : Type::MULTI_WRITE),
      multi_mem_access_info(multi_mem_access_info),
      scatter_maskon_mem_access_info(
        produce_scatter_access_info(multi_mem_access_info, ctxt, is_scatter)) {}

  uint32_t get_num_mem_writes() {
    switch(type) {
      case Type::NO_WRITE:
        return 0;
      case Type::ONE_WRITE:
        return 1;
      case Type::MULTI_WRITE:
        return multi_mem_access_info->numberOfMemops;
      case Type::MULTI_WRITE_SCATTER:
        return scatter_maskon_mem_access_info.size();
    }
    ASSERTM(0, false, "Bad Mem Write Info");
    return 0;
  }

  std::pair<ADDRINT, uint32_t> get_write_addr_size(int i) {
    switch(type) {
      case Type::NO_WRITE:
        ASSERTM(0, false, "Attempted to save a write when it doesn't exist");
        return {};
      case Type::ONE_WRITE:
        ASSERTM(0, i == 0,
                "Write info %d out of range (ONE_WRITE). Must be < 1", i);
        return {single_write_addr, single_write_size};

      case Type::MULTI_WRITE:
      case Type::MULTI_WRITE_SCATTER:
        int max = (type == Type::MULTI_WRITE) ?
                    multi_mem_access_info->numberOfMemops :
                    scatter_maskon_mem_access_info.size();
        ASSERTM(0, i < max,
                "Write info %d out of range (MULTI_WRITE). Must be < %d. Is "
                "Scatter: %d",
                i, multi_mem_access_info->numberOfMemops,
                type == Type::MULTI_WRITE_SCATTER);

        const auto& info = (type == Type::MULTI_WRITE) ?
                             multi_mem_access_info->memop[i] :
                             scatter_maskon_mem_access_info[i];
        return {info.memoryAddress, info.bytesAccessed};
    }
    ASSERTM(0, false, "Bad Mem Write Info");
    return {};
  }

  const Type                             type;
  const ADDRINT                          single_write_addr     = 0;
  const uint32_t                         single_write_size     = 0;
  const PIN_MULTI_MEM_ACCESS_INFO* const multi_mem_access_info = nullptr;
  const std::vector<PIN_MEM_ACCESS_INFO> scatter_maskon_mem_access_info;

 private:
  static std::vector<PIN_MEM_ACCESS_INFO> produce_scatter_access_info(
    const PIN_MULTI_MEM_ACCESS_INFO* multi_mem_info, CONTEXT* ctxt,
    bool is_scatter) {
    std::vector<PIN_MEM_ACCESS_INFO> scatter_mem_infos;
    if(is_scatter) {
      // don't care about stores in lanes that are disabled by k mask
      for(auto mem_access :
          get_gather_scatter_mem_access_infos_from_gather_scatter_info(
            ctxt, multi_mem_info)) {
        if(mem_access.maskOn) {
          scatter_mem_infos.push_back(mem_access);
        }
      }
    }
    return scatter_mem_infos;
  }
};

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
  bool      is_syscall;
  ADDRINT   wpnm_eip;

  ProcState() : mem_state_list(NULL), num_mem_state(0) {}

  void update(CONTEXT* _ctxt, UINT64 _uid, bool _u_i, bool _wrongpath,
              bool _wrongpath_nop_mode, ADDRINT _wpnm_eip,
              Mem_Writes_Info _mem_write_info, bool _is_syscall) {
    uid                      = _uid;
    unretireable_instruction = _u_i;
    wrongpath                = _wrongpath;
    wrongpath_nop_mode       = _wrongpath_nop_mode;
    wpnm_eip                 = _wpnm_eip;
    is_syscall               = _is_syscall;

    PIN_SaveContext(_ctxt, &ctxt);

    uint32_t _num_mem_state = _mem_write_info.get_num_mem_writes();

    if(_num_mem_state > num_mem_state) {
      if(NULL != mem_state_list) {
        free(mem_state_list);
      }

      mem_state_list = (MemState*)malloc(_num_mem_state * sizeof(MemState));
    }

    num_mem_state = _num_mem_state;

    auto save_mem = [](MemState* mem_state, ADDRINT write_addr,
                       uint32_t write_size) {
      ADDRINT masked_write_addr = ADDR_MASK(write_addr);
      mem_state->init(masked_write_addr, write_size);
      PIN_SafeCopy(mem_state->mem_data_ptr, (void*)masked_write_addr,
                   write_size);
    };

    for(uint32_t i = 0; i < _num_mem_state; ++i) {
      auto addr_size_pair = _mem_write_info.get_write_addr_size(i);
      save_mem(&mem_state_list[i], addr_size_pair.first, addr_size_pair.second);
    }
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

class Pintool_State {
 public:
  Pintool_State() { clear_changing_control_flow(); }

  // ***********************  Getters  **********************
  bool skip_further_processing() { return should_change_control_flow(); }

  bool should_change_control_flow() { return should_change_control_flow_; }

  bool should_skip_next_instruction() { return should_skip_next_instruction_; }

  uint64_t get_next_inst_uid() { return uid_ctr++; }

  uint64_t get_curr_inst_uid() { return uid_ctr; }

  CONTEXT* get_context_for_changing_control_flow() {
    return &next_pintool_state_;
  }

  bool is_on_wrongpath() { return on_wrongpath_; }

  // ***********************  Setters  **********************
  void clear_changing_control_flow() {
    should_change_control_flow_   = false;
    should_skip_next_instruction_ = false;
  }

  void set_next_state_for_changing_control_flow(const CONTEXT* next_state,
                                                bool           redirect_rip,
                                                uint64_t       next_rip,
                                                bool skip_next_instruction) {
    should_change_control_flow_ = true;
    PIN_SaveContext(next_state, &next_pintool_state_);
    if(redirect_rip) {
      PIN_SetContextReg(&next_pintool_state_, REG_INST_PTR, next_rip);
    }
    should_skip_next_instruction_ = skip_next_instruction;
  }

  void set_wrongpath(bool on_wrongpath) { on_wrongpath_ = on_wrongpath; }

 private:
  bool    should_change_control_flow_;
  bool    should_skip_next_instruction_;
  CONTEXT next_pintool_state_;

  uint64_t uid_ctr       = 0;
  bool     on_wrongpath_ = false;
};


#endif  // PIN_EXEC_UTILS_H__
