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

#include "gather_scatter_addresses.h"
#include <iostream>

// Global static instruction map just for scatter instructions
typedef unordered_map<ADDRINT, gather_scatter_info> scatter_info_map;
scatter_info_map                                    scatter_info_storage;

void add_to_gather_scatter_info_storage(ADDRINT iaddr, bool is_gather,
                                        bool is_scatter) {
  ASSERTX(!(is_gather && is_scatter));
  ASSERTX(is_gather || is_scatter);
  scatter_info_storage[iaddr] = gather_scatter_info(
    is_gather ? gather_scatter_info::GATHER : gather_scatter_info::SCATTER);
}

void set_gather_scatter_reg_operand_info(ADDRINT iaddr, REG pin_reg,
                                         bool operandRead,
                                         bool operandWritten) {
  gather_scatter_info::type info_type = scatter_info_storage[iaddr].get_type();
  ASSERTX(gather_scatter_info::INVALID != info_type);
  if(REG_is_k_mask(pin_reg)) {
    ASSERTX(operandRead);
    ASSERTX(operandWritten);
    scatter_info_storage[iaddr].set_kmask_reg(pin_reg);
  } else {
    ASSERTX(REG_is_xmm_ymm_zmm(pin_reg));
    switch(info_type) {
      case gather_scatter_info::GATHER:
        ASSERTX(!operandRead && operandWritten);
        break;
      case gather_scatter_info::SCATTER:
        ASSERTX(operandRead && !operandWritten);
        break;
      default:
        ASSERTX(false);
        break;
    }
    scatter_info_storage[iaddr].set_data_reg_total_width(pin_reg);
  }
}

void set_gather_scatter_memory_operand_info(ADDRINT iaddr, REG pin_base_reg,
                                            REG       pin_index_reg,
                                            ADDRDELTA displacement,
                                            UINT32 scale, bool operandReadOnly,
                                            bool operandWritenOnly) {
  switch(scatter_info_storage[iaddr].get_type()) {
    case gather_scatter_info::GATHER:
      ASSERTX(operandReadOnly);
      break;
    case gather_scatter_info::SCATTER:
      ASSERTX(operandWritenOnly);
      break;
    default:
      ASSERTX(false);
      break;
  }
  scatter_info_storage[iaddr].set_base_reg(pin_base_reg);
  scatter_info_storage[iaddr].set_index_reg(pin_index_reg);
  scatter_info_storage[iaddr].set_displacement(displacement);
  scatter_info_storage[iaddr].set_scale(scale);
}

void finalize_scatter_info(ADDRINT iaddr, ctype_pin_inst* info) {
  ASSERTX(info->is_simd);

  switch(scatter_info_storage[iaddr].get_type()) {
    case gather_scatter_info::GATHER:
      scatter_info_storage[iaddr].set_data_lane_width_bytes(info->ld_size);
      break;
    case gather_scatter_info::SCATTER:
      scatter_info_storage[iaddr].set_data_lane_width_bytes(info->st_size);
      break;
    default:
      ASSERTX(false);
      break;
  }

  scatter_info_storage[iaddr].set_index_lane_width_bytes(
    info->lane_width_bytes);

  scatter_info_storage[iaddr].compute_num_mem_ops();

  // TODO: remove this debug print later
  std::cout << StringFromAddrint(iaddr) << std::endl
            << scatter_info_storage[iaddr] << std::endl;
}

bool gather_scatter_info::is_non_zero_and_powerof2(UINT32 v) {
  return v && ((v & (v - 1)) == 0);
}

UINT32 gather_scatter_info::pin_xyzmm_reg_width_in_bytes(REG pin_xyzmm_reg) {
  ASSERTX(REG_is_xmm_ymm_zmm(pin_xyzmm_reg));
  switch(REG_Width(pin_xyzmm_reg)) {
    case REGWIDTH_128:
      return 16;
    case REGWIDTH_256:
      return 32;
    case REGWIDTH_512:
      return 64;
    default:
      ASSERTX(false);
      return 0;
  }
}

gather_scatter_info::type gather_scatter_info::get_type() {
  return _type;
}

gather_scatter_info::gather_scatter_info() {
  _type                              = gather_scatter_info::INVALID;
  _data_vector_reg_total_width_bytes = 0;
  _data_lane_width_bytes             = 0;
  _kmask_reg                         = REG_INVALID();
  _base_reg                          = REG_INVALID();
  _index_reg                         = REG_INVALID();
  _displacement                      = 0;
  _scale                             = 0;
  _index_lane_width_bytes            = 0;
  _num_mem_ops                       = 0;
}

