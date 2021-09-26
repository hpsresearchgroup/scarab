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

#include <iostream>
#include <set>
#include <string>
#include <unordered_map>

#include "decoder.h"
#include "gather_scatter_addresses.h"
#include "x87_stack_delta.h"

#include "pin_scarab_common_lib.h"


using namespace std;

KNOB<BOOL> Knob_debug(KNOB_MODE_WRITEONCE, "pintool", "debug", "0",
                      "always add instructions to print map");
KNOB<BOOL> Knob_translate_x87_regs(
  KNOB_MODE_WRITEONCE, "pintool", "translate_x87_regs", "1",
  "translate Pin's relative x87 regs to Scarab's absolute regs");

// the most recently filled instruction
static ctype_pin_inst* filled_inst_info;
static ctype_pin_inst  tmp_inst_info;


/************************** Data Type Definitions *****************************/

// Globals used for communication between analysis functions
uint32_t       glb_opcode, glb_actually_taken;
deque<ADDRINT> glb_ld_vaddrs, glb_st_vaddrs;

std::ostream*                                    glb_err_ostream;
bool                                             glb_translate_x87_regs;
std::set<std::string>                            unknown_opcodes;
inst_info_map                                    inst_info_storage;
static std::map<xed_reg_enum_t, LEVEL_BASE::REG> reg_xed_to_pin_map;
extern scatter_info_map                          scatter_info_storage;

/********************* Private Functions Prototypes ***************************/
ctype_pin_inst* get_inst_info_obj(const INS& ins);

void insert_analysis_functions(ctype_pin_inst* info, const INS& ins);
void print_err_if_invalid(ctype_pin_inst* info, const INS& ins);

void get_opcode(UINT32 opcode);
void get_gather_scatter_eas(bool is_gather, CONTEXT* ctxt,
                            PIN_MULTI_MEM_ACCESS_INFO* mem_access_info);
void get_ld_ea(ADDRINT addr);
void get_ld_ea2(ADDRINT addr1, ADDRINT addr2);
void get_st_ea(ADDRINT addr);
void get_branch_dir(bool taken);
void create_compressed_op(ADDRINT iaddr);

void update_gather_scatter_num_ld_or_st(const ADDRINT                   iaddr,
                                        const gather_scatter_info::type type,
                                        const uint      num_maskon_memops,
                                        ctype_pin_inst* info);

static void verify_mem_access_infos(
  const vector<PIN_MEM_ACCESS_INFO> computed_infos,
  const PIN_MULTI_MEM_ACCESS_INFO* infos_from_pin, bool base_reg_is_gr32);
std::vector<PIN_MEM_ACCESS_INFO> compute_mem_access_infos(
  const CONTEXT* ctxt, gather_scatter_info* info);
ADDRDELTA compute_base_reg_addr_contribution(const CONTEXT*       ctxt,
                                             gather_scatter_info* info);
ADDRDELTA compute_base_index_addr_contribution(
  const PIN_REGISTER& vector_index_reg_val, const UINT32 lane_id,
  gather_scatter_info* info);
PIN_MEMOP_ENUM type_to_PIN_MEMOP_ENUM(gather_scatter_info* info);
bool extract_mask_on(const PIN_REGISTER& mask_reg_val_buf, const UINT32 lane_id,
                     gather_scatter_info* info);

/**************************** Public Functions ********************************/
void pin_decoder_init(bool translate_x87_regs, std::ostream* err_ostream) {
  init_x86_decoder(err_ostream);
  init_reg_xed_to_pin_map();
  init_x87_stack_delta();
  glb_translate_x87_regs = translate_x87_regs;
  if(err_ostream) {
    glb_err_ostream = err_ostream;
  } else {
    glb_err_ostream = &std::cout;
  }
}

void pin_decoder_insert_analysis_functions(const INS& ins) {
  ctype_pin_inst*     info    = get_inst_info_obj(ins);
  xed_decoded_inst_t* xed_ins = INS_XedDec(ins);
  fill_in_basic_info(info, xed_ins);

  info->instruction_addr = INS_Address(ins);
  // Note: should be overwritten for a taken control flow instruction
  info->instruction_next_addr = INS_NextAddress(ins);
  if(INS_IsDirectBranchOrCall(ins)) {
    info->branch_target = INS_DirectBranchOrCallTargetAddress(ins);
  }

  if(INS_IsVgather(ins) || INS_IsVscatter(ins)) {
    xed_category_enum_t category           = XED_INS_Category(xed_ins);
    scatter_info_storage[INS_Address(ins)] = add_to_gather_scatter_info_storage(
      INS_Address(ins), INS_IsVgather(ins), INS_IsVscatter(ins), category);
  }
  uint32_t max_op_width = add_dependency_info(info, xed_ins);
  fill_in_simd_info(info, xed_ins, max_op_width);
  if(INS_IsVgather(ins) || INS_IsVscatter(ins)) {
    finalize_scatter_info(INS_Address(ins), info);
  }
  apply_x87_bug_workaround(info, xed_ins);
  fill_in_cf_info(info, xed_ins);
  insert_analysis_functions(info, ins);
  print_err_if_invalid(info, xed_ins);
}

ctype_pin_inst* pin_decoder_get_latest_inst() {
  return filled_inst_info;
}

void pin_decoder_print_unknown_opcodes() {
  for(const auto opcode : unknown_opcodes) {
    (*glb_err_ostream) << opcode << std::endl;
  }
}

ctype_pin_inst create_sentinel() {
  ctype_pin_inst inst;
  memset(&inst, 0, sizeof(inst));
  inst.op_type     = OP_INV;
  inst.is_sentinel = 1;
  strcpy(inst.pin_iclass, "SENTINEL");
  return inst;
}

