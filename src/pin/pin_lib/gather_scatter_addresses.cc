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
#include <cassert>
#include <iostream>
#include <map>

// Global static instruction map just for scatter instructions
scatter_info_map scatter_info_storage;

gather_scatter_info add_to_gather_scatter_info_storage(
  const ADDRINT iaddr, const bool is_gather, const bool is_scatter,
  const xed_category_enum_t category) {
  assert(!(is_gather && is_scatter));
  assert(is_gather || is_scatter);
  gather_scatter_info::type type = is_gather ? gather_scatter_info::GATHER :
                                               gather_scatter_info::SCATTER;
  gather_scatter_info::mask_reg_type mask_reg_type;
  switch(category) {
    case XED_CATEGORY_AVX2GATHER:
      mask_reg_type = gather_scatter_info::XYMM;
      break;

    case XED_CATEGORY_GATHER:
    case XED_CATEGORY_SCATTER:
      mask_reg_type = gather_scatter_info::K;
      break;

    default:
      std::cout << std::string(
                     "unexpected category for gather/scatter instruction: ") +
                     std::string(XED_CATEGORY_StringShort(category))
                << std::endl;
      assert(0);
      break;
  }
  return gather_scatter_info(type, mask_reg_type);
}

static void set_gather_scatter_data_width(
  const ADDRINT iaddr, xed_reg_enum_t xed_reg, const bool operandRead,
  const bool operandWritten, const gather_scatter_info::type info_type,
  const gather_scatter_info::mask_reg_type mask_reg_type) {
  assert(XED_REG_is_xmm_ymm_zmm(xed_reg));
  switch(info_type) {
    case gather_scatter_info::GATHER:
      switch(mask_reg_type) {
        case gather_scatter_info::K:
          assert(!operandRead && operandWritten);
          break;
        case gather_scatter_info::XYMM:
          // there's a bug in PIN as late as version 3.13 where INS_OperandRead
          // returns true for the destination reg (i.e., reg that we are
          // gathering into) for a AVX2 gather
          assert(operandRead && operandWritten);
          break;
        default:
          assert(false);
          break;
      }
      break;

    case gather_scatter_info::SCATTER:
      assert(operandRead && !operandWritten);
      break;

    default:
      assert(false);
      break;
  }
  scatter_info_storage[iaddr].set_data_reg_total_width(xed_reg);
}

void set_gather_scatter_reg_operand_info(const ADDRINT        iaddr,
                                         const xed_reg_enum_t pin_reg,
                                         const bool           operandRead,
                                         const bool           operandWritten) {
  const gather_scatter_info::type info_type =
    scatter_info_storage[iaddr].get_type();
  const gather_scatter_info::mask_reg_type mask_reg_type =
    scatter_info_storage[iaddr].get_mask_reg_type();

  assert(gather_scatter_info::INVALID_TYPE != info_type);
  assert(gather_scatter_info::INVALID_MASK_REG_TYPE != mask_reg_type);

  if(XED_REG_is_k_mask(pin_reg)) {
    assert(operandRead);
    assert(operandWritten);
    assert(gather_scatter_info::K == mask_reg_type);
    scatter_info_storage[iaddr].set_mask_reg(pin_reg);
  } else {
    assert(XED_REG_is_xmm_ymm_zmm(pin_reg));
    // for AVX2 gathers, both the destination
    // register (i.e., the register that we gather into) and the mask register
    // (i.e., the register that controls whether each lane gets predicated) are
    // xmm/ymm registers. Unfortunate, PIN has a big (as late as 3.13) that
    // marks operandRead for the destination register, even though it shouldn't,
    // which means it's impossible to tell whether a given xmm/ymm register is
    // the destination register, or the mask by looking at operandRead and
    // operandWritten. It appears PIN will always provide the destination
    // register first, so we first check of the mask_reg is set; if it is
    // already set, then we assume the incoming pin_reg is a mask_reg.
    // Otherwise, we assume it's the destination register
    if((gather_scatter_info::XYMM == mask_reg_type) &&
       scatter_info_storage[iaddr].data_dest_reg_set()) {
      scatter_info_storage[iaddr].set_mask_reg(pin_reg);
    } else {
      set_gather_scatter_data_width(iaddr, pin_reg, operandRead, operandWritten,
                                    info_type, mask_reg_type);
    }
  }
}

