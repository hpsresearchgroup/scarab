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
 * File         : frontend/memtrace_trace_reader_memtrace.h
 * Author       : Heiner Litz
 * Date         : 05/15/2020
 * Description  :
 ***************************************************************************************/
#ifndef MEMTRACE_READER_MEMTRACE_H
#define MEMTRACE_READER_MEMTRACE_H

#include "frontend/memtrace/memtrace_trace_reader.h"

//#include "instrument.h"
#include "analyzer.h"
#include "raw2trace.h"
#include "raw2trace_directory.h"

class TraceReaderMemtrace : public TraceReader {
 public:
  const InstInfo* getNextInstruction() override;
  TraceReaderMemtrace(const std::string& _trace, const std::string& _binary,
                      uint64_t _offset, uint32_t _bufsize);
  TraceReaderMemtrace(const std::string& _trace,
                      const std::string& _binary_group_path, uint32_t _bufsize);
  ~TraceReaderMemtrace();

 private:
  void               binaryGroupPathIs(const std::string& _path) override;
  bool               initTrace() override;
  bool               locationForVAddr(uint64_t _vaddr, uint8_t** _loc,
                                      uint64_t* _size) override;
  void               init(const std::string& _trace);
  static const char* parse_buildid_string(const char* src, OUT void** data);
  bool               getNextInstruction__(InstInfo* _info, InstInfo* _prior);
  void               processInst(InstInfo* _info);
  bool               typeIsMem(trace_type_t _type);

  std::unique_ptr<module_mapper_t> module_mapper_;
  raw2trace_directory_t            directory_;
  void*                            dcontext_;
  unsigned int                     knob_verbose_;

  enum class MTState {
    INST,
    MEM1,
    MEM2,
  };

  std::unique_ptr<analyzer_t> mt_reader_;
  reader_t*                   mt_iter_;
  reader_t*                   mt_end_;
  MTState                     mt_state_;
  memref_t                    mt_ref_;
  int                         mt_mem_ops_;
  uint64_t                    mt_seq_;
  uint32_t                    mt_prior_isize_;
  InstInfo                    mt_info_a_;
  InstInfo                    mt_info_b_;
  bool                        mt_using_info_a_;
  uint64_t                    mt_warn_target_;
};

#endif
