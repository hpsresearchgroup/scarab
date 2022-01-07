/* Copyright 2020 University of California Santa Cruz
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

/***************************************************************************************
 * File         : frontend/memtrace_trace_reader.h
 * Author       : Heiner Litz
 * Date         : 05/15/2020
 * Description  :
 ***************************************************************************************/
#ifndef MEMTRACE_TRACE_READER_H
#define MEMTRACE_TRACE_READER_H

#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#define DR_DO_NOT_DEFINE_int64
#include "./pin/pin_lib/x86_decoder.h"

extern "C" {
#include "xed-interface.h"
}

#if __cplusplus < 201402L
template <typename T, typename... Args>
static std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif

// Indices to 'xed_map_' cached features
static constexpr int MAP_MEMOPS  = 0;
static constexpr int MAP_UNKNOWN = 1;
static constexpr int MAP_COND    = 2;
static constexpr int MAP_REP     = 3;
static constexpr int MAP_XED     = 4;

class TraceReader {
 public:
  enum returnValue : uint8_t {
    ENTRY_VALID,
    ENTRY_NOT_FOUND,
    ENTRY_FIRST,
    ENTRY_OUT_OF_SEGMENT,
  };
  using bufferEntry = std::deque<InstInfo>::iterator;

  // The default-constructed object will not return valid instructions
  TraceReader();
  // A trace and single-binary object
  TraceReader(const std::string& _trace, const std::string& _binary,
              uint64_t _offset, uint32_t _buf_size = 0);
  // A trace and multi-binary object which reads 'binary-info.txt' from the
  // input path. This file contains one '<binary> <offset>' pair per line.
  TraceReader(const std::string& _trace, const std::string& _binary_group_path,
              uint32_t _buf_size = 0);
  ~TraceReader();
  // A constructor that fails will cause operator! to return true
  bool              operator!();
  const InstInfo*   nextInstruction();
  const returnValue findPCInSegment(bufferEntry& ref, uint64_t _pc,
                                    uint64_t _termination_pc);
  const returnValue findPC(bufferEntry& ref, uint64_t _pc);
  const returnValue peekInstructionAtIndex(uint32_t idx, bufferEntry& ref);
  bufferEntry       bufferStart();

 private:
  virtual const InstInfo* getNextInstruction()                        = 0;
  virtual void            binaryGroupPathIs(const std::string& _path) = 0;
  virtual bool            initTrace()                                 = 0;
  virtual bool            locationForVAddr(uint64_t _vaddr, uint8_t** _loc,
                                           uint64_t* _size)           = 0;

  void init_buffer();
  void binaryFileIs(const std::string& _binary, uint64_t _offset);

  std::unique_ptr<xed_decoded_inst_t> makeNop(uint8_t _length);

 protected:
  std::string                                                    trace_;
  InstInfo                                                       info_;
  InstInfo                                                       invalid_info_;
  bool                                                           trace_ready_;
  bool                                                           binary_ready_;
  xed_state_t                                                    xed_state_;
  std::unordered_map<std::string, std::pair<uint8_t*, uint64_t>> binaries_;
  std::vector<std::tuple<uint64_t, uint64_t, uint8_t*>>          sections_;
  std::unordered_map<uint64_t, std::tuple<int, bool, bool, bool,
                                          std::unique_ptr<xed_decoded_inst_t>>>
                       xed_map_;
  int                  warn_not_found_;
  uint64_t             skipped_;
  uint32_t             buf_size_;
  std::deque<InstInfo> ins_buffer;

  void init(const std::string& _trace);
  bool initBinary(const std::string& _name, uint64_t _offset);
  void clearBinaries();
  void fillCache(uint64_t _vAddr, uint8_t _reported_size,
                 uint8_t* inst_bytes = NULL);
  void traceFileIs(const std::string& _trace);
};

#endif
