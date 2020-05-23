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

class gather_scatter_info {
 public:
  enum type { INVALID_TYPE, GATHER, SCATTER, NUM_TYPES };
  std::string type_to_string[NUM_TYPES] = {"INVALID_TYPE", "GATHER", "SCATTER"};

  enum mask_reg_type { INVALID_MASK_REG_TYPE, K, XYMM, NUM_MASK_REG_TYPES };
  std::string mask_reg_type_to_string[NUM_MASK_REG_TYPES] = {
    "INVALID_MASK_REG_TYPE", "K", "XYMM"};

  gather_scatter_info();
  gather_scatter_info(
    const type                               given_type,
    const gather_scatter_info::mask_reg_type given_mask_reg_type);
  ~gather_scatter_info();

  gather_scatter_info::type          get_type() const;
  gather_scatter_info::mask_reg_type get_mask_reg_type() const;
  bool                               data_dest_reg_set() const;

  void   set_data_reg_total_width(const REG pin_reg);
  void   set_data_lane_width_bytes(const UINT32 st_lane_width);
  void   set_mask_reg(const REG pin_reg);
  void   set_base_reg(const REG pin_reg);
  void   set_index_reg(const REG pin_reg);
  void   set_displacement(const ADDRDELTA displacement);
  void   set_scale(const UINT32 scale);
  void   set_index_lane_width_bytes(const UINT32 idx_lane_width);
  void   compute_num_mem_ops();
  UINT32 get_num_mem_ops() const;
  void   verify_fields_for_mem_access_info_generation() const;
  vector<PIN_MEM_ACCESS_INFO> compute_mem_access_infos(
    const CONTEXT* ctxt) const;
  bool base_reg_is_gr32() const;

  friend ostream& operator<<(ostream& os, const gather_scatter_info& sinfo);

 private:
  gather_scatter_info::type          _type;
  gather_scatter_info::mask_reg_type _mask_reg_type;
  UINT32                             _data_vector_reg_total_width_bytes;
  UINT32                             _data_lane_width_bytes;
  REG                                _mask_reg;
  REG                                _base_reg;
  REG                                _index_reg;
  ADDRDELTA                          _displacement;
  UINT32                             _scale;
  UINT32                             _index_lane_width_bytes;
  UINT32                             _num_mem_ops;

  bool      is_non_zero_and_powerof2(const UINT32 v) const;
  UINT32    pin_xyzmm_reg_width_in_bytes(const REG pin_xyzmm_reg) const;
  ADDRDELTA compute_base_reg_addr_contribution(const CONTEXT* ctxt) const;
  ADDRDELTA compute_base_index_addr_contribution(
    const PIN_REGISTER& vector_index_reg_val, const UINT32 lane_id) const;
  PIN_MEMOP_ENUM type_to_PIN_MEMOP_ENUM() const;
  void           verify_mask_reg() const;
  bool           extract_mask_on(const PIN_REGISTER& mask_reg_val_buf,
                                 const UINT32        lane_id) const;
};

void add_to_gather_scatter_info_storage(const ADDRINT iaddr,
                                        const bool    is_gather,
                                        const bool    is_scatter,
                                        const INT32   category);
void set_gather_scatter_reg_operand_info(const ADDRINT iaddr, const REG pin_reg,
                                         const bool operandRead,
                                         const bool operandWritten);
void set_gather_scatter_memory_operand_info(
  const ADDRINT iaddr, const REG pin_base_reg, const REG pin_index_reg,
  const ADDRDELTA displacement, const UINT32 scale, const bool operandReadOnly,
  const bool operandWritenOnly);
void finalize_scatter_info(const ADDRINT iaddr, ctype_pin_inst* info);
vector<PIN_MEM_ACCESS_INFO>
  get_gather_scatter_mem_access_infos_from_gather_scatter_info(
    const CONTEXT* ctxt, const PIN_MULTI_MEM_ACCESS_INFO* infos_from_pin);
void update_gather_scatter_num_ld_or_st(const ADDRINT                   iaddr,
                                        const gather_scatter_info::type type,
                                        const uint      num_maskon_memops,
                                        ctype_pin_inst* info);

#endif  // __SCATTER_H__