void set_gather_scatter_memory_operand_info(
  const ADDRINT iaddr, const xed_reg_enum_t pin_base_reg,
  const xed_reg_enum_t                            pin_index_reg,
  /*const uint64_t displacement,*/ const uint32_t scale,
  const bool operandReadOnly, const bool operandWritenOnly) {
  switch(scatter_info_storage[iaddr].get_type()) {
    case gather_scatter_info::GATHER:
      assert(operandReadOnly);
      break;
    case gather_scatter_info::SCATTER:
      assert(operandWritenOnly);
      break;
    default:
      assert(false);
      break;
  }
  scatter_info_storage[iaddr].set_base_reg(pin_base_reg);
  scatter_info_storage[iaddr].set_index_reg(pin_index_reg);
  // scatter_info_storage[iaddr].set_displacement(displacement);
  scatter_info_storage[iaddr].set_scale(scale);
}

static void set_info_num_ld_or_st(const ADDRINT iaddr, ctype_pin_inst* info) {
  // set info->num_ld/st to the total number of mem ops (both mask on and off)
  // However, when we actually generate the compressed op, we're going to set
  // the num_ld/st of the compressed op to just the number of masked mem ops
  assert(info->is_simd);
  assert(info->is_gather_scatter);

  uint32_t total_mask_on_and_off_mem_ops =
    scatter_info_storage[iaddr].get_num_mem_ops();
  switch(scatter_info_storage[iaddr].get_type()) {
    case gather_scatter_info::GATHER:
      // should be set to 1 in decoder.cc:add_dependency_info, because PIN
      // treats gathers as having 1 memory operand
      assert(1 == info->num_ld);
      info->num_ld = total_mask_on_and_off_mem_ops;
      break;
    case gather_scatter_info::SCATTER:
      // should be set to 1 in decoder.cc:add_dependency_info, because PIN
      // treats scatters as having 1 memory operand
      assert(1 == info->num_st);
      info->num_st = total_mask_on_and_off_mem_ops;
      break;
    default:
      assert(false);
      break;
  }
}

void finalize_scatter_info(const ADDRINT iaddr, ctype_pin_inst* info) {
  assert(info->is_simd);
  assert(info->is_gather_scatter);

  switch(scatter_info_storage[iaddr].get_type()) {
    case gather_scatter_info::GATHER:
      scatter_info_storage[iaddr].set_data_lane_width_bytes(info->ld_size);
      break;
    case gather_scatter_info::SCATTER:
      scatter_info_storage[iaddr].set_data_lane_width_bytes(info->st_size);
      break;
    default:
      assert(false);
      break;
  }
  scatter_info_storage[iaddr].set_index_lane_width_bytes(
    info->lane_width_bytes);
  scatter_info_storage[iaddr].compute_num_mem_ops();
  scatter_info_storage[iaddr].verify_fields_for_mem_access_info_generation();

  // set info->num_ld/st to the total number of mem ops (both mask on and off)
  // However, when we actually generate the compressed op, we're going to set
  // the num_ld/st of the compressed op to just the number of masked on mem ops
  set_info_num_ld_or_st(iaddr, info);
}

/*void update_gather_scatter_num_ld_or_st(const ADDRINT                   iaddr,
                                        const gather_scatter_info::type type,
                                        const uint      num_maskon_memops,
                                        ctype_pin_inst* info) {
  assert(info->is_simd);
  assert(info->is_gather_scatter);
  assert(1 == scatter_info_storage.count(iaddr));
  assert(type == scatter_info_storage[iaddr].get_type());
  // number of actual mask on loads/stores should be less or equal to total of
  // mem ops (both mask on and off) in the gather/scatter instruction
  uint32_t total_mask_on_and_off_mem_ops =
    scatter_info_storage[iaddr].get_num_mem_ops();
  switch(type) {
    case gather_scatter_info::GATHER:
      assert(num_maskon_memops <= total_mask_on_and_off_mem_ops);
      info->num_ld = num_maskon_memops;
      break;
    case gather_scatter_info::SCATTER:
      assert(num_maskon_memops <= total_mask_on_and_off_mem_ops);
      info->num_st = num_maskon_memops;
      break;
    default:
      assert(false);
      break;
  }
}*/