gather_scatter_info::gather_scatter_info(gather_scatter_info::type given_type) :
    gather_scatter_info::gather_scatter_info() {
  _type = given_type;
}

gather_scatter_info::~gather_scatter_info() {}

ostream& operator<<(ostream& os, const gather_scatter_info& sinfo) {
  os << "_type: " << sinfo._type << std::endl;
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
  os << "_num_mem_ops: " << std::dec << sinfo._num_mem_ops << std::endl;
  return os;
}

void gather_scatter_info::set_data_reg_total_width(REG pin_reg) {
  // make sure data vector total width not set yet
  ASSERTX(0 == _data_vector_reg_total_width_bytes);

  ASSERTX(REG_is_xmm_ymm_zmm(pin_reg));
  _data_vector_reg_total_width_bytes = REG_Size(pin_reg);

  ASSERTX(0 != _data_vector_reg_total_width_bytes);
}

void gather_scatter_info::set_data_lane_width_bytes(UINT32 st_lane_width) {
  ASSERTX(0 == _data_lane_width_bytes);
  _data_lane_width_bytes = st_lane_width;
  ASSERTX(0 != _data_lane_width_bytes);
}

void gather_scatter_info::set_kmask_reg(REG pin_reg) {
  ASSERTX(!REG_valid(_kmask_reg));  // make sure kmask not set yet

  ASSERTX(REG_is_k_mask(pin_reg));
  _kmask_reg = pin_reg;

  ASSERTX(REG_valid(_kmask_reg));
}

void gather_scatter_info::set_base_reg(REG pin_reg) {
  // make sure base reg not set yet
  ASSERTX(!REG_valid(_base_reg));

  if(REG_valid(pin_reg)) {
    ASSERTX(REG_is_gr64(pin_reg) || REG_is_gr32(pin_reg));
    _base_reg = pin_reg;
    ASSERTX(REG_valid(_base_reg));
  }
}

void gather_scatter_info::set_index_reg(REG pin_reg) {
  // make sure index reg not set yet
  ASSERTX(!REG_valid(_index_reg));

  if(REG_valid(pin_reg)) {
    ASSERTX(REG_is_xmm_ymm_zmm(pin_reg));
    _index_reg = pin_reg;
    ASSERTX(REG_valid(_index_reg));
  }
}

void gather_scatter_info::set_displacement(ADDRDELTA displacement) {
  ASSERTX(0 == _displacement);
  _displacement = displacement;
  // displacement could still be 0, because not every scatter has a displacement
}

void gather_scatter_info::set_scale(UINT32 scale) {
  ASSERTX(0 == _scale);
  _scale = scale;
  ASSERTX(0 != _scale);
}

void gather_scatter_info::set_index_lane_width_bytes(UINT32 idx_lane_width) {
  ASSERTX(0 == _index_lane_width_bytes);

  _index_lane_width_bytes = idx_lane_width;

  // only expecting doubleword or quadword indices
  ASSERTX((4 == _index_lane_width_bytes) || (8 == _index_lane_width_bytes));
}

void gather_scatter_info::compute_num_mem_ops() {
  ASSERTX(0 == _num_mem_ops);

  ASSERTX(is_non_zero_and_powerof2(_data_vector_reg_total_width_bytes));
  ASSERTX(is_non_zero_and_powerof2(_data_lane_width_bytes));
  UINT32 num_data_lanes = _data_vector_reg_total_width_bytes /
                          _data_lane_width_bytes;
  ASSERTX(is_non_zero_and_powerof2(num_data_lanes));

  ASSERTX(REG_valid(_index_reg) && REG_is_xmm_ymm_zmm(_index_reg));
  UINT32 index_xyzmm_reg_width_bytes = pin_xyzmm_reg_width_in_bytes(_index_reg);
  ASSERTX(is_non_zero_and_powerof2(index_xyzmm_reg_width_bytes));
  ASSERTX(is_non_zero_and_powerof2(_index_lane_width_bytes));
  UINT32 num_index_lanes = index_xyzmm_reg_width_bytes /
                           _index_lane_width_bytes;
  ASSERTX(is_non_zero_and_powerof2(num_index_lanes));

  _num_mem_ops = std::min(num_data_lanes, num_index_lanes);
  ASSERTX(is_non_zero_and_powerof2(_num_mem_ops));
}