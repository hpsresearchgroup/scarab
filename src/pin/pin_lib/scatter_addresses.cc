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

#include "scatter_addresses.h"
#include <iostream>

// Global static instruction map just for scatter instructions
typedef unordered_map<ADDRINT, scatter_info> scatter_info_map;
scatter_info_map                             scatter_info_storage;

void add_to_scatter_info_storage(ADDRINT iaddr) {
  scatter_info_storage[iaddr] = scatter_info();
}

void analyze_scatter_regs(ADDRINT iaddr, REG pin_reg, bool operandRead,
                          bool operandWritten) {
  // we are expecting either the register with the store data, or the
  // mask register. Either way, the register should be read
  ASSERTX(operandRead);

  if(operandWritten) {
    // only register that a scatter should be modifying is the kmask
    // register
    scatter_info_storage[iaddr].set_kmask_reg(pin_reg);
  } else {
    scatter_info_storage[iaddr].set_data_reg_total_width(pin_reg);
  }
}

void analyze_scatter_memory_operand(ADDRINT iaddr, REG pin_base_reg,
                                    REG pin_index_reg, ADDRDELTA displacement,
                                    UINT32 scale) {
  scatter_info_storage[iaddr].set_base_reg(pin_base_reg);
  scatter_info_storage[iaddr].set_index_reg(pin_index_reg);
  scatter_info_storage[iaddr].set_displacement(displacement);
  scatter_info_storage[iaddr].set_scale(scale);
}

void finalize_scatter_info(ADDRINT iaddr, ctype_pin_inst* info) {
  ASSERTX(info->is_simd);
  scatter_info_storage[iaddr].set_data_lane_width_bytes(info->st_size);
  scatter_info_storage[iaddr].set_index_lane_width_bytes(
    info->lane_width_bytes);

  // TODO: remove this debug print later
  std::cout << scatter_info_storage[iaddr] << std::endl;
}

scatter_info::scatter_info() {
  _data_vector_reg_total_width_bytes = 0;
  _data_lane_width_bytes             = 0;
  _kmask_reg                         = REG_INVALID();
  _base_reg                          = REG_INVALID();
  _index_reg                         = REG_INVALID();
  _displacement                      = 0;
  _scale                             = 0;
  _index_lane_width_bytes            = 0;
  _num_stores                        = 0;
}

scatter_info::~scatter_info() {}

ostream& operator<<(ostream& os, const scatter_info& sinfo) {
  os << "_data_vector_reg_total_width_bytes: " << std::dec
     << sinfo._data_vector_reg_total_width_bytes << std::endl;
  os << "_data_lane_width_bytes " << std::dec << sinfo._data_lane_width_bytes
     << std::endl;
  os << "_k_mask_reg: " << REG_StringShort(sinfo._kmask_reg) << std::endl;
  os << "_base_reg: " << REG_StringShort(sinfo._base_reg) << std::endl;
  os << "_index_reg: " << REG_StringShort(sinfo._index_reg) << std::endl;
  os << "_displacement: 0x" << std::hex << sinfo._displacement << std::endl;
  os << "_scale: " << std::dec << sinfo._scale << std::endl;
  os << "_index_lane_width_bytes " << std::dec << sinfo._index_lane_width_bytes
     << std::endl;
  return os;
}

void scatter_info::set_data_reg_total_width(REG pin_reg) {
  // make sure data vector total width not set yet
  ASSERTX(0 == _data_vector_reg_total_width_bytes);

  ASSERTX(REG_is_xmm_ymm_zmm(pin_reg));
  _data_vector_reg_total_width_bytes = REG_Size(pin_reg);

  ASSERTX(0 != _data_vector_reg_total_width_bytes);
}

void scatter_info::set_data_lane_width_bytes(UINT32 st_lane_width) {
  ASSERTX(0 == _data_lane_width_bytes);
  _data_lane_width_bytes = st_lane_width;
  ASSERTX(0 != _data_lane_width_bytes);
}

void scatter_info::set_kmask_reg(REG pin_reg) {
  ASSERTX(!REG_valid(_kmask_reg));  // make sure kmask not set yet

  ASSERTX(REG_is_k_mask(pin_reg));
  _kmask_reg = pin_reg;

  ASSERTX(REG_valid(_kmask_reg));
}

void scatter_info::set_base_reg(REG pin_reg) {
  // make sure base reg not set yet
  ASSERTX(!REG_valid(_base_reg));

  if(REG_valid(pin_reg)) {
    ASSERTX(REG_is_gr64(pin_reg) || REG_is_gr32(pin_reg));
    _base_reg = pin_reg;
    ASSERTX(REG_valid(_base_reg));
  }
}

void scatter_info::set_index_reg(REG pin_reg) {
  // make sure index reg not set yet
  ASSERTX(!REG_valid(_index_reg));

  if(REG_valid(pin_reg)) {
    ASSERTX(REG_is_xmm_ymm_zmm(pin_reg));
    _index_reg = pin_reg;
    ASSERTX(REG_valid(_index_reg));
  }
}

void scatter_info::set_displacement(ADDRDELTA displacement) {
  ASSERTX(0 == _displacement);
  _displacement = displacement;
  // displacement could still be 0, because not every scatter has a displacement
}

void scatter_info::set_scale(UINT32 scale) {
  ASSERTX(0 == _scale);
  _scale = scale;
  ASSERTX(0 != _scale);
}

void scatter_info::set_index_lane_width_bytes(UINT32 idx_lane_width) {
  ASSERTX(0 == _index_lane_width_bytes);

  _index_lane_width_bytes = idx_lane_width;

  // only expecting doubleword or quadword indices
  ASSERTX((4 == _index_lane_width_bytes) || (8 == _index_lane_width_bytes));
}