/*static void verify_mem_access_infos(
  const vector<PIN_MEM_ACCESS_INFO> computed_infos,
  const PIN_MULTI_MEM_ACCESS_INFO* infos_from_pin, bool base_reg_is_gr32) {
  for(uint32_t lane_id = 0; lane_id < infos_from_pin->numberOfMemops; lane_id++)
  { ADDRINT        addr_from_pin = infos_from_pin->memop[lane_id].memoryAddress;
    PIN_MEMOP_ENUM type_from_pin = infos_from_pin->memop[lane_id].memopType;
    uint32_t         size_from_pin =
  infos_from_pin->memop[lane_id].bytesAccessed; bool           mask_on_from_pin
  = infos_from_pin->memop[lane_id].maskOn;

    // as late as PIN 3.13, there is a bug where PIN will not correctly compute
    // the full 64 addresses of gathers/scatters if the base register is a
    // 32-bit register and holds a negative value. The low 32 bits, however,
    // appear to be correct, so we check against that
    ADDRINT addr_mask;
    if(base_reg_is_gr32) {
      addr_mask = 0xFFFFFFFF;
    } else {
      addr_mask = -1;
    }
    assert((computed_infos[lane_id].memoryAddress & addr_mask) ==
            (addr_from_pin & addr_mask));
    assert(computed_infos[lane_id].memopType == type_from_pin);
    assert(computed_infos[lane_id].bytesAccessed == size_from_pin);
    assert(computed_infos[lane_id].maskOn == mask_on_from_pin);
  }
  }*/

bool gather_scatter_info::is_non_zero_and_powerof2(const uint32_t v) const {
  return v && ((v & (v - 1)) == 0);
}

uint32_t gather_scatter_info::pin_xyzmm_reg_width_in_bytes(
  const xed_reg_enum_t pin_xyzmm_reg) const {
  assert(XED_REG_is_xmm_ymm_zmm(pin_xyzmm_reg));
  switch(XED_REG_Width(pin_xyzmm_reg)) {
    case XED_REGWIDTH_128:
      return 16;
    case XED_REGWIDTH_256:
      return 32;
    case XED_REGWIDTH_512:
      return 64;
    default:
      assert(false);
      return 0;
  }
}

gather_scatter_info::type gather_scatter_info::get_type() const {
  return _type;
}

gather_scatter_info::mask_reg_type gather_scatter_info::get_mask_reg_type()
  const {
  return _mask_reg_type;
}

xed_reg_enum_t gather_scatter_info::get_mask_reg() const {
  return _mask_reg;
}
xed_reg_enum_t gather_scatter_info::get_base_reg() const {
  return _base_reg;
}

xed_reg_enum_t gather_scatter_info::get_index_reg() const {
  return _index_reg;
}

uint64_t gather_scatter_info::get_displacement() const {
  return _displacement;
}

uint32_t gather_scatter_info::get_scale() const {
  return _scale;
}

uint32_t gather_scatter_info::get_index_lane_width_bytes() const {
  return _index_lane_width_bytes;
}

uint32_t gather_scatter_info::get_data_lane_width_bytes() const {
  return _data_lane_width_bytes;
}

bool gather_scatter_info::data_dest_reg_set() const {
  return 0 != _data_vector_reg_total_width_bytes;
}

gather_scatter_info::gather_scatter_info() {
  _type          = gather_scatter_info::INVALID_TYPE;
  _mask_reg_type = gather_scatter_info::INVALID_MASK_REG_TYPE;
  _data_vector_reg_total_width_bytes = 0;
  _data_lane_width_bytes             = 0;
  _mask_reg                          = XED_REG_INVALID();
  _base_reg                          = XED_REG_INVALID();
  _index_reg                         = XED_REG_INVALID();
  _displacement                      = 0;
  _scale                             = 0;
  _index_lane_width_bytes            = 0;
  _num_mem_ops                       = 0;
}

gather_scatter_info::gather_scatter_info(
  const gather_scatter_info::type          given_type,
  const gather_scatter_info::mask_reg_type given_mask_reg_type) :
    gather_scatter_info::gather_scatter_info() {
  _type          = given_type;
  _mask_reg_type = given_mask_reg_type;
}

