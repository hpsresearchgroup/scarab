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
  scatter_info_storage[iaddr].verify_fields_for_mem_access_info_generation();

  // std::cout << StringFromAddrint(iaddr) << std::endl
  //           << scatter_info_storage[iaddr] << std::endl;
}

static void verify_mem_access_infos(
  const vector<PIN_MEM_ACCESS_INFO> computed_infos,
  const PIN_MULTI_MEM_ACCESS_INFO* infos_from_pin, bool base_reg_is_gr32) {
  for(UINT32 lane_id = 0; lane_id < infos_from_pin->numberOfMemops; lane_id++) {
    ADDRINT        addr_from_pin = infos_from_pin->memop[lane_id].memoryAddress;
    PIN_MEMOP_ENUM type_from_pin = infos_from_pin->memop[lane_id].memopType;
    UINT32         size_from_pin = infos_from_pin->memop[lane_id].bytesAccessed;
    bool           mask_on_from_pin = infos_from_pin->memop[lane_id].maskOn;

    // as late as PIN 3.13, there is a bug where PIN will not correctly compute
    // the addresses of gathers/scatters if the base register is a 32-bit
    // register and holds a negative value
    if(!base_reg_is_gr32) {
      // std::cout << "Comparing addresses" << std::endl;
      // std::cout << StringFromAddrint(computed_infos[lane_id].memoryAddress)
      //           << endl;
      // std::cout << StringFromAddrint(addr_from_pin) << endl;
      ASSERTX(computed_infos[lane_id].memoryAddress == addr_from_pin);
    }
    ASSERTX(computed_infos[lane_id].memopType == type_from_pin);
    ASSERTX(computed_infos[lane_id].bytesAccessed == size_from_pin);
    ASSERTX(computed_infos[lane_id].maskOn == mask_on_from_pin);
  }
}

vector<PIN_MEM_ACCESS_INFO>
  get_gather_scatter_mem_access_infos_from_gather_scatter_info(
    const CONTEXT* ctxt, const PIN_MULTI_MEM_ACCESS_INFO* infos_from_pin) {
  ADDRINT iaddr;
  PIN_GetContextRegval(ctxt, REG_INST_PTR, (UINT8*)&iaddr);
  ASSERTX(1 == scatter_info_storage.count(iaddr));
  std::cout << "Getting mem access infos for inst at "
            << StringFromAddrint(iaddr) << std::endl;
  vector<PIN_MEM_ACCESS_INFO> computed_infos =
    scatter_info_storage[iaddr].compute_mem_access_infos(ctxt);
  verify_mem_access_infos(computed_infos, infos_from_pin,
                          scatter_info_storage[iaddr].base_reg_is_gr32());

  return computed_infos;
}

bool gather_scatter_info::is_non_zero_and_powerof2(UINT32 v) const {
  return v && ((v & (v - 1)) == 0);
}