ctype_pin_inst create_dummy_jump(uint64_t eip, uint64_t tgt) {
  ctype_pin_inst inst;
  memset(&inst, 0, sizeof(inst));
  inst.instruction_addr = eip;
  inst.size             = 1;
  inst.op_type          = OP_IADD;
  inst.cf_type          = CF_BR;
  inst.num_simd_lanes   = 1;
  inst.lane_width_bytes = 1;
  inst.branch_target    = tgt;
  inst.actually_taken   = 1;
  inst.fake_inst        = 1;
  strcpy(inst.pin_iclass, "DUMMY_JMP");
  return inst;
}

ctype_pin_inst create_dummy_nop(uint64_t                  eip,
                                Wrongpath_Nop_Mode_Reason reason) {
  ctype_pin_inst inst;
  memset(&inst, 0, sizeof(inst));
  inst.instruction_addr      = eip;
  inst.instruction_next_addr = eip + 1;
  inst.size                  = 1;
  inst.op_type               = OP_NOP;
  strcpy(inst.pin_iclass, "DUMMY_NOP");
  inst.fake_inst        = 1;
  inst.fake_inst_reason = reason;
  return inst;
}

/*************************** Private Functions  *******************************/
ctype_pin_inst* get_inst_info_obj(const INS& ins) {
  ctype_pin_inst* info = (ctype_pin_inst*)calloc(1, sizeof(ctype_pin_inst));
  inst_info_map_p lp   = inst_info_storage.find(INS_Address(ins));
  if(lp == inst_info_storage.end()) {
    inst_info_storage[INS_Address(ins)] = info;
  } else {
    // TODO_b: should we free the existing ctype_pin_inst at
    // inst_info_storage[INS_Address(ins)]?
  }
  return info;
}

void insert_analysis_functions(ctype_pin_inst* info, const INS& ins) {
  if(Knob_translate_x87_regs.Value()) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)get_opcode, IARG_UINT32,
                   INS_Opcode(ins), IARG_END);
  }

  if(INS_IsVgather(ins) || INS_IsVscatter(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)get_gather_scatter_eas,
                   IARG_BOOL, INS_IsVgather(ins), IARG_CONTEXT,
                   IARG_MULTI_MEMORYACCESS_EA, IARG_END);
  } else {
    if(INS_IsMemoryRead(ins)) {
      if(INS_HasMemoryRead2(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)get_ld_ea2,
                       IARG_MEMORYREAD_EA, IARG_MEMORYREAD2_EA, IARG_END);
      } else {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)get_ld_ea,
                       IARG_MEMORYREAD_EA, IARG_END);
      }
    }

    if(INS_IsMemoryWrite(ins)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)get_st_ea,
                     IARG_MEMORYWRITE_EA, IARG_END);
    }
  }

  if(info->cf_type) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)get_branch_dir,
                   IARG_BRANCH_TAKEN, IARG_END);
  }

  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)create_compressed_op,
                 IARG_INST_PTR, IARG_END);
}

// int64_t heartbeat = 0;
void create_compressed_op(ADDRINT iaddr) {
  if(!fast_forward_count) {
    assert(inst_info_storage.count(iaddr) == 1);
    filled_inst_info = inst_info_storage[iaddr];
    if(glb_translate_x87_regs) {
      // copy ctype_pin_inst to avoid clobbering it
      memcpy(&tmp_inst_info, filled_inst_info, sizeof(ctype_pin_inst));
      filled_inst_info = &tmp_inst_info;
      // translate registers (no need to translate agen, since they won't be
      // FP)
      for(int i = 0; i < filled_inst_info->num_src_regs; ++i) {
        filled_inst_info->src_regs[i] = absolute_reg(
          filled_inst_info->src_regs[i], glb_opcode, false);
      }
      for(int i = 0; i < filled_inst_info->num_dst_regs; ++i) {
        filled_inst_info->dst_regs[i] = absolute_reg(
          filled_inst_info->dst_regs[i], glb_opcode, true);
      }
      // update x87 state
      update_x87_stack_state(glb_opcode);
    }

    uint num_lds = glb_ld_vaddrs.size();
    assert(num_lds <= MAX_LD_NUM);
    if(filled_inst_info->num_ld != num_lds) {
      update_gather_scatter_num_ld_or_st(iaddr, gather_scatter_info::GATHER,
                                         num_lds, filled_inst_info);
    }
    assert(filled_inst_info->num_ld == num_lds);
    for(uint ld = 0; ld < num_lds; ld++) {
      filled_inst_info->ld_vaddr[ld] = glb_ld_vaddrs[ld];
    }

    uint num_sts = glb_st_vaddrs.size();
    assert(num_sts <= MAX_ST_NUM);
    if(filled_inst_info->num_st != num_sts) {
      update_gather_scatter_num_ld_or_st(iaddr, gather_scatter_info::SCATTER,
                                         num_sts, filled_inst_info);
    }
    assert(filled_inst_info->num_st == num_sts);
    for(uint st = 0; st < num_sts; st++) {
      filled_inst_info->st_vaddr[st] = glb_st_vaddrs[st];
    }

    filled_inst_info->actually_taken = glb_actually_taken;
  }
  glb_opcode = 0;
  glb_ld_vaddrs.clear();
  glb_st_vaddrs.clear();
  glb_actually_taken = 0;

  // if (heartbeat % 100000000 == 0) {
  //  (*glb_err_ostream) << "Heartbeat: " << heartbeat << std::endl <<
  //  std::flush;
  //}
  // heartbeat += 1;
}

