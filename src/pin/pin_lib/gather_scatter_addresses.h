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

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab
//#include "pin.H"
#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab
#include "../../ctype_pin_inst.h"
#include "pin_api_to_xed.h"

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

  void           set_data_reg_total_width(const xed_reg_enum_t pin_reg);
  void           set_data_lane_width_bytes(const uint32_t st_lane_width);
  uint32_t       get_data_lane_width_bytes() const;
  void           set_mask_reg(const xed_reg_enum_t pin_reg);
  void           set_base_reg(const xed_reg_enum_t pin_reg);
  void           set_index_reg(const xed_reg_enum_t pin_reg);
  xed_reg_enum_t get_mask_reg() const;
  xed_reg_enum_t get_base_reg() const;
  xed_reg_enum_t get_index_reg() const;
  void           set_displacement(const uint64_t displacement);
  uint64_t       get_displacement() const;
  void           set_scale(const uint32_t scale);
  uint32_t       get_scale() const;
  void           set_index_lane_width_bytes(const uint32_t idx_lane_width);
  uint32_t       get_index_lane_width_bytes() const;
  void           compute_num_mem_ops();
  uint32_t       get_num_mem_ops() const;
  void           verify_fields_for_mem_access_info_generation() const;
  bool           base_reg_is_gr32() const;

  friend std::ostream& operator<<(std::ostream&              os,
                                  const gather_scatter_info& sinfo);

 private:
  gather_scatter_info::type          _type;
  gather_scatter_info::mask_reg_type _mask_reg_type;
  uint32_t                           _data_vector_reg_total_width_bytes;
  uint32_t                           _data_lane_width_bytes;
  xed_reg_enum_t                     _mask_reg;
  xed_reg_enum_t                     _base_reg;
  xed_reg_enum_t                     _index_reg;
  uint64_t                           _displacement;
  uint32_t                           _scale;
  uint32_t                           _index_lane_width_bytes;
  uint32_t                           _num_mem_ops;

  bool     is_non_zero_and_powerof2(const uint32_t v) const;
  uint32_t pin_xyzmm_reg_width_in_bytes(
    const xed_reg_enum_t pin_xyzmm_reg) const;
  void verify_mask_reg() const;
};

typedef std::unordered_map<ADDRINT, gather_scatter_info> scatter_info_map;
gather_scatter_info add_to_gather_scatter_info_storage(
  const ADDRINT iaddr, const bool is_gather, const bool is_scatter,
  const xed_category_enum_t category);
void set_gather_scatter_reg_operand_info(const ADDRINT        iaddr,
                                         const xed_reg_enum_t pin_reg,
                                         const bool           operandRead,
                                         const bool           operandWritten);
void set_gather_scatter_memory_operand_info(const ADDRINT        iaddr,
                                            const xed_reg_enum_t pin_base_reg,
                                            const xed_reg_enum_t pin_index_reg,
                                            const uint32_t       scale,
                                            const bool operandReadOnly,
                                            const bool operandWritenOnly);
void finalize_scatter_info(const ADDRINT iaddr, ctype_pin_inst* info);
void init_reg_xed_to_pin_map();

#endif  // __SCATTER_H__