UINT32 gather_scatter_info::pin_xyzmm_reg_width_in_bytes(
  REG pin_xyzmm_reg) const {
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

PIN_MEMOP_ENUM gather_scatter_info::type_to_PIN_MEMOP_ENUM() const {
  switch(_type) {
    case GATHER:
      return PIN_MEMOP_LOAD;
    case SCATTER:
      return PIN_MEMOP_STORE;
    default:
      ASSERTX(false);
      return PIN_MEMOP_LOAD;
  }
}

ADDRDELTA gather_scatter_info::compute_base_reg_addr_contribution(
  const CONTEXT* ctxt) const {
  ADDRDELTA base_addr_contribution = 0;
  if(REG_valid(_base_reg)) {
    PIN_REGISTER buf;
    PIN_GetContextRegval(ctxt, _base_reg, (UINT8*)&buf);
    // need to do this to make sure a 32-bit base register holding a negative
    // value is properly sign-extended into a 64-bit ADDRDELTA value
    if(REG_is_gr32(_base_reg)) {
      base_addr_contribution = buf.s_dword[0];
    } else if(REG_is_gr64(_base_reg)) {
      base_addr_contribution = buf.s_qword[0];
    } else {
      ASSERTX(false);
    }
  }
  return base_addr_contribution;
}

ADDRDELTA gather_scatter_info::compute_base_index_addr_contribution(
  const PIN_REGISTER& vector_index_reg_val, UINT32 lane_id) const {
  ADDRDELTA index_val;
  switch(_index_lane_width_bytes) {
    case 4:
      index_val = vector_index_reg_val.s_dword[lane_id];
      break;
    case 8:
      index_val = vector_index_reg_val.s_qword[lane_id];
      break;
    default:
      ASSERTX(false);
      break;
  }
  return index_val;
}

gather_scatter_info::type gather_scatter_info::get_type() const {
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
  os << "_type: " << sinfo.type_to_string[sinfo._type] << std::endl;
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
  _kmask_reg = pin_reg;
  ASSERTX(REG_is_k_mask(_kmask_reg));
}

void gather_scatter_info::set_base_reg(REG pin_reg) {
  // make sure base reg not set yet
  ASSERTX(!REG_valid(_base_reg));
  if(REG_valid(pin_reg)) {
    _base_reg = pin_reg;
    ASSERTX(REG_is_gr64(pin_reg) || REG_is_gr32(pin_reg));
  }
}

void gather_scatter_info::set_index_reg(REG pin_reg) {
  // make sure index reg not set yet
  ASSERTX(!REG_valid(_index_reg));
  if(REG_valid(pin_reg)) {
    _index_reg = pin_reg;
    ASSERTX(REG_is_xmm_ymm_zmm(_index_reg));
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
  ASSERTX(is_non_zero_and_powerof2(_scale));
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

  UINT32 index_xyzmm_reg_width_bytes = pin_xyzmm_reg_width_in_bytes(_index_reg);
  ASSERTX(is_non_zero_and_powerof2(index_xyzmm_reg_width_bytes));
  ASSERTX(is_non_zero_and_powerof2(_index_lane_width_bytes));
  UINT32 num_index_lanes = index_xyzmm_reg_width_bytes /
                           _index_lane_width_bytes;
  ASSERTX(is_non_zero_and_powerof2(num_index_lanes));

  _num_mem_ops = std::min(num_data_lanes, num_index_lanes);
  ASSERTX(is_non_zero_and_powerof2(_num_mem_ops));
}

void gather_scatter_info::verify_fields_for_mem_access_info_generation() const {
  ASSERTX(REG_is_k_mask(_kmask_reg));
  if(REG_valid(_base_reg)) {
    ASSERTX(REG_is_gr64(_base_reg) || REG_is_gr32(_base_reg));
  }
  ASSERTX(REG_is_xmm_ymm_zmm(_index_reg));
  ASSERTX(is_non_zero_and_powerof2(_scale));
  ASSERTX((4 == _index_lane_width_bytes) || (8 == _index_lane_width_bytes));
  ASSERTX((4 == _data_lane_width_bytes) || (8 == _data_lane_width_bytes));
  ASSERTX(is_non_zero_and_powerof2(_num_mem_ops));
}

vector<PIN_MEM_ACCESS_INFO> gather_scatter_info::compute_mem_access_infos(
  const CONTEXT* ctxt) const {
  verify_fields_for_mem_access_info_generation();

  vector<PIN_MEM_ACCESS_INFO> mem_access_infos;
  ADDRINT base_addr_contribution = compute_base_reg_addr_contribution(ctxt);
  PIN_MEMOP_ENUM memop_type      = type_to_PIN_MEMOP_ENUM();

  PIN_REGISTER vector_index_reg_val_buf;
  PIN_GetContextRegval(ctxt, _index_reg, (UINT8*)&vector_index_reg_val_buf);
  PIN_REGISTER mask_reg_val_buf;
  PIN_GetContextRegval(ctxt, _kmask_reg, (UINT8*)&mask_reg_val_buf);
  for(UINT32 lane_id = 0; lane_id < _num_mem_ops; lane_id++) {
    ADDRDELTA index_val = compute_base_index_addr_contribution(
      vector_index_reg_val_buf, lane_id);
    ADDRINT final_addr = base_addr_contribution + (index_val * _scale) +
                         _displacement;
    bool                mask_on = (mask_reg_val_buf.word[0] & (1 << lane_id));
    PIN_MEM_ACCESS_INFO access_info = {.memoryAddress = final_addr,
                                       .memopType     = memop_type,
                                       .bytesAccessed = _data_lane_width_bytes,
                                       .maskOn        = mask_on};

    mem_access_infos.push_back(access_info);
  }

  return mem_access_infos;
}

bool gather_scatter_info::base_reg_is_gr32() const {
  if(REG_valid(_base_reg) && REG_is_gr32(_base_reg)) {
    return true;
  }
  return false;
}