gather_scatter_info::~gather_scatter_info() {}

std::ostream& operator<<(std::ostream& os, const gather_scatter_info& sinfo) {
  os << "_type: " << sinfo.type_to_string[sinfo._type] << std::endl;
  os << "_data_vector_reg_total_width_bytes: " << std::dec
     << sinfo._data_vector_reg_total_width_bytes << std::endl;
  os << "_data_lane_width_bytes " << std::dec << sinfo._data_lane_width_bytes
     << std::endl;
  os << "_mask_reg_type: " << sinfo.type_to_string[sinfo._mask_reg_type]
     << std::endl;
  os << "_k_mask_reg: " << XED_REG_StringShort(sinfo._mask_reg) << std::endl;
  os << "_base_reg: " << XED_REG_StringShort(sinfo._base_reg) << std::endl;
  os << "_index_reg: " << XED_REG_StringShort(sinfo._index_reg) << std::endl;
  os << "_displacement: 0x" << std::hex << sinfo._displacement << std::endl;
  os << "_scale: " << std::dec << sinfo._scale << std::endl;
  os << "_index_lane_width_bytes " << std::dec << sinfo._index_lane_width_bytes
     << std::endl;
  os << "_num_mem_ops: " << std::dec << sinfo._num_mem_ops << std::endl;
  return os;
}

void gather_scatter_info::set_data_reg_total_width(xed_reg_enum_t xed_reg) {
  // make sure data vector total width not set yet
  assert(!data_dest_reg_set());
  assert(XED_REG_is_xmm_ymm_zmm(xed_reg));
  _data_vector_reg_total_width_bytes = XED_REG_Size(xed_reg);
  assert(data_dest_reg_set());
}

void gather_scatter_info::set_data_lane_width_bytes(
  const uint32_t st_lane_width) {
  assert(0 == _data_lane_width_bytes);
  _data_lane_width_bytes = st_lane_width;
  assert(0 != _data_lane_width_bytes);
}

void gather_scatter_info::verify_mask_reg() const {
  switch(_mask_reg_type) {
    case gather_scatter_info::K:
      assert(XED_REG_is_k_mask(_mask_reg));
      break;
    case gather_scatter_info::XYMM:
      assert(data_dest_reg_set());
      assert(XED_REG_is_xmm(_mask_reg) || XED_REG_is_ymm(_mask_reg));
      // for all AVX2 gather instructions, the width of the mask xmm/ymm
      // register and the destination register should be the same
      assert(XED_REG_Size(_mask_reg) == _data_vector_reg_total_width_bytes);
      break;
    default:
      std::cout << std::string("unexpected mask reg type") << std::endl;
      assert(0);
      break;
  }
}

void gather_scatter_info::set_mask_reg(const xed_reg_enum_t pin_reg) {
  assert(!XED_REG_valid(_mask_reg));  // make sure kmask not set yet
  _mask_reg = pin_reg;
  verify_mask_reg();
}

void gather_scatter_info::set_base_reg(const xed_reg_enum_t pin_reg) {
  // make sure base reg not set yet
  assert(!XED_REG_valid(_base_reg));
  if(XED_REG_valid(pin_reg)) {
    _base_reg = pin_reg;
    assert(XED_REG_is_gr64(pin_reg) || XED_REG_is_gr32(pin_reg));
  }
}

void gather_scatter_info::set_index_reg(const xed_reg_enum_t pin_reg) {
  // make sure index reg not set yet
  assert(!XED_REG_valid(_index_reg));
  if(XED_REG_valid(pin_reg)) {
    _index_reg = pin_reg;
    assert(XED_REG_is_xmm_ymm_zmm(_index_reg));
  }
}

void gather_scatter_info::set_displacement(const uint64_t displacement) {
  assert(0 == _displacement);
  _displacement = displacement;
  // displacement could still be 0, because not every scatter has a displacement
}

void gather_scatter_info::set_scale(const uint32_t scale) {
  assert(0 == _scale);
  _scale = scale;
  assert(is_non_zero_and_powerof2(_scale));
}