void get_gather_scatter_eas(bool is_gather, CONTEXT* ctxt,
                            PIN_MULTI_MEM_ACCESS_INFO* mem_access_info) {
  const vector<PIN_MEM_ACCESS_INFO> gather_scatter_mem_access_infos =
    get_gather_scatter_mem_access_infos_from_gather_scatter_info(
      ctxt, mem_access_info);
  UINT32 num_mem_accesses = gather_scatter_mem_access_infos.size();

  for(UINT32 i = 0; i < num_mem_accesses; i++) {
    ADDRINT        addr    = gather_scatter_mem_access_infos[i].memoryAddress;
    PIN_MEMOP_ENUM type    = gather_scatter_mem_access_infos[i].memopType;
    bool           mask_on = gather_scatter_mem_access_infos[i].maskOn;
    bool           is_load = (type == PIN_MEMOP_LOAD);

    ASSERTX(type == (is_gather ? PIN_MEMOP_LOAD : PIN_MEMOP_STORE));

    // TODO: get rid of the print
    // UINT32         size    =
    // gather_scatter_mem_access_infos[i].bytesAccessed;
    // (*glb_err_ostream) << (mask_on ? "(mask on) " : "(mask off)  ")
    //                    << (is_load ? "load" : "store") << " memop to "
    //                    << std::dec << size << "@" << StringFromAddrint(addr)
    //                    << std::endl;

    // only let Scarab know about it if the memop is not masked away
    if(mask_on) {
      if(is_load)
        glb_ld_vaddrs.push_back(addr);
      else
        glb_st_vaddrs.push_back(addr);
    }
  }
}

void get_opcode(UINT32 opcode) {
  glb_opcode = opcode;
}

void get_ld_ea(ADDRINT addr) {
  glb_ld_vaddrs.push_back(addr);
}

void get_ld_ea2(ADDRINT addr1, ADDRINT addr2) {
  glb_ld_vaddrs.push_back(addr1);
  glb_ld_vaddrs.push_back(addr2);
}

void get_st_ea(ADDRINT addr) {
  glb_st_vaddrs.push_back(addr);
}

void get_branch_dir(bool taken) {
  glb_actually_taken = taken;
}

void update_gather_scatter_num_ld_or_st(const ADDRINT                   iaddr,
                                        const gather_scatter_info::type type,
                                        const uint      num_maskon_memops,
                                        ctype_pin_inst* info) {
  ASSERTX(info->is_simd);
  ASSERTX(info->is_gather_scatter);
  ASSERTX(1 == scatter_info_storage.count(iaddr));
  ASSERTX(type == scatter_info_storage[iaddr].get_type());
  // number of actual mask on loads/stores should be less or equal to total of
  // mem ops (both mask on and off) in the gather/scatter instruction
  UINT32 total_mask_on_and_off_mem_ops =
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
      ASSERTX(false);
      break;
  }
}

vector<PIN_MEM_ACCESS_INFO>
  get_gather_scatter_mem_access_infos_from_gather_scatter_info(
    const CONTEXT* ctxt, const PIN_MULTI_MEM_ACCESS_INFO* infos_from_pin) {
  ADDRINT iaddr;
  PIN_GetContextRegval(ctxt, REG_INST_PTR, (UINT8*)&iaddr);
  ASSERTX(1 == scatter_info_storage.count(iaddr));
  vector<PIN_MEM_ACCESS_INFO> computed_infos = compute_mem_access_infos(
    ctxt, &scatter_info_storage[iaddr]);
  verify_mem_access_infos(computed_infos, infos_from_pin,
                          scatter_info_storage[iaddr].base_reg_is_gr32());

  return computed_infos;
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
    // the full 64 addresses of gathers/scatters if the base register is a
    // 32-bit register and holds a negative value. The low 32 bits, however,
    // appear to be correct, so we check against that
    ADDRINT addr_mask;
    if(base_reg_is_gr32) {
      addr_mask = 0xFFFFFFFF;
    } else {
      addr_mask = -1;
    }
    ASSERTX((computed_infos[lane_id].memoryAddress & addr_mask) ==
            (addr_from_pin & addr_mask));
    ASSERTX(computed_infos[lane_id].memopType == type_from_pin);
    ASSERTX(computed_infos[lane_id].bytesAccessed == size_from_pin);
    ASSERTX(computed_infos[lane_id].maskOn == mask_on_from_pin);
  }
}

// TODO: extract displacement information
vector<PIN_MEM_ACCESS_INFO> compute_mem_access_infos(
  const CONTEXT* ctxt, gather_scatter_info* info) {
  info->verify_fields_for_mem_access_info_generation();

  vector<PIN_MEM_ACCESS_INFO> mem_access_infos;
  ADDRINT base_addr_contribution = compute_base_reg_addr_contribution(ctxt,
                                                                      info);
  PIN_MEMOP_ENUM memop_type      = type_to_PIN_MEMOP_ENUM(info);

  PIN_REGISTER vector_index_reg_val_buf;
  assert(reg_xed_to_pin_map.find(info->get_index_reg()) !=
         reg_xed_to_pin_map.end());
  PIN_GetContextRegval(ctxt, reg_xed_to_pin_map[info->get_index_reg()],
                       (UINT8*)&vector_index_reg_val_buf);
  PIN_REGISTER mask_reg_val_buf;
  assert(reg_xed_to_pin_map.find(info->get_mask_reg()) !=
         reg_xed_to_pin_map.end());
  PIN_GetContextRegval(ctxt, reg_xed_to_pin_map[info->get_mask_reg()],
                       (UINT8*)&mask_reg_val_buf);
  for(UINT32 lane_id = 0; lane_id < info->get_num_mem_ops(); lane_id++) {
    ADDRDELTA index_val = compute_base_index_addr_contribution(
      vector_index_reg_val_buf, lane_id, info);
    ADDRINT final_addr = base_addr_contribution +
                         (index_val * info->get_scale()) +
                         info->get_displacement();
    bool mask_on = extract_mask_on(mask_reg_val_buf, lane_id, info);
    PIN_MEM_ACCESS_INFO access_info = {
      .memoryAddress = final_addr,
      .memopType     = memop_type,
      .bytesAccessed = info->get_data_lane_width_bytes(),
      .maskOn        = mask_on};

    mem_access_infos.push_back(access_info);
  }

  return mem_access_infos;
}

