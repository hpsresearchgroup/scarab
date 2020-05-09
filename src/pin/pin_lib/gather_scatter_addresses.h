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
#include <vector>
#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab
#include "pin.H"
#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab
#include "../../ctype_pin_inst.h"

void add_to_gather_scatter_info_storage(ADDRINT iaddr, bool is_gather,
                                        bool is_scatter);
void set_gather_scatter_reg_operand_info(ADDRINT iaddr, REG pin_reg,
                                         bool operandRead, bool operandWritten);
void set_gather_scatter_memory_operand_info(ADDRINT iaddr, REG pin_base_reg,
                                            REG       pin_index_reg,
                                            ADDRDELTA displacement,
                                            UINT32 scale, bool operandReadOnly,
                                            bool operandWritenOnly);
void finalize_scatter_info(ADDRINT iaddr, ctype_pin_inst* info);
vector<PIN_MEM_ACCESS_INFO>
  get_gather_scatter_mem_access_infos_from_gather_scatter_info(
    const CONTEXT* ctxt, const PIN_MULTI_MEM_ACCESS_INFO* infos_from_pin);

class gather_scatter_info {
 public:
  enum type { INVALID, GATHER, SCATTER, NUM_TYPES };
  std::string type_to_string[NUM_TYPES] = {"INVALID", "GATHER", "SCATTER"};

  gather_scatter_info();
  gather_scatter_info(type given_type);
  ~gather_scatter_info();

  gather_scatter_info::type get_type() const;

  void set_data_reg_total_width(REG pin_reg);
  void set_data_lane_width_bytes(UINT32 st_lane_width);
  void set_kmask_reg(REG pin_reg);
  void set_base_reg(REG pin_reg);
  void set_index_reg(REG pin_reg);
  void set_displacement(ADDRDELTA displacement);
  void set_scale(UINT32 scale);
  void set_index_lane_width_bytes(UINT32 idx_lane_width);
  void compute_num_mem_ops();
  void verify_fields_for_mem_access_info_generation() const;
  vector<PIN_MEM_ACCESS_INFO> compute_mem_access_infos(
    const CONTEXT* ctxt) const;
  bool base_reg_is_gr32() const;

  friend ostream& operator<<(ostream& os, const gather_scatter_info& sinfo);

 private:
  gather_scatter_info::type _type;
  UINT32                    _data_vector_reg_total_width_bytes;
  UINT32                    _data_lane_width_bytes;
  REG                       _kmask_reg;
  REG                       _base_reg;
  REG                       _index_reg;
  ADDRDELTA                 _displacement;
  UINT32                    _scale;
  UINT32                    _index_lane_width_bytes;
  UINT32                    _num_mem_ops;

  bool      is_non_zero_and_powerof2(UINT32 v) const;
  UINT32    pin_xyzmm_reg_width_in_bytes(REG pin_xyzmm_reg) const;
  ADDRDELTA compute_base_reg_addr_contribution(const CONTEXT* ctxt) const;
  ADDRDELTA compute_base_index_addr_contribution(
    const PIN_REGISTER& vector_index_reg_val, UINT32 lane_id) const;
  PIN_MEMOP_ENUM type_to_PIN_MEMOP_ENUM() const;
};

#endif  // __SCATTER_H__