void gather_scatter_info::set_index_lane_width_bytes(
  const uint32_t idx_lane_width) {
  assert(0 == _index_lane_width_bytes);
  _index_lane_width_bytes = idx_lane_width;
  // only expecting doubleword or quadword indices
  assert((4 == _index_lane_width_bytes) || (8 == _index_lane_width_bytes));
}

void gather_scatter_info::compute_num_mem_ops() {
  assert(0 == _num_mem_ops);

  assert(is_non_zero_and_powerof2(_data_vector_reg_total_width_bytes));
  assert(is_non_zero_and_powerof2(_data_lane_width_bytes));
  uint32_t num_data_lanes = _data_vector_reg_total_width_bytes /
                            _data_lane_width_bytes;
  assert(is_non_zero_and_powerof2(num_data_lanes));

  uint32_t index_xyzmm_reg_width_bytes = pin_xyzmm_reg_width_in_bytes(
    _index_reg);
  assert(is_non_zero_and_powerof2(index_xyzmm_reg_width_bytes));
  assert(is_non_zero_and_powerof2(_index_lane_width_bytes));
  uint32_t num_index_lanes = index_xyzmm_reg_width_bytes /
                             _index_lane_width_bytes;
  assert(is_non_zero_and_powerof2(num_index_lanes));

  _num_mem_ops = std::min(num_data_lanes, num_index_lanes);
  assert(is_non_zero_and_powerof2(_num_mem_ops));
}

uint32_t gather_scatter_info::get_num_mem_ops() const {
  assert(is_non_zero_and_powerof2(_num_mem_ops));
  return _num_mem_ops;
}

void gather_scatter_info::verify_fields_for_mem_access_info_generation() const {
  verify_mask_reg();
  if(XED_REG_valid(_base_reg)) {
    assert(XED_REG_is_gr64(_base_reg) || XED_REG_is_gr32(_base_reg));
  }
  assert(XED_REG_is_xmm_ymm_zmm(_index_reg));
  assert(is_non_zero_and_powerof2(_scale));
  assert((4 == _index_lane_width_bytes) || (8 == _index_lane_width_bytes));
  assert((4 == _data_lane_width_bytes) || (8 == _data_lane_width_bytes));
  assert(is_non_zero_and_powerof2(_num_mem_ops));
}

/*
//TODO: extract displacement information
vector<PIN_MEM_ACCESS_INFO> gather_scatter_info::compute_mem_access_infos(
  const CONTEXT* ctxt) const {
  verify_fields_for_mem_access_info_generation();

  vector<PIN_MEM_ACCESS_INFO> mem_access_infos;
  ADDRINT base_addr_contribution = compute_base_reg_addr_contribution(ctxt);
  PIN_MEMOP_ENUM memop_type      = type_to_PIN_MEMOP_ENUM();

  PIN_REGISTER vector_index_reg_val_buf;
  assert(reg_xed_to_pin_map.find(_index_reg) != reg_xed_to_pin_map.end());
  PIN_GetContextRegval(ctxt, reg_xed_to_pin_map[_index_reg],
(UINT8*)&vector_index_reg_val_buf); PIN_REGISTER mask_reg_val_buf;
  assert(reg_xed_to_pin_map.find(_mask_reg) != reg_xed_to_pin_map.end());
  PIN_GetContextRegval(ctxt, reg_xed_to_pin_map[_mask_reg],
(UINT8*)&mask_reg_val_buf); for(uint32_t lane_id = 0; lane_id < _num_mem_ops;
lane_id++) { uint64_t index_val = compute_base_index_addr_contribution(
      vector_index_reg_val_buf, lane_id);
    ADDRINT final_addr = base_addr_contribution + (index_val * _scale) +
                         _displacement;
    bool                mask_on = extract_mask_on(mask_reg_val_buf, lane_id);
    PIN_MEM_ACCESS_INFO access_info = {.memoryAddress = final_addr,
                                       .memopType     = memop_type,
                                       .bytesAccessed = _data_lane_width_bytes,
                                       .maskOn        = mask_on};

    mem_access_infos.push_back(access_info);
  }

  return mem_access_infos;
}
*/

bool gather_scatter_info::base_reg_is_gr32() const {
  if(XED_REG_valid(_base_reg) && XED_REG_is_gr32(_base_reg)) {
    return true;
  }
  return false;
}