ADDRDELTA compute_base_reg_addr_contribution(const CONTEXT*       ctxt,
                                             gather_scatter_info* info) {
  ADDRDELTA base_addr_contribution = 0;
  if(XED_REG_valid(info->get_base_reg())) {
    PIN_REGISTER buf;
    ASSERTX(reg_xed_to_pin_map.find(info->get_base_reg()) !=
            reg_xed_to_pin_map.end());
    PIN_GetContextRegval(ctxt, reg_xed_to_pin_map[info->get_base_reg()],
                         (UINT8*)&buf);
    // need to do this to make sure a 32-bit base register holding a negative
    // value is properly sign-extended into a 64-bit ADDRDELTA value
    if(XED_REG_is_gr32(info->get_base_reg())) {
      base_addr_contribution = buf.s_dword[0];
    } else if(XED_REG_is_gr64(info->get_base_reg())) {
      base_addr_contribution = buf.s_qword[0];
    } else {
      ASSERTX(false);
    }
  }
  return base_addr_contribution;
}

ADDRDELTA compute_base_index_addr_contribution(
  const PIN_REGISTER& vector_index_reg_val, const UINT32 lane_id,
  gather_scatter_info* info) {
  ADDRDELTA index_val;
  switch(info->get_index_lane_width_bytes()) {
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

PIN_MEMOP_ENUM type_to_PIN_MEMOP_ENUM(gather_scatter_info* info) {
  switch(info->get_type()) {
    case gather_scatter_info::GATHER:
      return PIN_MEMOP_LOAD;
    case gather_scatter_info::SCATTER:
      return PIN_MEMOP_STORE;
    default:
      ASSERTX(false);
      return PIN_MEMOP_LOAD;
  }
}

bool extract_mask_on(const PIN_REGISTER& mask_reg_val_buf, const UINT32 lane_id,
                     gather_scatter_info* info) {
  bool   mask_on  = false;
  UINT64 msb_mask = ((UINT64)1) << (info->get_data_lane_width_bytes() * 8 - 1);

  switch(info->get_mask_reg_type()) {
    case gather_scatter_info::K:
      mask_on = (mask_reg_val_buf.word[0] & (1 << lane_id));
      break;
    case gather_scatter_info::XYMM:
      // Conditionality is specified by the most significant bit of each data
      // element of the mask register. The width of data element in the
      // destination register and mask register are identical.
      switch(info->get_data_lane_width_bytes()) {
        case 4:
          mask_on = (mask_reg_val_buf.dword[lane_id] & msb_mask);
          break;

        case 8:
          mask_on = (mask_reg_val_buf.qword[lane_id] & msb_mask);
          break;

        default:
          ASSERT(false, "expecting data lane width to be 4 or 8 bytes");
          break;
      }
      break;
    default:
      ASSERTX(false);
      break;
  }
  return mask_on;
}

void init_reg_xed_to_pin_map() {
  reg_xed_to_pin_map[XED_REG_INVALID] = REG_INVALID_;
  reg_xed_to_pin_map[XED_REG_RDI]     = REG_RDI;
  reg_xed_to_pin_map[XED_REG_EDI]     = REG_EDI;
  reg_xed_to_pin_map[XED_REG_ESI]     = REG_ESI;
  reg_xed_to_pin_map[XED_REG_RSI]     = REG_RSI;
  reg_xed_to_pin_map[XED_REG_EBP]     = REG_EBP;
  reg_xed_to_pin_map[XED_REG_RBP]     = REG_RBP;
  reg_xed_to_pin_map[XED_REG_ESP]     = REG_ESP;
  reg_xed_to_pin_map[XED_REG_RSP]     = REG_RSP;
  reg_xed_to_pin_map[XED_REG_EBX]     = REG_EBX;
  reg_xed_to_pin_map[XED_REG_RBX]     = REG_RBX;
  reg_xed_to_pin_map[XED_REG_EDX]     = REG_EDX;
  reg_xed_to_pin_map[XED_REG_RDX]     = REG_RDX;
  reg_xed_to_pin_map[XED_REG_ECX]     = REG_ECX;
  reg_xed_to_pin_map[XED_REG_RCX]     = REG_RCX;
  reg_xed_to_pin_map[XED_REG_EAX]     = REG_EAX;
  reg_xed_to_pin_map[XED_REG_RAX]     = REG_RAX;
  reg_xed_to_pin_map[XED_REG_R8]      = REG_R8;
  reg_xed_to_pin_map[XED_REG_R9]      = REG_R9;
  reg_xed_to_pin_map[XED_REG_R10]     = REG_R10;
  reg_xed_to_pin_map[XED_REG_R11]     = REG_R11;
  reg_xed_to_pin_map[XED_REG_R12]     = REG_R12;
  reg_xed_to_pin_map[XED_REG_R13]     = REG_R13;
  reg_xed_to_pin_map[XED_REG_R14]     = REG_R14;
  reg_xed_to_pin_map[XED_REG_R15]     = REG_R15;

  /*
  //  reg_xed_to_pin_map[XED_REG_GR_LAST]   = REG_RAX;
  reg_xed_to_pin_map[XED_REG_FSBASE]  = REG_CS;
  //todo: Not sure about the next
  reg_xed_to_pin_map[XED_REG_GSBASE]  = REG_CS;
  reg_xed_to_pin_map[XED_REG_CS]    = REG_CS;
  reg_xed_to_pin_map[XED_REG_SS]    = REG_SS;
  reg_xed_to_pin_map[XED_REG_DS]    = REG_DS;
  reg_xed_to_pin_map[XED_REG_ES]    = REG_ES;
  reg_xed_to_pin_map[XED_REG_FS]    = REG_FS;
  reg_xed_to_pin_map[XED_REG_GS]    = REG_GS;
  //reg_xed_to_pin_map[XED_REG_SEG_LAST]  = REG_GS;
  // Treating any flag dependency as ZPS because we could not
  // get finer grain dependicies from PIN
  reg_xed_to_pin_map[XED_REG_EFLAGS]   = REG_ZPS;
  reg_xed_to_pin_map[XED_REG_RFLAGS]   = REG_ZPS;
  reg_xed_to_pin_map[XED_REG_EIP]      = REG_RIP;
  reg_xed_to_pin_map[XED_REG_RIP] = REG_RIP;
  reg_xed_to_pin_map[XED_REG_AL]       = REG_RAX;
  reg_xed_to_pin_map[XED_REG_AH]       = REG_RAX;
  reg_xed_to_pin_map[XED_REG_AX]       = REG_RAX;
  reg_xed_to_pin_map[XED_REG_CL]       = REG_RCX;
  reg_xed_to_pin_map[XED_REG_CH]       = REG_RCX;
  reg_xed_to_pin_map[XED_REG_CX]       = REG_RCX;
  reg_xed_to_pin_map[XED_REG_DL]       = REG_RDX;
  reg_xed_to_pin_map[XED_REG_DH]       = REG_RDX;
  reg_xed_to_pin_map[XED_REG_DX]       = REG_RDX;
  reg_xed_to_pin_map[XED_REG_BL]       = REG_RBX;
  reg_xed_to_pin_map[XED_REG_BH]       = REG_RBX;
  reg_xed_to_pin_map[XED_REG_BX]       = REG_RBX;
  reg_xed_to_pin_map[XED_REG_BP]       = REG_RBX;
  reg_xed_to_pin_map[XED_REG_SI]       = REG_RSI;
  reg_xed_to_pin_map[XED_REG_DI]       = REG_RDI;
  reg_xed_to_pin_map[XED_REG_SP]       = REG_RSP;
  reg_xed_to_pin_map[XED_REG_FLAGS]    = REG_ZPS;
  reg_xed_to_pin_map[XED_REG_IP]       = REG_RIP;
  reg_xed_to_pin_map[XED_REG_MMX_FIRST]  = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_MMX0]      = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_MMX1]      = REG_ZMM1;
  reg_xed_to_pin_map[XED_REG_MMX2]      = REG_ZMM2;
  reg_xed_to_pin_map[XED_REG_MMX3]      = REG_ZMM3;
  reg_xed_to_pin_map[XED_REG_MMX4]      = REG_ZMM4;
  reg_xed_to_pin_map[XED_REG_MMX5]      = REG_ZMM5;
  reg_xed_to_pin_map[XED_REG_MMX6]      = REG_ZMM6;
  reg_xed_to_pin_map[XED_REG_MMX7]      = REG_ZMM7;
  reg_xed_to_pin_map[XED_REG_MMX_LAST]  = REG_ZMM7;

  reg_xed_to_pin_map[XED_REG_XMM_FIRST]       = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_XMM0]            = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_XMM1]            = REG_ZMM1;
  reg_xed_to_pin_map[XED_REG_XMM2]            = REG_ZMM2;
  reg_xed_to_pin_map[XED_REG_XMM3]            = REG_ZMM3;
  reg_xed_to_pin_map[XED_REG_XMM4]            = REG_ZMM4;
  reg_xed_to_pin_map[XED_REG_XMM5]            = REG_ZMM5;
  reg_xed_to_pin_map[XED_REG_XMM6]            = REG_ZMM6;
  reg_xed_to_pin_map[XED_REG_XMM7]            = REG_ZMM7;
  //reg_xed_to_pin_map[XED_REG_XMM_SSE_LAST]    = REG_ZMM7;
  //reg_xed_to_pin_map[XED_REG_XMM_AVX_LAST]    = REG_ZMM7;
  //reg_xed_to_pin_map[XED_REG_XMM_AVX512_LAST] = REG_ZMM7;
  reg_xed_to_pin_map[XED_REG_XMM_LAST]        = REG_ZMM7;

  reg_xed_to_pin_map[XED_REG_YMM_FIRST]        = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_YMM0]            = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_YMM1]            = REG_ZMM1;
  reg_xed_to_pin_map[XED_REG_YMM2]            = REG_ZMM2;
  reg_xed_to_pin_map[XED_REG_YMM3]            = REG_ZMM3;
  reg_xed_to_pin_map[XED_REG_YMM4]            = REG_ZMM4;
  reg_xed_to_pin_map[XED_REG_YMM5]            = REG_ZMM5;
  reg_xed_to_pin_map[XED_REG_YMM6]            = REG_ZMM6;
  reg_xed_to_pin_map[XED_REG_YMM7]            = REG_ZMM7;
  //reg_xed_to_pin_map[XED_REG_YMM_AVX_LAST]    = REG_ZMM7;
  //reg_xed_to_pin_map[XED_REG_YMM_AVX512_LAST] = REG_ZMM7;
  reg_xed_to_pin_map[XED_REG_YMM_LAST]        = REG_ZMM7;

  reg_xed_to_pin_map[XED_REG_ZMM_FIRST]              = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_ZMM0]                  = REG_ZMM0;
  reg_xed_to_pin_map[XED_REG_ZMM1]                  = REG_ZMM1;
  reg_xed_to_pin_map[XED_REG_ZMM2]                  = REG_ZMM2;
  reg_xed_to_pin_map[XED_REG_ZMM3]                  = REG_ZMM3;
  reg_xed_to_pin_map[XED_REG_ZMM4]                  = REG_ZMM4;
  reg_xed_to_pin_map[XED_REG_ZMM5]                  = REG_ZMM5;
  reg_xed_to_pin_map[XED_REG_ZMM6]                  = REG_ZMM6;
  reg_xed_to_pin_map[XED_REG_ZMM7]                  = REG_ZMM7;
  //reg_xed_to_pin_map[XED_REG_ZMM_AVX512_SPLIT_LAST] = REG_ZMM7;
  //reg_xed_to_pin_map[XED_REG_ZMM_AVX512_LAST]       = REG_ZMM7;
  reg_xed_to_pin_map[XED_REG_ZMM_LAST]              = REG_ZMM7;

  //reg_xed_to_pin_map[XED_REG_MASK_FIRST]             = REG_K0;
  //reg_xed_to_pin_map[XED_REG_IMPLICIT_FULL_MASK] = REG_K0;
  reg_xed_to_pin_map[XED_REG_K0]                 = REG_K0;
  reg_xed_to_pin_map[XED_REG_K1]                 = REG_K1;
  reg_xed_to_pin_map[XED_REG_K2]                 = REG_K2;
  reg_xed_to_pin_map[XED_REG_K3]                 = REG_K3;
  reg_xed_to_pin_map[XED_REG_K4]                 = REG_K4;
  reg_xed_to_pin_map[XED_REG_K5]                 = REG_K5;
  reg_xed_to_pin_map[XED_REG_K6]                 = REG_K6;
  reg_xed_to_pin_map[XED_REG_K7]                 = REG_K7;
  //reg_xed_to_pin_map[XED_REG_MASK_LAST]             = REG_K7;

  reg_xed_to_pin_map[XED_REG_MXCSR]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_MXCSRMASK] = REG_OTHER;

  reg_xed_to_pin_map[XED_REG_X87CONTROL] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87STATUS] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87TAG] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87PUSH] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87POP] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87POP2] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87OPCODE] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87LASTCS] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87LASTIP] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87LASTDS] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_X87LASTDP] = REG_OTHER;

  //reg_xed_to_pin_map[XED_REG_FPST_BASE]     = REG_FPCW;
  //reg_xed_to_pin_map[XED_REG_FPSTATUS_BASE] = REG_FPCW;
  //reg_xed_to_pin_map[XED_REG_FPCW]          = REG_FPCW;
  //reg_xed_to_pin_map[XED_REG_FPSW]          = REG_FPST;
  //reg_xed_to_pin_map[XED_REG_FPTAG]         = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_FPIP_OFF]      = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_FPIP_SEL]      = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_FPOPCODE]      = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_FPDP_OFF]      = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_FPDP_SEL]      = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_FPSTATUS_LAST] = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_FPTAG_FULL]    = REG_OTHER;

  //  reg_xed_to_pin_map[XED_REG_ST_BASE]   = REG_FP0;
  reg_xed_to_pin_map[XED_REG_ST0]       = REG_FP0;
  reg_xed_to_pin_map[XED_REG_ST1]       = REG_FP1;
  reg_xed_to_pin_map[XED_REG_ST2]       = REG_FP2;
  reg_xed_to_pin_map[XED_REG_ST3]       = REG_FP3;
  reg_xed_to_pin_map[XED_REG_ST4]       = REG_FP4;
  reg_xed_to_pin_map[XED_REG_ST5]       = REG_FP5;
  reg_xed_to_pin_map[XED_REG_ST6]       = REG_FP6;
  reg_xed_to_pin_map[XED_REG_ST7]       = REG_FP7;
  //reg_xed_to_pin_map[XED_REG_ST_LAST]   = REG_FP7;
  //reg_xed_to_pin_map[XED_REG_FPST_LAST] = REG_FP7;

  //reg_xed_to_pin_map[XED_REG_DR_BASE] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR0]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR1]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR2]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR3]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR4]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR5]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR6]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_DR7]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_DR_LAST] = REG_OTHER;

  //reg_xed_to_pin_map[XED_REG_CR_BASE] = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_TSC]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_TSCAUX]  = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_XCR0]    = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR0]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR1]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR2]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR3]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR4]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR5]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR6]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR7]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR8]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR9]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR10]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR11]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR12]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR13]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR14]     = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_CR15]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_CR_LAST] = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TSSR]    = REG_OTHER;
  reg_xed_to_pin_map[XED_REG_LDTR]    = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR_BASE] = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR]      = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR3]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR4]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR5]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR6]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR7]     = REG_OTHER;
  //reg_xed_to_pin_map[XED_REG_TR_LAST] = REG_OTHER;

  reg_xed_to_pin_map[XED_REG_STACKPUSH] = REG_RSP;
  reg_xed_to_pin_map[XED_REG_STACKPOP] = REG_RSP;
  reg_xed_to_pin_map[XED_REG_RDI]     = REG_RDI;
  reg_xed_to_pin_map[XED_REG_RSI]     = REG_RSI;
  reg_xed_to_pin_map[XED_REG_RBP]     = REG_RBP;
  reg_xed_to_pin_map[XED_REG_RSP]     = REG_RSP;
  reg_xed_to_pin_map[XED_REG_RBX]     = REG_RBX;
  reg_xed_to_pin_map[XED_REG_RDX]     = REG_RDX;
  reg_xed_to_pin_map[XED_REG_RCX]     = REG_RCX;
  reg_xed_to_pin_map[XED_REG_RAX]     = REG_RAX;
  reg_xed_to_pin_map[XED_REG_R8]      = REG_R8;
  reg_xed_to_pin_map[XED_REG_R9]      = REG_R9;
  reg_xed_to_pin_map[XED_REG_R10]     = REG_R10;
  reg_xed_to_pin_map[XED_REG_R11]     = REG_R11;
  reg_xed_to_pin_map[XED_REG_R12]     = REG_R12;
  reg_xed_to_pin_map[XED_REG_R13]     = REG_R13;
  reg_xed_to_pin_map[XED_REG_R14]     = REG_R14;
  reg_xed_to_pin_map[XED_REG_R15]     = REG_R15;
  //reg_xed_to_pin_map[XED_REG_GR_LAST] = REG_R15;
  reg_xed_to_pin_map[XED_REG_RFLAGS]  = REG_ZPS;
  reg_xed_to_pin_map[XED_REG_RIP]     = REG_RIP;

  reg_xed_to_pin_map[XED_REG_DIL]  = REG_RDI;
  reg_xed_to_pin_map[XED_REG_SIL]  = REG_RSI;
  reg_xed_to_pin_map[XED_REG_BPL]  = REG_RBP;
  reg_xed_to_pin_map[XED_REG_SPL]  = REG_RSP;
  reg_xed_to_pin_map[XED_REG_R8B]  = REG_R8;
  reg_xed_to_pin_map[XED_REG_R8W]  = REG_R8;
  reg_xed_to_pin_map[XED_REG_R8D]  = REG_R8;
  reg_xed_to_pin_map[XED_REG_R9B]  = REG_R9;
  reg_xed_to_pin_map[XED_REG_R9W]  = REG_R9;
  reg_xed_to_pin_map[XED_REG_R9D]  = REG_R9;
  reg_xed_to_pin_map[XED_REG_R10B] = REG_R10;
  reg_xed_to_pin_map[XED_REG_R10W] = REG_R10;
  reg_xed_to_pin_map[XED_REG_R10D] = REG_R10;
  reg_xed_to_pin_map[XED_REG_R11B] = REG_R11;
  reg_xed_to_pin_map[XED_REG_R11W] = REG_R11;
  reg_xed_to_pin_map[XED_REG_R11D] = REG_R11;
  reg_xed_to_pin_map[XED_REG_R12B] = REG_R12;
  reg_xed_to_pin_map[XED_REG_R12W] = REG_R12;
  reg_xed_to_pin_map[XED_REG_R12D] = REG_R12;
  reg_xed_to_pin_map[XED_REG_R13B] = REG_R13;
  reg_xed_to_pin_map[XED_REG_R13W] = REG_R13;
  reg_xed_to_pin_map[XED_REG_R13D] = REG_R13;
  reg_xed_to_pin_map[XED_REG_R14B] = REG_R14;
  reg_xed_to_pin_map[XED_REG_R14W] = REG_R14;
  reg_xed_to_pin_map[XED_REG_R14D] = REG_R14;
  reg_xed_to_pin_map[XED_REG_R15B] = REG_R15;
  reg_xed_to_pin_map[XED_REG_R15W] = REG_R15;
  reg_xed_to_pin_map[XED_REG_R15D] = REG_R15;

  reg_xed_to_pin_map[XED_REG_XMM8]                  = REG_ZMM8;
  reg_xed_to_pin_map[XED_REG_XMM9]                  = REG_ZMM9;
  reg_xed_to_pin_map[XED_REG_XMM10]                 = REG_ZMM10;
  reg_xed_to_pin_map[XED_REG_XMM11]                 = REG_ZMM11;
  reg_xed_to_pin_map[XED_REG_XMM12]                 = REG_ZMM12;
  reg_xed_to_pin_map[XED_REG_XMM13]                 = REG_ZMM13;
  reg_xed_to_pin_map[XED_REG_XMM14]                 = REG_ZMM14;
  reg_xed_to_pin_map[XED_REG_XMM15]                 = REG_ZMM15;
  //reg_xed_to_pin_map[XED_REG_XMM_SSE_LAST]          = REG_ZMM15;
  //reg_xed_to_pin_map[XED_REG_XMM_AVX_LAST]          = REG_ZMM15;
  reg_xed_to_pin_map[XED_REG_XMM16]                 = REG_ZMM16;
  //reg_xed_to_pin_map[XED_REG_XMM_AVX512_HI16_FIRST] = REG_ZMM16;
  reg_xed_to_pin_map[XED_REG_XMM17]                 = REG_ZMM17;
  reg_xed_to_pin_map[XED_REG_XMM18]                 = REG_ZMM18;
  reg_xed_to_pin_map[XED_REG_XMM19]                 = REG_ZMM19;
  reg_xed_to_pin_map[XED_REG_XMM20]                 = REG_ZMM20;
  reg_xed_to_pin_map[XED_REG_XMM21]                 = REG_ZMM21;
  reg_xed_to_pin_map[XED_REG_XMM22]                 = REG_ZMM22;
  reg_xed_to_pin_map[XED_REG_XMM23]                 = REG_ZMM23;
  reg_xed_to_pin_map[XED_REG_XMM24]                 = REG_ZMM24;
  reg_xed_to_pin_map[XED_REG_XMM25]                 = REG_ZMM25;
  reg_xed_to_pin_map[XED_REG_XMM26]                 = REG_ZMM26;
  reg_xed_to_pin_map[XED_REG_XMM27]                 = REG_ZMM27;
  reg_xed_to_pin_map[XED_REG_XMM28]                 = REG_ZMM28;
  reg_xed_to_pin_map[XED_REG_XMM29]                 = REG_ZMM29;
  reg_xed_to_pin_map[XED_REG_XMM30]                 = REG_ZMM30;
  reg_xed_to_pin_map[XED_REG_XMM31]                 = REG_ZMM31;
  //reg_xed_to_pin_map[XED_REG_XMM_AVX512_HI16_LAST]  = REG_ZMM31;
  //reg_xed_to_pin_map[XED_REG_XMM_AVX512_LAST]       = REG_ZMM31;
  reg_xed_to_pin_map[XED_REG_XMM_LAST]              = REG_ZMM31;

  reg_xed_to_pin_map[XED_REG_YMM8]                  = REG_ZMM8;
  reg_xed_to_pin_map[XED_REG_YMM9]                  = REG_ZMM9;
  reg_xed_to_pin_map[XED_REG_YMM10]                 = REG_ZMM10;
  reg_xed_to_pin_map[XED_REG_YMM11]                 = REG_ZMM11;
  reg_xed_to_pin_map[XED_REG_YMM12]                 = REG_ZMM12;
  reg_xed_to_pin_map[XED_REG_YMM13]                 = REG_ZMM13;
  reg_xed_to_pin_map[XED_REG_YMM14]                 = REG_ZMM14;
  reg_xed_to_pin_map[XED_REG_YMM15]                 = REG_ZMM15;
  //reg_xed_to_pin_map[XED_REG_YMM_AVX_LAST]          = REG_ZMM15;
  reg_xed_to_pin_map[XED_REG_YMM16]                 = REG_ZMM16;
  //reg_xed_to_pin_map[XED_REG_YMM_AVX512_HI16_FIRST] = REG_ZMM16;
  reg_xed_to_pin_map[XED_REG_YMM17]                 = REG_ZMM17;
  reg_xed_to_pin_map[XED_REG_YMM18]                 = REG_ZMM18;
  reg_xed_to_pin_map[XED_REG_YMM19]                 = REG_ZMM19;
  reg_xed_to_pin_map[XED_REG_YMM20]                 = REG_ZMM20;
  reg_xed_to_pin_map[XED_REG_YMM21]                 = REG_ZMM21;
  reg_xed_to_pin_map[XED_REG_YMM22]                 = REG_ZMM22;
  reg_xed_to_pin_map[XED_REG_YMM23]                 = REG_ZMM23;
  reg_xed_to_pin_map[XED_REG_YMM24]                 = REG_ZMM24;
  reg_xed_to_pin_map[XED_REG_YMM25]                 = REG_ZMM25;
  reg_xed_to_pin_map[XED_REG_YMM26]                 = REG_ZMM26;
  reg_xed_to_pin_map[XED_REG_YMM27]                 = REG_ZMM27;
  reg_xed_to_pin_map[XED_REG_YMM28]                 = REG_ZMM28;
  reg_xed_to_pin_map[XED_REG_YMM29]                 = REG_ZMM29;
  reg_xed_to_pin_map[XED_REG_YMM30]                 = REG_ZMM30;
  reg_xed_to_pin_map[XED_REG_YMM31]                 = REG_ZMM31;
  //reg_xed_to_pin_map[XED_REG_YMM_AVX512_HI16_LAST]  = REG_ZMM31;
  //reg_xed_to_pin_map[XED_REG_YMM_AVX512_LAST]       = REG_ZMM31;
  reg_xed_to_pin_map[XED_REG_YMM_LAST]              = REG_ZMM31;

  reg_xed_to_pin_map[XED_REG_ZMM8]                  = REG_ZMM8;
  reg_xed_to_pin_map[XED_REG_ZMM9]                  = REG_ZMM9;
  reg_xed_to_pin_map[XED_REG_ZMM10]                 = REG_ZMM10;
  reg_xed_to_pin_map[XED_REG_ZMM11]                 = REG_ZMM11;
  reg_xed_to_pin_map[XED_REG_ZMM12]                 = REG_ZMM12;
  reg_xed_to_pin_map[XED_REG_ZMM13]                 = REG_ZMM13;
  reg_xed_to_pin_map[XED_REG_ZMM14]                 = REG_ZMM14;
  reg_xed_to_pin_map[XED_REG_ZMM15]                 = REG_ZMM15;
  //reg_xed_to_pin_map[XED_REG_ZMM_AVX512_SPLIT_LAST] = REG_ZMM15;
  reg_xed_to_pin_map[XED_REG_ZMM16]                 = REG_ZMM16;
  //reg_xed_to_pin_map[XED_REG_ZMM_AVX512_HI16_FIRST] = REG_ZMM16;
  reg_xed_to_pin_map[XED_REG_ZMM17]                 = REG_ZMM17;
  reg_xed_to_pin_map[XED_REG_ZMM18]                 = REG_ZMM18;
  reg_xed_to_pin_map[XED_REG_ZMM19]                 = REG_ZMM19;
  reg_xed_to_pin_map[XED_REG_ZMM20]                 = REG_ZMM20;
  reg_xed_to_pin_map[XED_REG_ZMM21]                 = REG_ZMM21;
  reg_xed_to_pin_map[XED_REG_ZMM22]                 = REG_ZMM22;
  reg_xed_to_pin_map[XED_REG_ZMM23]                 = REG_ZMM23;
  reg_xed_to_pin_map[XED_REG_ZMM24]                 = REG_ZMM24;
  reg_xed_to_pin_map[XED_REG_ZMM25]                 = REG_ZMM25;
  reg_xed_to_pin_map[XED_REG_ZMM26]                 = REG_ZMM26;
  reg_xed_to_pin_map[XED_REG_ZMM27]                 = REG_ZMM27;
  reg_xed_to_pin_map[XED_REG_ZMM28]                 = REG_ZMM28;
  reg_xed_to_pin_map[XED_REG_ZMM29]                 = REG_ZMM29;
  reg_xed_to_pin_map[XED_REG_ZMM30]                 = REG_ZMM30;
  reg_xed_to_pin_map[XED_REG_ZMM31]                 = REG_ZMM31;
  //reg_xed_to_pin_map[XED_REG_ZMM_AVX512_HI16_LAST]  = REG_ZMM31;
  //reg_xed_to_pin_map[XED_REG_ZMM_AVX512_LAST]       = REG_ZMM31;
  reg_xed_to_pin_map[XED_REG_ZMM_LAST]              = REG_ZMM31;
  */
};
