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

#ifndef __SCATTER_H__
#define __SCATTER_H__

#include <unordered_map>
#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab
#include "pin.H"
#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab
#include "../../ctype_pin_inst.h"

void add_to_scatter_info_storage(ADDRINT iaddr);
void analyze_scatter_regs(ADDRINT iaddr, REG pin_reg, bool operandRead,
                          bool operandWritten);
void analyze_scatter_memory_operand(ADDRINT iaddr, REG pin_base_reg,
                                    REG pin_index_reg, ADDRDELTA displacement,
                                    UINT32 scale);
void finalize_scatter_info(ADDRINT iaddr, ctype_pin_inst* info);

class scatter_info {
 private:
  UINT32    _data_vector_reg_total_width_bytes;
  UINT32    _data_lane_width_bytes;
  REG       _kmask_reg;
  REG       _base_reg;
  REG       _index_reg;
  ADDRDELTA _displacement;
  UINT32    _scale;
  UINT32    _index_lane_width_bytes;
  UINT32    _num_stores;

 public:
  scatter_info();
  ~scatter_info();
  friend ostream& operator<<(ostream& os, const scatter_info& sinfo);

  void set_data_reg_total_width(REG pin_reg);
  void set_data_lane_width_bytes(UINT32 st_lane_width);
  void set_kmask_reg(REG pin_reg);
  void set_base_reg(REG pin_reg);
  void set_index_reg(REG pin_reg);
  void set_displacement(ADDRDELTA displacement);
  void set_scale(UINT32 scale);
  void set_index_lane_width_bytes(UINT32 idx_lane_width);
};

#endif  // __SCATTER_H__
