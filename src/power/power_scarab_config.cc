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

/***************************************************************************************
 * File         : power_register_unit.cc
 * Author       : HPS Research Group
 * Date         : 02/07/2019
 * Description  : Print the XML file that is input into McPat
 ***************************************************************************************/

#include <assert.h>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <stdio.h>
#include <string>

#include <cmath>
#include "power/power_scarab_config.h"

extern "C" {
#include "bp/bp.param.h"
#include "core.param.h"
#include "general.param.h"
#include "globals/assert.h"
#include "memory/memory.param.h"
#include "power/power.param.h"
#include "ramulator.h"
#include "ramulator.param.h"
#include "statistics.h"
}

#define XML_PARAM_NAME_WIDTH 50
#define XML_PARAM_VALUE_WIDTH 25
#define ADD_XML_COMPONENT(stream, header, component_id, component_name,  \
                          comment)                                       \
  stream << header << "<component id=\"" << component_id << "\" name=\"" \
         << component_name << "\">\t<!-- " #comment " -->" << std::endl;

#define ADD_XML_PARAM(stream, header, param_name, param_value, comment)      \
  stream << header << std::left << std::setw(XML_PARAM_NAME_WIDTH)           \
         << std::string("\t<param name=\"") + param_name + "\"" << std::left \
         << std::setw(XML_PARAM_VALUE_WIDTH)                                 \
         << std::string("value=\"") + std::to_string(param_value) + "\"/>"   \
         << " <!-- " #comment " -->" << std::endl
#define ADD_XML_PARAM_str(stream, header, param_name, param_value, comment)  \
  stream << header << std::left << std::setw(XML_PARAM_NAME_WIDTH)           \
         << std::string("\t<param name=\"") + param_name + "\"" << std::left \
         << std::setw(XML_PARAM_VALUE_WIDTH)                                 \
         << std::string("value=\"") + param_value + "\"/>"                   \
         << " <!-- " #comment " -->" << std::endl

#define ADD_XML_STAT(stream, header, stat_name, stat_value, comment)       \
  stream << header << std::left << std::setw(XML_PARAM_NAME_WIDTH)         \
         << std::string("\t<stat name=\"") + stat_name + "\"" << std::left \
         << std::setw(XML_PARAM_VALUE_WIDTH)                               \
         << std::string("value=\"") + std::to_string(stat_value) + "\"/>"  \
         << " <!-- " #comment " -->" << std::endl
#define ADD_XML_CORE_STAT(stream, header, core, stat_name, stat_value,        \
                          comment)                                            \
  stream << header << std::left << std::setw(XML_PARAM_NAME_WIDTH)            \
         << std::string("\t<stat name=\"") + stat_name + "\"" << std::left    \
         << std::setw(XML_PARAM_VALUE_WIDTH)                                  \
         << std::string("value=\"") +                                         \
              std::to_string(GET_TOTAL_STAT_EVENT(core, stat_value)) + "\"/>" \
         << " <!-- " #comment " -->" << std::endl
#define ADD_XML_ACCUM_STAT(stream, header, stat_name, stat_value, comment) \
  stream << header << std::left << std::setw(XML_PARAM_NAME_WIDTH)         \
         << std::string("\t<stat name=\"") + stat_name + "\"" << std::left \
         << std::setw(XML_PARAM_VALUE_WIDTH)                               \
         << std::string("value=\"") +                                      \
              std::to_string(GET_ACCUM_STAT_EVENT(stat_value)) + "\"/>"    \
         << " <!-- " #comment " -->" << std::endl

#define END_OF_COMPONENT(stream, header) \
  stream << header << "</component>" << std::endl

#define ADD_CACTI_PARAM(stream, param, value, comment)                     \
  stream << std::left << std::setw(XML_PARAM_NAME_WIDTH)                   \
         << std::string("-") + param << std::left                          \
         << std::setw(XML_PARAM_VALUE_WIDTH)                               \
         << std::string("\t") + std::to_string(value) << "\t#" << #comment \
         << std::endl
#define ADD_CACTI_PARAM_str(stream, param, value, comment)                 \
  stream << std::left << std::setw(XML_PARAM_NAME_WIDTH)                   \
         << std::string("-") + param << std::left                          \
         << std::setw(XML_PARAM_VALUE_WIDTH) << std::string("\t") + #value \
         << "\t#" << #comment << std::endl
#define ADD_CACTI_PARAM_str2(stream, param, value, comment)               \
  stream << std::left << std::setw(XML_PARAM_NAME_WIDTH)                  \
         << std::string("-") + param << std::left                         \
         << std::setw(XML_PARAM_VALUE_WIDTH) << std::string("\t") + value \
         << "\t#" << #comment << std::endl

const uint32_t machine_bits            = 64;
const uint32_t virtual_address_length  = 64;
const uint32_t physical_address_length = 52;
const double   time_unit               = 1e-15;
double         CHIP_FREQ_IN_MHZ        = 0.0;
uint32_t       num_l2_caches;
uint32_t       num_l3_caches;

/***********************************************************************************************************
 * Known Issues
 ***********************************************************************************************************
 * 1) Power model does not consider SIMD.
 */


/***********************************************************************************************************
 * Local Prototypes
 ***********************************************************************************************************/

void power_print_system_params(std::ofstream& out);
void power_print_core_params(std::ofstream& out, uint32_t core_id);
void power_print_cache_directory_params(std::ofstream& out,
                                        uint32_t       num_l1_directories,
                                        uint32_t       num_l2_directories);
void power_print_l2_params(std::ofstream& out, uint32_t l2_id);
void power_print_l3_params(std::ofstream& out);
void power_print_mc_params(std::ofstream& out);
void power_print_noc_params(std::ofstream& out);
void power_print_io_params(std::ofstream& out);

/***********************************************************************************************************
 * Local Functions
 ***********************************************************************************************************/

void power_print_system_params(std::ofstream& out) {
  std::string header = "";

  ADD_XML_PARAM(out, header, "number_of_cores", NUM_CORES, );
  ADD_XML_PARAM(out, header, "number_of_L1Directories", 0, );
  ADD_XML_PARAM(out, header, "number_of_L2Directories", 0, );

  /* Scarab: Scarab either has a private last level cache (LLC, refered to as L1
   * in the scarab src code) or a shared LLC. McPAT requires us to model this as
   * either several private "L2" caches or a single shared "L3" cache*/
  ADD_XML_PARAM(out, header, "number_of_L2s", num_l2_caches,
                "This number means how many L2 clusters in each cluster there "
                "can be multiple banks/ports");
  ADD_XML_PARAM(
    out, header, "Private_L2", PRIVATE_L1,
    "1 Private, 0 shared/coherent");  // TODO: can the Scarab:L1
                                      // always be set to McPat:L2 by
                                      // setting this bit?
  ADD_XML_PARAM(out, header, "number_of_L3s", num_l3_caches,
                "This number means how many L3 clusters");

  ADD_XML_PARAM(out, header, "number_of_NoCs", 0, );
  ADD_XML_PARAM(out, header, "homogeneous_cores", 0, "1 means homo");
  ADD_XML_PARAM(out, header, "homogeneous_L2s", 0, );
  ADD_XML_PARAM(out, header, "homogeneous_L1Directorys", 1, );
  ADD_XML_PARAM(out, header, "homogeneous_L2Directorys", 1, );
  ADD_XML_PARAM(out, header, "homogeneous_L3s",
                num_l3_caches, );  // num_l3_caches is 0 or 1
  ADD_XML_PARAM(out, header, "homogeneous_ccs", 1, "cache coherece hardware");
  ADD_XML_PARAM(out, header, "homogeneous_NoCs", 1, );
  ADD_XML_PARAM(out, header, "core_tech_node", POWER_INTF_REF_CHIP_TECH_NM,
                "nm");  // 22nm default
  ADD_XML_PARAM(out, header, "target_core_clockrate", CHIP_FREQ_IN_MHZ, "MHz");
  ADD_XML_PARAM(out, header, "temperature", 380, "Kelvin");
  ADD_XML_PARAM(out, header, "number_cache_levels",
                2, );  // TODO: does not support MLC

  ADD_XML_PARAM(
    out, header, "interconnect_projection_type", 0,
    "0: agressive wire technology; 1: conservative wire technology");
  ADD_XML_PARAM(out, header, "device_type", 0,
                "0: HP(High Performance Type); 1: LSTP(Low standby power) 2: "
                "LOP (Low Operating Power)");
  ADD_XML_PARAM(out, header, "longer_channel_device", 0,
                "0 no use; 1 use when approperiate");
  ADD_XML_PARAM(out, header, "machine_bits", machine_bits, );
  ADD_XML_PARAM(out, header, "virtual_address_width", virtual_address_length, );
  ADD_XML_PARAM(out, header, "physical_address_width", physical_address_length,
                "address width determins the tag_width in Cache, LSQ and "
                "buffers in cache controller default value is machine_bits, if "
                "not set");
  ADD_XML_PARAM(out, header, "virtual_memory_page_size", VA_PAGE_SIZE_BYTES,
                "This page size(B) is complete different from the page size in "
                "Main memo secction. this page size is the size of virtual "
                "memory from OS/Archi perspective; the page size in Main memo "
                "secction is the actuall physical line in a DRAM bank");

  /* idle_cycles and busy_cycles are only parsed by McPat and are not used for
   * any computation */
  ADD_XML_CORE_STAT(out, header, 0, "total_cycles", POWER_CYCLE, );
  ADD_XML_STAT(out, header, "idle_cycles", 0, "Scarab: McPAT ignores this");
  ADD_XML_CORE_STAT(out, header, 0, "busy_cycles", POWER_CYCLE,
                    "Scarab: McPAT ignores this");
}

void power_print_core_params(std::ofstream& out, uint32_t core_id) {
  uns PIPELINE_DEPTH = DECODE_CYCLES + MAP_CYCLES + 1 + 1 + 1 +
                       1;  // icache, node, exec, retire

  uns32 peak_ops_per_cycle = std::min(
    std::min(NUM_FUS, ISSUE_WIDTH),
    std::min(RS_FILL_WIDTH == 0 ? MAX_INT : RS_FILL_WIDTH,
             NODE_RET_WIDTH)); /*Scarab: theoretical peak ops per cycle*/
  DEBUG(core_id, "peak_ops_per_cycle: %d\n", peak_ops_per_cycle);

  double ops_per_cycle = GET_TOTAL_STAT_EVENT(core_id, POWER_CYCLE) ?
                           ((double)GET_TOTAL_STAT_EVENT(core_id, POWER_OP)) /
                             GET_TOTAL_STAT_EVENT(core_id, POWER_CYCLE) :
                           0.0; /*Actual ops per cycle*/
  DEBUG(core_id, "ops_per_cycle: %f\n", ops_per_cycle);

  double OPC_TO_PEAK_OPC_RATIO = ops_per_cycle / peak_ops_per_cycle;
  DEBUG(core_id, "OPC_TO_PEAK_OPC_RATIO: %f\n", OPC_TO_PEAK_OPC_RATIO);
  ASSERTM(core_id, OPC_TO_PEAK_OPC_RATIO <= 1,
          "OPC_TO_PEAK_OPC_RATIO should be less than one\n");

  std::string header = "\t";

  ADD_XML_COMPONENT(out, header, "system.core" + std::to_string(core_id),
                    "core" + std::to_string(core_id), );

  /*******************************************************************************************************************
   * Core Params
   *******************************************************************************************************************/

  ADD_XML_PARAM(out, header, "clock_rate", CHIP_FREQ_IN_MHZ, );
  ADD_XML_PARAM(
    out, header, "opt_local", 0,
    "for cores with unknow timing, set to 0 to force off the opt flag");
  ADD_XML_PARAM(out, header, "instruction_length", 32, );
  ADD_XML_PARAM(out, header, "opcode_width", 16, );
  ADD_XML_PARAM(out, header, "x86", 1, );
  ADD_XML_PARAM(out, header, "micro_opcode_width", 8, );
  ADD_XML_PARAM(out, header, "machine_type", 0,
                "inorder/OoO; 1 inorder; 0 OOO");
  ADD_XML_PARAM(out, header, "number_hardware_threads", 1, );

  /* BTB ports always equals to fetch ports since branch information in
   * consective branch instructions in the same fetch group can be read out from
   * BTB once.*/
  ADD_XML_PARAM(
    out, header, "fetch_width", ISSUE_WIDTH,
    "fetch_width determins the size of cachelines of L1 cache block");
  ADD_XML_PARAM(out, header, "number_instruction_fetch_ports", 1,
                "number_instruction_fetch_ports(icache ports) is always 1 in "
                "single-thread processor, it only may be more than one in SMT "
                "processors.");
  ADD_XML_PARAM(out, header, "decode_width", ISSUE_WIDTH,
                "decode_width determins the number of ports of the renaming "
                "table (both RAM and CAM) scheme");

  /* Instructions are "dispatched" from rename to reservation stations, and
   * "Issued" from reservation stations to functional units*/
  /* Note: this seems to be inaccurate, issue_width should not be the same as
   * dispatch width. issue_width=NUM_FUS and dispatch_width=RS_FILL_WIDTH*/
  /* Note: McPat uses issue_width to compute the pipeline (FP and INT) register
   * storage, while it uses peak_issue_width to compute number of register file
   * and instruction window ports.*/
  ADD_XML_PARAM(out, header, "issue_width", NUM_FUS,
                "issue_width determins the number of ports of Issue window and "
                "other logic as in the complexity effective proccessors paper; "
                "issue_width==dispatch_width");
  ADD_XML_PARAM(
    out, header, "peak_issue_width", NUM_FUS,
    "peak_issue_width is used to determine the number of "
    "read/write ports of the instruction window and the register file");

  ADD_XML_PARAM(out, header, "commit_width", NODE_RET_WIDTH,
                "commit_width determins the number of ports of register files");
  ADD_XML_PARAM(out, header, "fp_issue_width", POWER_NUM_FPUS, );
  ADD_XML_PARAM(
    out, header, "prediction_width", CFS_PER_CYCLE,
    "number of branch instructions can be predicted simultanouesly");

  /* Current version of McPAT does not distinguish int and floating point
   * pipelines. Theses parameters are reserved for future use.*/
  /* Note: what does it mean to share the pipeline? McPat does not seem to have
   * a special case for floating_pipelines=0. It is unclear how setting this to
   * zero would cause any kind of sharing (whatever sharing means). */
  ADD_XML_PARAM_str(
    out, header, "pipelines_per_core", std::string("1,1"),
    "integer_pipeline and floating_pipelines, if the floating_pipelines is 0, "
    "then the pipeline is shared");
  ADD_XML_PARAM_str(
    out, header, "pipeline_depth",
    std::to_string(PIPELINE_DEPTH) + "," + std::to_string(PIPELINE_DEPTH),
    "pipeline depth of int and fp, if pipeline is shared, the "
    "second number is the average cycles of fp ops issue and "
    "exe unit"); /*Scarab: FP pipeline depth is not used*/

  ADD_XML_PARAM(out, header, "ALU_per_core", POWER_NUM_ALUS,
                "contains an adder, a shifter, and a logical unit");
  ADD_XML_PARAM(out, header, "MUL_per_core", POWER_NUM_MULS_AND_DIVS,
                "For MUL and Div");
  ADD_XML_PARAM(out, header, "FPU_per_core", POWER_NUM_FPUS, );

  /* Note: what is instruction_buffer_size? based on the comment and how McPat
   * uses this parameter, instruction_buffer_size is the size of an instruction
   * buffer between fetch and decode stage (per hardware thread). McPat
   * multiplies this value with the issue_width, which makes size of this
   * buffer to be in packets rather than instructions.
   * With this setting, McPat will place a "32 * (instruction_length *
   * issue_width)" buffer between the instruction fetch and decode stages.*/
  ADD_XML_PARAM(out, header, "instruction_buffer_size", 32,
                "buffer between IF and ID stage");

  /* Note: this value is set to 16 by default, including all McPat pre-defined
   * descriptor files. However, McPat does not use it.*/
  ADD_XML_PARAM(out, header, "decoded_stream_buffer_size", 16,
                "buffer between ID and sche/exe stage");

  ADD_XML_PARAM(out, header, "instruction_window_scheme", 0,
                "0 PHYREG based, 1 RSBASED. McPAT support 2 types of OoO "
                "cores, RS based and physical reg based.");

  // FIXME: based on my current understanding, McPat considers both anyway. So,
  // if we use a unified RS for both int and fp, then with current setting,
  // McPat is going to consider the power twice
  ADD_XML_PARAM(out, header, "instruction_window_size",
                std::min(POWER_TOTAL_INT_RS_SIZE, NODE_TABLE_SIZE),
                "Instruction window is limited by the size of the RS and ROB");
  ADD_XML_PARAM(out, header, "fp_instruction_window_size",
                std::min(POWER_TOTAL_FP_RS_SIZE, NODE_TABLE_SIZE),
                "Instruction window is limited by the size of the RS and ROB");

  /*the instruction issue Q as in Alpha 21264; The RS as in Intel P6*/
  ADD_XML_PARAM(out, header, "ROB_size", NODE_TABLE_SIZE,
                "each in-flight instruction has an entry in ROB");

  /* Registers */
  ADD_XML_PARAM(out, header, "archi_Regs_IRF_size", 16,
                "Number of integer architectural registers");
  ADD_XML_PARAM(out, header, "archi_Regs_FRF_size", 32,
                "Number of floating point architectural registers");

  /* if OoO processor, phy_reg number is needed for renaming logic, renaming
   * logic is for both integer and floating point insts.*/
  ADD_XML_PARAM(
    out, header, "phy_Regs_IRF_size", NODE_TABLE_SIZE,
    "Number of integer physical registers needed for renaming in OoO mode. In "
    "Scarab these are tied to ROB."); /*Scarab: Scarab does not model physical
                                         registers, this is an approximation*/
  ADD_XML_PARAM(out, header, "phy_Regs_FRF_size", NODE_TABLE_SIZE,
                "Number of floating point physical registers needed for "
                "renaming OoO mode. In scarab these are tied to ROB.");

  /* rename logic */
  ADD_XML_PARAM(out, header, "rename_scheme", 0,
                "can be RAM based(0) or CAM based(1) rename scheme RAM-based "
                "scheme will have free list, status table; CAM-based scheme "
                "have the valid bit in the data field of the CAM both RAM and "
                "CAM need RAM-based checkpoint table, checkpoint_depth=# of "
                "in_flight instructions; Detailed RAT Implementation see McPat "
                "TR");
  /* register windows */
  ADD_XML_PARAM(out, header, "register_windows_size", 0,
                "how many windows in the windowed register file, sun "
                "processors; no register windowing is used when this number is "
                "0");

  /* LSU */
  /* Note: LSU_order param is not used in McPat */
  ADD_XML_PARAM_str(
    out, header, "LSU_order", "out-of-order",
    "In OoO cores, loads and stores can be issued whether inorder(Pentium Pro) "
    "or (OoO)out-of-order(Alpha), They will always try to exeute out-of-order "
    "though.");
  /* Note: Scarab does not model a store buffer, this is an estimation*/
  ADD_XML_PARAM(out, header, "store_buffer_size", NODE_TABLE_SIZE / 3, );
  /* Note: Scarab does not model a load buffer, this is an estimation*/
  ADD_XML_PARAM(out, header, "load_buffer_size", NODE_TABLE_SIZE / 3,
                "By default, in-order cores do not have load buffers");

  ADD_XML_PARAM(
    out, header, "memory_ports", DCACHE_READ_PORTS,
    "number of ports refer to sustainable concurrent memory "
    "accesses. max_allowed_in_flight_memo_instructions determins "
    "the # of ports of load and store buffer as well as the ports "
    "of Dcache which is connected to LSU. dual-pumped Dcache can "
    "be used to save the extra read/write ports"); /*Scarab: McPAT
                                                      assumes these
                                                      are R/W,
                                                      dcache_read_ports
                                                      >
                                                      dcache_write_ports*/
  ADD_XML_PARAM(out, header, "RAS_size", CRS_ENTRIES,
                "Size of return address stack");

  /*******************************************************************************************************************
   * Core Stats
   *******************************************************************************************************************/

  /* general stats, defines simulation periods; require total, idle, and busy
   * cycles for sanity check please note: if target architecture is X86, then
   * all the instrucions refer to (fused) micro-ops*/
  ADD_XML_CORE_STAT(out, header, core_id, "total_instructions", POWER_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "int_instructions", POWER_INT_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "fp_instructions", POWER_FP_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "branch_instructions",
                    POWER_BRANCH_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "branch_mispredictions",
                    POWER_BRANCH_MISPREDICT, );
  ADD_XML_CORE_STAT(out, header, core_id, "load_instructions", POWER_LD_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "store_instructions", POWER_ST_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "committed_instructions",
                    POWER_COMMITTED_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "committed_int_instructions",
                    POWER_COMMITTED_INT_OP, );
  ADD_XML_CORE_STAT(out, header, core_id, "committed_fp_instructions",
                    POWER_COMMITTED_FP_OP, );

  ADD_XML_STAT(
    out, header, "pipeline_duty_cycle", OPC_TO_PEAK_OPC_RATIO,
    "<=1, runtime_ipc/peak_ipc; averaged for all cores if homogenous");

  /* the following cycle stats are used for heterogeneouse cores only, please
   * ignore them if homogeneouse cores */
  ADD_XML_CORE_STAT(out, header, core_id, "total_cycles", POWER_CYCLE, );
  ADD_XML_STAT(out, header, "idle_cycles", 0, );
  ADD_XML_CORE_STAT(out, header, core_id, "busy_cycles", POWER_CYCLE, );


  /*instruction buffer stats*/
  /*ROB stats, both RS and Phy based OoOs have ROB performance simulator should
    capture the difference on accesses, otherwise, McPAT has to guess based on
    number of commited instructions.*/
  ADD_XML_CORE_STAT(out, header, core_id, "ROB_reads", POWER_ROB_READ, );
  ADD_XML_CORE_STAT(out, header, core_id, "ROB_writes", POWER_ROB_WRITE, );

  /*RAT accesses*/
  ADD_XML_CORE_STAT(out, header, core_id, "rename_reads", POWER_RENAME_READ,
                    "lookup in renaming logic");
  ADD_XML_CORE_STAT(out, header, core_id, "rename_writes", POWER_RENAME_WRITE,
                    "update dest regs. renaming logic");
  ADD_XML_CORE_STAT(out, header, core_id, "fp_rename_reads",
                    POWER_FP_RENAME_READ, );
  ADD_XML_CORE_STAT(out, header, core_id, "fp_rename_writes",
                    POWER_FP_RENAME_WRITE, );
  /*decode and rename stage use this, should be total ic - nop*/

  /*Inst window stats*/
  ADD_XML_CORE_STAT(out, header, core_id, "inst_window_reads",
                    POWER_INST_WINDOW_READ, );
  ADD_XML_CORE_STAT(out, header, core_id, "inst_window_writes",
                    POWER_INST_WINDOW_WRITE, );
  ADD_XML_CORE_STAT(out, header, core_id, "inst_window_wakeup_accesses",
                    POWER_INST_WINDOW_WAKEUP_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "fp_inst_window_reads",
                    POWER_FP_INST_WINDOW_READ, );
  ADD_XML_CORE_STAT(out, header, core_id, "fp_inst_window_writes",
                    POWER_FP_INST_WINDOW_WRITE, );
  ADD_XML_CORE_STAT(out, header, core_id, "fp_inst_window_wakeup_accesses",
                    POWER_FP_INST_WINDOW_WAKEUP_ACCESS, );

  /*RF accesses*/
  ADD_XML_CORE_STAT(out, header, core_id, "int_regfile_reads",
                    POWER_INT_REGFILE_READ, );
  ADD_XML_CORE_STAT(out, header, core_id, "float_regfile_reads",
                    POWER_FP_REGFILE_READ, );
  ADD_XML_CORE_STAT(out, header, core_id, "int_regfile_writes",
                    POWER_INT_REGFILE_WRITE, );
  ADD_XML_CORE_STAT(out, header, core_id, "float_regfile_writes",
                    POWER_FP_REGFILE_WRITE, );

  /*accesses to the working reg*/
  ADD_XML_CORE_STAT(out, header, core_id, "function_calls",
                    POWER_FUNCTION_CALL, );
  ADD_XML_STAT(out, header, "context_switches", 0, );

  /*Number of Windowes switches (number of function calls and returns)
    Alu stats by default, the processor has one FPU that includes the divider
    and multiplier. The fpu accesses should include accesses to multiplier and
    divider*/

  /*multiple cycle accesses should be counted multiple times,
    otherwise, McPAT can use internal counter for different floating point
    instructions to get final accesses. But that needs detailed info for
    floating point inst mix */

  /*currently the performance simulator should
    make sure all the numbers are final numbers, including the explicit
    read/write accesses, and the implicite accesses such as replacements and
    etc. Future versions of McPAT may be able to reason the implicite access
    based on param and stats of last level cache The same rule applies to all
    cache access stats too!*/
  ADD_XML_CORE_STAT(out, header, core_id, "ialu_accesses", POWER_IALU_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "fpu_accesses", POWER_FPU_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "mul_accesses", POWER_MUL_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "cdb_alu_accesses",
                    POWER_CDB_IALU_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "cdb_mul_accesses",
                    POWER_CDB_MUL_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "cdb_fpu_accesses",
                    POWER_CDB_FPU_ACCESS, );

  /* following is AF for max power computation. Do not change them, unless you
   * understand them */
  ADD_XML_STAT(out, header, "IFU_duty_cycle", 1, );
  ADD_XML_STAT(out, header, "LSU_duty_cycle", 0.5, );
  ADD_XML_STAT(out, header, "MemManU_I_duty_cycle", 1, );
  ADD_XML_STAT(out, header, "MemManU_D_duty_cycle", 0.5, );
  ADD_XML_STAT(out, header, "ALU_duty_cycle", 1, );
  ADD_XML_STAT(out, header, "MUL_duty_cycle", 0.3, );
  ADD_XML_STAT(out, header, "FPU_duty_cycle", 0.3, );
  ADD_XML_STAT(out, header, "ALU_cdb_duty_cycle", 1, );
  ADD_XML_STAT(out, header, "MUL_cdb_duty_cycle", 0.3, );
  ADD_XML_STAT(out, header, "FPU_cdb_duty_cycle", 0.3, );

  /* Note: McPat does not use number_of_BPT param. */
  ADD_XML_PARAM(out, header, "number_of_BPT", 2, );

  /***********************************************************************/
  header = "\t\t";

  ADD_XML_COMPONENT(out, header,
                    "system.core" + std::to_string(core_id) + ".predictor",
                    "PBT", );

  /*branch predictor; tournament predictor see Alpha implementation*/
  ADD_XML_PARAM_str(out, header, "local_predictor_size",
                    "10,3", );  // TODO: these bp params need to look like the
                                // new TAGE predictor that scarab has
  ADD_XML_PARAM(out, header, "local_predictor_entries", 1024, );
  ADD_XML_PARAM(out, header, "global_predictor_entries", 4096, );
  ADD_XML_PARAM(out, header, "global_predictor_bits", 2, );
  ADD_XML_PARAM(out, header, "chooser_predictor_entries", 4096, );
  ADD_XML_PARAM(out, header, "chooser_predictor_bits", 2, );

  /* Note from McPat: These parameters can be combined like below in next
   * version.
   * <param name="load_predictor" value="10,3,1024"/>
   * <param name="global_predictor" value="4096,2"/>
   * <param name="predictor_chooser" value="4096,2"/>
   *
   * Scarab: do we need to update the params to look like this?*/
  END_OF_COMPONENT(out, header);

  /***********************************************************************/

  ADD_XML_COMPONENT(
    out, header, "system.core" + std::to_string(core_id) + ".itlb", "itlb", );
  ADD_XML_PARAM(
    out, header, "number_entries", 128,
    "Scarab: models perfect tlb, this number is hard coded in the power file");

  ADD_XML_CORE_STAT(out, header, core_id, "total_accesses",
                    POWER_ITLB_ACCESS, );
  ADD_XML_STAT(out, header, "total_misses", 0, "Scarab: perfect TLB");
  /* Note: conflicts parameter is not used in McPat anywhere, although some of
   * the predefined descriptor files have non-zero values. */
  ADD_XML_STAT(out, header, "conflicts", 0, );
  /* there is no write requests to itlb although writes happen to itlb after
   * miss, which is actually a replacement */

  END_OF_COMPONENT(out, header);

  /***********************************************************************/

  ADD_XML_COMPONENT(out, header,
                    "system.core" + std::to_string(core_id) + ".icache",
                    "icache", );

  /* Note: icache cycles (scarab assumes 1, that may be too fast for McPAT,
   * bug #25). */
  ADD_XML_PARAM_str(
    out, header, "icache_config",
    std::to_string(ICACHE_SIZE) + "," +        /*Capacity*/
      std::to_string(ICACHE_LINE_SIZE) + "," + /*Block_width*/
      std::to_string(ICACHE_ASSOC) + "," +     /*associativity*/
      std::to_string(ICACHE_BANKS) +           /*bank*/
      ",1,3,32,1", /* throughput w.r.t. core clock, latency w.r.t. core clock,
                    * output_width, cache policy (0 no write or write-though
                    * with non-write allocate; 1 write-back with
                    * write-allocate) */
    "the parameters are capacity,block_width, associativity, bank, throughput "
    "w.r.t. core clock, latency w.r.t. core clock,output_width, cache policy "
    "(0 no write or write-though with non-write allocate;1 write-back with "
    "write-allocate)");

  ADD_XML_PARAM_str(out, header, "buffer_sizes", "16,16,16,0",
                    "cache controller buffer sizes: "
                    "miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_"
                    "size,wb_buffer_size");
  ADD_XML_CORE_STAT(out, header, core_id, "read_accesses",
                    POWER_ICACHE_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "read_misses", POWER_ICACHE_MISS, );
  /* Note: conflicts parameter is not used in McPat anywhere, although some of
   * the predefined descriptor files have non-zero values. */
  ADD_XML_STAT(out, header, "conflicts", 0, );

  END_OF_COMPONENT(out, header);

  /***********************************************************************/

  ADD_XML_COMPONENT(
    out, header, "system.core" + std::to_string(core_id) + ".dtlb", "dtlb", );
  ADD_XML_PARAM(out, header, "number_entries", 128, "dual threads");
  ADD_XML_CORE_STAT(out, header, core_id, "total_accesses",
                    POWER_DTLB_ACCESS, );
  ADD_XML_STAT(out, header, "total_misses", 0, "Scarab: perfect DTLB");
  /* Note: conflicts parameter is not used in McPat anywhere, although some of
   * the predefined descriptor files have non-zero values. */
  ADD_XML_STAT(out, header, "conflicts", 0, );

  END_OF_COMPONENT(out, header);

  /***********************************************************************/

  ADD_XML_COMPONENT(out, header,
                    "system.core" + std::to_string(core_id) + ".dcache",
                    "dcache", );
  /*all the buffer related are optional*/
  ADD_XML_PARAM_str(
    out, header, "dcache_config",
    std::to_string(DCACHE_SIZE) + "," +        /*Capactiy*/
      std::to_string(DCACHE_LINE_SIZE) + "," + /*Block_width*/
      std::to_string(DCACHE_ASSOC) + "," +     /*associativity*/
      std::to_string(DCACHE_BANKS) + "," +     /*bank*/
      "1," +                                   /*throughput w.r.t. core clock*/
      std::to_string(DCACHE_CYCLES) + "," +    /*latency w.r.t. core clock*/
      "64,1",                                  /*output_width, cache policy
                                                *(0 no write or write-though with
                                                *non-write allocate; 1 write-back
                                                *with write-allocate)*/
    "the parameters are capacity,block_width, associativity, bank, throughput "
    "w.r.t. core clock, latency w.r.t. core clock,output_width, cache policy "
    "(0 no write or write-though with non-write allocate;1 write-back with "
    "write-allocate)");

  ADD_XML_PARAM_str(out, header, "buffer_sizes", "16, 16, 16, 16",
                    "cache controller buffer sizes: "
                    "miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_"
                    "size,wb_buffer_size");
  ADD_XML_CORE_STAT(out, header, core_id, "read_accesses",
                    POWER_DCACHE_READ_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "write_accesses",
                    POWER_DCACHE_WRITE_ACCESS, );
  ADD_XML_CORE_STAT(out, header, core_id, "read_misses",
                    POWER_DCACHE_READ_MISS, );
  ADD_XML_CORE_STAT(out, header, core_id, "write_misses",
                    POWER_DCACHE_WRITE_MISS, );
  /* Note: conflicts parameter is not used in McPat anywhere, although some of
   * the predefined descriptor files have non-zero values. */
  ADD_XML_STAT(out, header, "conflicts", 0, );

  END_OF_COMPONENT(out, header);

  /***********************************************************************/

  /* Note: McPat does not use number_of_BTB param. */
  ADD_XML_PARAM(out, header, "number_of_BTB", 1, );
  ADD_XML_COMPONENT(out, header,
                    "system.core" + std::to_string(core_id) + ".BTB", "BTB", );

  /* all the buffer related are optional */
  /* Note: scarab hardcodes block_width to 1 target (8B), do we want to fix
   * this for power?"*/
  ADD_XML_PARAM_str(
    out, header, "BTB_config",
    std::to_string(BTB_ENTRIES) + "," + /*capacity*/
      std::to_string(8) + "," +         /*block_width*/
      std::to_string(BTB_ASSOC) + "," + /*associativity*/
      "1,1,1", /*bank, throughput w.r.t.core clock, latency w.r.t. core clock*/
    "the parameters are capacity,block_width,associativity,bank, throughput "
    "w.r.t. core clock, latency w.r.t. core clock");
  ADD_XML_CORE_STAT(out, header, core_id, "read_accesses", POWER_BTB_READ,
                    "See IFU code for guideline");
  ADD_XML_CORE_STAT(out, header, core_id, "write_accesses", POWER_BTB_WRITE, );

  END_OF_COMPONENT(out, header);

  /***********************************************************************/

  header = "\t";
  END_OF_COMPONENT(out, header);
}

void power_print_cache_directory_params(std::ofstream& out,
                                        uint32_t       num_l1_directories,
                                        uint32_t       num_l2_directories) {
  std::string header = "\t";

  for(uint32_t i = 0; i < num_l1_directories; ++i) {
    ADD_XML_COMPONENT(out, header,
                      std::string("system.L1Directory") + std::to_string(i),
                      std::string("system.L1Directory") + std::to_string(i), );
    ADD_XML_PARAM(out, header, "Directory_type", 0,
                  "0 cam based shadowed tag. 1 directory cache");
    ADD_XML_PARAM_str(out, header, "Dir_config", "4096,2,0,1,100,100, 8",
                      "the parameters are capacity,block_width, "
                      "associativity,bank, throughput w.r.t. core clock, "
                      "latency w.r.t. core clock,");
    ADD_XML_PARAM_str(out, header, "buffer_sizes", "8, 8, 8, 8",
                      "all the buffer related are optional");
    ADD_XML_PARAM(out, header, "clockrate", 3400, );
    ADD_XML_PARAM_str(out, header, "ports", "1,1,1",
                      "number of r, w, and rw search ports");
    ADD_XML_PARAM(out, header, "device_type", 0, );

    /*altough there are multiple access types, Performance simulator needs to
     * cast them into reads or writes e.g. the invalidates can be considered as
     * writes*/
    ADD_XML_STAT(out, header, "read_accesses", 0, );
    ADD_XML_STAT(out, header, "write_accesses", 0, );
    ADD_XML_STAT(out, header, "read_misses", 0, );
    ADD_XML_STAT(out, header, "write_misses", 0, );
    /* Note: conflicts parameter is not used in McPat anywhere, although some of
     * the predefined descriptor files have non-zero values. */
    ADD_XML_STAT(out, header, "conflicts", 0, );
    END_OF_COMPONENT(out, header);
  }

  for(uint32_t i = 0; i < num_l2_directories; ++i) {
    ADD_XML_COMPONENT(out, header,
                      std::string("system.L2Directory") + std::to_string(i),
                      std::string("system.L2Directory") + std::to_string(i), );
    ADD_XML_PARAM(out, header, "Directory_type", 0,
                  "0 cam based shadowed tag. 1 directory cache");
    ADD_XML_PARAM_str(out, header, "Dir_config", "512,4,0,1,1, 1",
                      "the parameters are capacity,block_width, "
                      "associativity,bank, throughput w.r.t. core clock, "
                      "latency w.r.t. core clock,");
    ADD_XML_PARAM_str(out, header, "buffer_sizes", "16, 16, 16, 16",
                      "all the buffer related are optional");
    ADD_XML_PARAM(out, header, "clockrate", 1200, );
    ADD_XML_PARAM_str(out, header, "ports", "1,1,1",
                      "number of r, w, and rw search ports");
    ADD_XML_PARAM(out, header, "device_type", 0, );

    /*altough there are multiple access types, Performance simulator needs to
     * cast them into reads or writes e.g. the invalidates can be considered as
     * writes*/
    ADD_XML_STAT(out, header, "read_accesses", 0, );
    ADD_XML_STAT(out, header, "write_accesses", 0, );
    ADD_XML_STAT(out, header, "read_misses", 0, );
    ADD_XML_STAT(out, header, "write_misses", 0, );
    /* Note: conflicts parameter is not used in McPat anywhere, although some of
     * the predefined descriptor files have non-zero values. */
    ADD_XML_STAT(out, header, "conflicts", 0, );
    END_OF_COMPONENT(out, header);
  }
}

void power_print_l2_params(std::ofstream& out, uint32_t l2_id) {
  std::string header = "\t";

  uint32_t PRIVATE_L1_SIZE  = L1_SIZE / NUM_CORES;
  uint32_t PRIVATE_L1_BANKS = L1_BANKS / NUM_CORES;

  ADD_XML_COMPONENT(out, header,
                    std::string("system.L2") + std::to_string(l2_id),
                    std::string("L2") + std::to_string(l2_id), );
  /*all the buffer related are optional*/
  ADD_XML_PARAM_str(
    out, header, "L2_config",
    std::to_string(PRIVATE_L1_SIZE) + "," +    /*capacity*/
      std::to_string(L1_LINE_SIZE) + "," +     /*block_width*/
      std::to_string(L1_ASSOC) + "," +         /*associativity*/
      std::to_string(PRIVATE_L1_BANKS) + "," + /*bank*/
      std::to_string(PRIVATE_L1_BANKS) + "," + /*throughput w.r.t. core clock*/
      std::to_string(L1_CYCLES) + "," +        /*latency w.r.t. core clock*/
      "32, 1", /*output_width, cache policy (0 no write or write-though with
                  non-write allocate; 1 write-back with write-allocate)*/
    "the parameters are capacity,block_width, associativity, bank, throughput "
    "w.r.t. core clock, latency w.r.t. core clock,output_width, cache policy");

  ADD_XML_PARAM_str(out, header, "buffer_sizes", "16, 16, 16, 16",
                    "cache controller buffer sizes: "
                    "miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_"
                    "size,wb_buffer_size");
  ADD_XML_PARAM(out, header, "clockrate", CHIP_FREQ_IN_MHZ, );
  ADD_XML_PARAM_str(out, header, "ports", "1,1,1",
                    "number of r, w, and rw ports");
  ADD_XML_PARAM(out, header, "device_type", 0, );
  ADD_XML_CORE_STAT(out, header, l2_id, "read_accesses",
                    POWER_LLC_READ_ACCESS, );
  ADD_XML_CORE_STAT(out, header, l2_id, "write_accesses",
                    POWER_LLC_WRITE_ACCESS, );
  ADD_XML_CORE_STAT(out, header, l2_id, "read_misses", POWER_LLC_READ_MISS, );
  ADD_XML_CORE_STAT(out, header, l2_id, "write_misses", POWER_LLC_WRITE_MISS, );
  /* Note: conflicts parameter is not used in McPat anywhere, although some of
   * the predefined descriptor files have non-zero values. */
  ADD_XML_STAT(out, header, "conflicts", 0, );
  ADD_XML_STAT(out, header, "duty_cycle", 1, );
  END_OF_COMPONENT(out, header);
}

void power_print_l3_params(std::ofstream& out) {
  std::string header = "\t";

  ADD_XML_COMPONENT(out, header, std::string("system.L30"),
                    std::string("L30"), );
  /*all the buffer related are optional*/
  ADD_XML_PARAM_str(
    out, header, "L3_config",
    std::to_string(L1_SIZE) + "," +        /*capacity*/
      std::to_string(L1_LINE_SIZE) + "," + /*block_width*/
      std::to_string(L1_ASSOC) + "," +     /*associativity*/
      std::to_string(L1_BANKS) + "," +     /*bank*/
      std::to_string(L1_BANKS) + "," +     /*throughput w.r.t. core clock*/
      std::to_string(L1_CYCLES) + "," +    /*latency w.r.t. core clock*/
      "32, 1", /*output_width, cache policy (0 no write or write-though with
                  non-write allocate; 1 write-back with write-allocate)*/
    "the parameters are capacity,block_width, associativity, bank, throughput "
    "w.r.t. core clock, latency w.r.t. core clock,output_width, cache policy");

  ADD_XML_PARAM(out, header, "clockrate", CHIP_FREQ_IN_MHZ, );
  ADD_XML_PARAM_str(out, header, "ports", "1,1,1",
                    "number of r, w, and rw ports");
  ADD_XML_PARAM(out, header, "device_type", 0, );
  ADD_XML_PARAM_str(out, header, "buffer_sizes", "16, 16, 16, 16",
                    "cache controller buffer sizes: "
                    "miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_"
                    "size,wb_buffer_size");

  ADD_XML_ACCUM_STAT(out, header, "read_accesses", POWER_LLC_READ_ACCESS, );
  ADD_XML_ACCUM_STAT(out, header, "write_accesses", POWER_LLC_WRITE_ACCESS, );
  ADD_XML_ACCUM_STAT(out, header, "read_misses", POWER_LLC_READ_MISS, );
  ADD_XML_ACCUM_STAT(out, header, "write_misses", POWER_LLC_WRITE_MISS, );
  /* Note: conflicts parameter is not used in McPat anywhere, although some of
   * the predefined descriptor files have non-zero values. */
  ADD_XML_STAT(out, header, "conflicts", 0, );
  ADD_XML_STAT(out, header, "duty_cycle", 1, );
  END_OF_COMPONENT(out, header);
}

void power_print_noc_params(std::ofstream& out) {
  std::string header = "\t";

  ADD_XML_COMPONENT(out, header, std::string("system.NoC0"),
                    std::string("noc0"), );
  ADD_XML_PARAM(out, header, "clockrate", 1200, );
  ADD_XML_PARAM(out, header, "type", 1,
                "0:bus, 1:NoC , for bus no matter how many nodes sharing the "
                "bus at each time only one node can send req");
  ADD_XML_PARAM(out, header, "horizontal_nodes", 1, );
  ADD_XML_PARAM(out, header, "vertical_nodes", 1, );
  ADD_XML_PARAM(out, header, "has_global_link", 1,
                "1 has global link, 0 does not have global link");
  ADD_XML_PARAM(out, header, "link_throughput", 1, "w.r.t clock");
  ADD_XML_PARAM(out, header, "link_latency", 1,
                "w.r.t clock, througput >= latency");

  /*Router architecture*/
  ADD_XML_PARAM(out, header, "input_ports", 8, );
  ADD_XML_PARAM(out, header, "output_ports", 7, );

  /*For bus the I/O ports should be 1*/
  ADD_XML_PARAM(out, header, "virtual_channel_per_port", 2, );
  ADD_XML_PARAM(out, header, "input_buffer_entries_per_vc", 128, );
  ADD_XML_PARAM(out, header, "flit_bits", 40, );
  ADD_XML_PARAM(out, header, "chip_coverage", 1,
                "When multiple NOC present, one NOC will cover part of the "
                "whole chip. chip_coverage <=1");
  ADD_XML_PARAM(out, header, "link_routing_over_percentage", 1.0,
                "Links can route over other components or occupy whole area. "
                "by default, 50% of the NoC global links routes over other "
                "components");
  ADD_XML_STAT(out, header, "total_accesses", 0,
               "This is the number of total accesses within the whole network "
               "not for each router");
  ADD_XML_STAT(out, header, "duty_cycle", 1, );
  END_OF_COMPONENT(out, header);
}

void power_print_mc_params(std::ofstream& out) {
  double MEMORY_FREQ_IN_MHZ             = (1e15 / RAMULATOR_TCK) / 1e6;
  double MEMORY_PEAK_RATE_IN_MB_PER_SEC = (double(BUS_WIDTH_IN_BYTES) /
                                           1000000.0) *
                                          2 * MEMORY_FREQ_IN_MHZ * 1e6;  // MBps
  std::string header = "\t";

  ADD_XML_COMPONENT(out, header, std::string("system.mc"), std::string("mc"), );
  /* McPat note: Memory controllers are for DDR(2,3...) DIMMs*/
  /* McPat note: current version of McPAT uses published values for base
   * parameters of memory controller. Improvments on MC will be added in later
   * versions.*/
  ADD_XML_PARAM(out, header, "type", 0, "1: low power; 0 high performance");
  ADD_XML_PARAM(out, header, "mc_clock", uint64_t(MEMORY_FREQ_IN_MHZ),
                "McPat: DIMM IO bus clock rate MHz");
  ADD_XML_PARAM(out, header, "peak_transfer_rate",
                uint64_t(MEMORY_PEAK_RATE_IN_MB_PER_SEC), "MB/S");

  ADD_XML_PARAM(out, header, "block_size", 64, "Bytes");

  /* current McPAT only supports homogeneous memory controllers */
  ADD_XML_PARAM(out, header, "number_mcs", RAMULATOR_CHANNELS, );
  ADD_XML_PARAM(out, header, "memory_channels_per_mc", 1, );
  ADD_XML_PARAM(out, header, "number_ranks", RAMULATOR_RANKS, );

  ADD_XML_PARAM(out, header, "withPHY", 0, );  // TODO: what is this?

  uint32_t MEM_REQ_WINDOW_SIZE = RAMULATOR_READQ_ENTRIES +
                                 RAMULATOR_WRITEQ_ENTRIES;
  ADD_XML_PARAM(out, header, "req_window_size_per_channel",
                MEM_REQ_WINDOW_SIZE, );
  ADD_XML_PARAM(out, header, "IO_buffer_size_per_channel",
                MEM_REQ_WINDOW_SIZE, );

  /* Note: McPAT accpets data bus in bits, internally converts it to bytes, and
   * computes additional bus control bits. This is consistent with the second
   * parameter here, which we subtract log2 of bus width in bytes. */
  ADD_XML_PARAM(out, header, "databus_width", BUS_WIDTH_IN_BYTES * 8, "bits");
  ADD_XML_PARAM(out, header, "addressbus_width",
                physical_address_length - std::log2(BUS_WIDTH_IN_BYTES),
                "McPAT will add the control bus width to the addressbus width "
                "automatically");

  ADD_XML_ACCUM_STAT(out, header, "memory_accesses",
                     POWER_MEMORY_CTRL_ACCESS, );
  ADD_XML_ACCUM_STAT(out, header, "memory_reads", POWER_MEMORY_CTRL_READ, );
  ADD_XML_ACCUM_STAT(out, header, "memory_writes", POWER_MEMORY_CTRL_WRITE, );

  /* McPAT does not track individual mc, instead, it takes the total accesses
   * and calculate the average power per MC or per channel. This is sufficent
   * for most application. Further trackdown can be easily added in later
   * versions.*/
  END_OF_COMPONENT(out, header);
}

void power_print_io_params(std::ofstream& out) {
  std::string header = "\t";

  /***********************************************************************/
  ADD_XML_COMPONENT(out, header, std::string("system.niu"),
                    std::string("niu"), );
  /*On chip 10Gb Ethernet NIC, including XAUI Phy and MAC controller*/
  /*For a minimum IP packet size of 84B at 10Gb/s, a new packet arrives
   * every 67.2ns. the low bound of clock rate of a 10Gb MAC is 150Mhz*/
  ADD_XML_PARAM(out, header, "type", 0, "1: low power; 0 high performance");
  ADD_XML_PARAM(out, header, "clockrate", 350, );
  ADD_XML_PARAM(out, header, "number_units", 0,
                "unlike PCIe and memory controllers, each Ethernet controller "
                "only have one port");
  ADD_XML_STAT(out, header, "duty_cycle", 1.0, "achievable max load <= 1.0");
  ADD_XML_STAT(out, header, "total_load_perc", 0.7,
               "ratio of total achived load to total achivable bandwidth");
  /*McPAT does not track individual nic, instead, it takes the total accesses
   * and calculate the average power per nic or per channel. This is sufficent
   * for most application.*/
  END_OF_COMPONENT(out, header);

  /***********************************************************************/
  ADD_XML_COMPONENT(out, header, std::string("system.pcie"),
                    std::string("pcie"), );
  /*On chip PCIe controller, including Phy*/
  /*For a minimum PCIe packet size of 84B at 8Gb/s per lane (PCIe 3.0), a new
   * packet arrives every 84ns. the low bound of clock rate of a PCIe per lane
   * logic is 120Mhz*/
  ADD_XML_PARAM(out, header, "type", 0, "1: low power; 0 high performance");
  ADD_XML_PARAM(out, header, "withPHY", 1, );
  ADD_XML_PARAM(out, header, "clockrate", 350, );
  ADD_XML_PARAM(out, header, "number_units", 0, );
  ADD_XML_PARAM(out, header, "num_channels", 8, "2 ,4 ,8 ,16 ,32");
  ADD_XML_STAT(out, header, "duty_cycle", 1.0, "achievable max load <= 1.0");
  ADD_XML_STAT(out, header, "total_load_perc", 0.7,
               "Percentage of total achived load to total achivable bandwidth");
  /*McPAT does not track individual pcie controllers, instead, it takes the
    total accesses and calculate the average power per pcie controller or per
    channel. This is sufficent for most application.*/
  END_OF_COMPONENT(out, header);

  /***********************************************************************/
  ADD_XML_COMPONENT(out, header, std::string("system.flashc"),
                    std::string("flashc"), );
  ADD_XML_PARAM(out, header, "number_flashcs", 0, );
  ADD_XML_PARAM(out, header, "type", 1, "1: low power; 0 high performance");
  ADD_XML_PARAM(out, header, "withPHY", 1, );
  ADD_XML_PARAM(out, header, "peak_transfer_rate", 200,
                "Per controller sustainable reak rate MB/S");
  ADD_XML_STAT(out, header, "duty_cycle", 1.0, "achievable max load <= 1.0");
  ADD_XML_STAT(out, header, "total_load_perc", 0.7,
               "Percentage of total achived load to total achivable bandwidth");
  /*McPAT does not track individual flash controller, instead, it takes the
    total accesses and calculate the average power per fc or per channel. This
    is sufficent for most application*/
  END_OF_COMPONENT(out, header);
}

void power_print_memory_parts(std::ofstream& out) {
  double DRAM_TECH_IN_UM = ((double)DRAM_TECH_IN_NM) / 1000;  // default is 32nm
  uint32_t DRAM_BURST_LENGTH = RAMULATOR_TBL * 2;  // RAMULATOR_TBL is in
                                                   // cycles, burst length is in
                                                   // transfers
  uint32_t BUS_WIDTH_IN_BITS = BUS_WIDTH_IN_BYTES * 8;

  uint64_t CHIP_SIZE_IN_BYTES = uint64_t(ramulator_get_chip_size()) * 1024 *
                                1024 / 8;  // Convert MBits to Bytes
  ASSERTM(
    0, CHIP_SIZE_IN_BYTES != 0 && CHIP_SIZE_IN_BYTES <= (1 << 30),
    "chip_size(%lu) is either zero or too large to represent in a 32-bit int\n",
    CHIP_SIZE_IN_BYTES);
  ADD_CACTI_PARAM(out, "size (bytes)", CHIP_SIZE_IN_BYTES, );

  ADD_CACTI_PARAM(out, "block size (bytes)", L1_LINE_SIZE, );
  ADD_CACTI_PARAM(out, "associativity", 1, );
  ADD_CACTI_PARAM(out, "read-write port", 1, );
  ADD_CACTI_PARAM(out, "exclusive read port", 0, );
  ADD_CACTI_PARAM(out, "exclusive write port", 0, );
  ADD_CACTI_PARAM(out, "single ended read ports", 0, );
  ADD_CACTI_PARAM(out, "UCA bank count", 1, );
  ADD_CACTI_PARAM(out, "technology (u)", DRAM_TECH_IN_UM, );

  // following three parameters are meaningful only for main memories
  uint64_t DRAM_CHIP_ROW_BUFFER_SIZE = ramulator_get_chip_row_buffer_size();
  ADD_CACTI_PARAM(out, "page size (bits)", DRAM_CHIP_ROW_BUFFER_SIZE, );
  ADD_CACTI_PARAM(out, "burst length", DRAM_BURST_LENGTH, );
  ADD_CACTI_PARAM(out, "internal prefetch width", 8, );

  // following parameter can have one of the five values -- (itrs-hp, itrs-lstp,
  // itrs-lop, lp-dram, comm-dram)
  ADD_CACTI_PARAM_str(out, "Data array cell type - ", "comm-dram", );

  // following parameter can have one of the three values -- (itrs-hp,
  // itrs-lstp, itrs-lop)
  ADD_CACTI_PARAM_str(out, "Data array peripheral type - ", "itrs-hp", );

  // following parameter can have one of the five values -- (itrs-hp, itrs-lstp,
  // itrs-lop, lp-dram, comm-dram)
  ADD_CACTI_PARAM_str(out, "Tag array cell type - ", "itrs-hp", );

  // following parameter can have one of the three values -- (itrs-hp,
  // itrs-lstp, itrs-lop)
  ADD_CACTI_PARAM_str(out, "Tag array peripheral type - ", "itrs-hp", );

  // Bus width include data bits and address bits required by the decoder
  ADD_CACTI_PARAM(out, "output/input bus width", BUS_WIDTH_IN_BITS, );
  ADD_CACTI_PARAM(out, "operating temperature (K)", 350, );

  ADD_CACTI_PARAM_str(out, "cache type", "main memory", );

  /*to model special structure like branch target buffers, directory, etc.
    change the tag size parameter if you want cacti to calculate the tagbits,
    set the tag size to "default"*/
  ADD_CACTI_PARAM_str(out, "tag size (b) ", "default", );

  /* fast       - data and tag access happen in parallel
   * sequential - data array is accessed after accessing the tag array
   * normal     - data array lookup and tag access happen in parallel final
   * data block is broadcasted in data array h-tree after getting the signal
   * from the tag array*/
  ADD_CACTI_PARAM_str(out, "access mode (normal, sequential, fast) - ",
                      "normal", );

  // DESIGN OBJECTIVE for UCA (or banks in NUCA)
  ADD_CACTI_PARAM_str2(out,
                       "design objective (weight delay, dynamic power, leakage "
                       "power, cycle time, area)",
                       "0:0:0:100:0", );
  ADD_CACTI_PARAM_str2(
    out, "deviate (delay, dynamic power, leakage power, cycle time, area)",
    "20:100000:100000:100000:1000000", );

  ADD_CACTI_PARAM_str(out, "Optimize ED or ED^2 (ED, ED^2, NONE): ", "NONE", );

  ADD_CACTI_PARAM_str(out, "Cache model (NUCA, UCA)  - ", "UCA", );

  ADD_CACTI_PARAM_str(out, "Wire signalling (fullswing, lowswing, default) - ",
                      "Global_10", );

  ADD_CACTI_PARAM_str(out, "Wire inside mat - ", "global", );
  ADD_CACTI_PARAM_str(out, "Wire outside mat - ", "global", );

  ADD_CACTI_PARAM_str(out, "Interconnect projection - ", "conservative", );

  ADD_CACTI_PARAM_str(out, "Add ECC - ", "true", );

  ADD_CACTI_PARAM_str(out, "Print level (DETAILED, CONCISE) - ", "DETAILED", );

  // for debugging
  ADD_CACTI_PARAM_str(out, "Print input parameters - ", "true", );

  /* force CACTI to model the cache with the following Ndbl, Ndwl, Nspd, Ndsam,
   * and Ndcm values */
  ADD_CACTI_PARAM_str(out, "Force cache config -", "false", );
  ADD_CACTI_PARAM(out, "Ndwl", 1, );
  ADD_CACTI_PARAM(out, "Ndbl", 1, );
  ADD_CACTI_PARAM(out, "Nspd", 0, );
  ADD_CACTI_PARAM(out, "Ndcm", 1, );
  ADD_CACTI_PARAM(out, "Ndsam1", 0, );
  ADD_CACTI_PARAM(out, "Ndsam2", 0, );

  /*########### NUCA Params ############*/

  // Objective for NUCA
  ADD_CACTI_PARAM_str2(out,
                       "NUCAdesign objective (weight delay, dynamic power, "
                       "leakage power, cycle time, area)",
                       "100:100:0:0:100", );
  ADD_CACTI_PARAM_str2(
    out, "NUCAdeviate (delay, dynamic power, leakage power, cycle time, area)",
    "10:10000:10000:10000:10000", );

  /* Contention in network (which is a function of core count and cache level)
   * is one of the critical factor used for deciding the optimal bank count
   * value core count can be 4, 8, or 16 */
  ADD_CACTI_PARAM(out, "Core count", 8, );
  ADD_CACTI_PARAM_str(out, "Cache level (L2/L3) - ", "L3", );

  /* In order for CACTI to find the optimal NUCA bank value the following
   * variable should be assigned 0. */
  ADD_CACTI_PARAM(out, "NUCA bank count", 0, );
}

/***********************************************************************************************************
 * Global Functions
 ***********************************************************************************************************/

void power_print_mcpat_xml_infile() {
  std::string   mcpat_infile_name = std::string(FILE_TAG) + "mcpat_infile.xml";
  std::ofstream out;
  out.open(mcpat_infile_name, std::ofstream::out | std::ofstream::trunc);

  std::string header = "";
  out << "<?xml version=\"1.0\" ?>" << std::endl;
  ADD_XML_COMPONENT(out, header, std::string("root"), std::string("root"), );
  ADD_XML_COMPONENT(out, header, "system", "system", );

  num_l2_caches = (PRIVATE_L1 ? NUM_CORES : 0);
  num_l3_caches = (PRIVATE_L1 ? 0 : 1);

  CHIP_FREQ_IN_MHZ = POWER_INTF_REF_CHIP_FREQ / 1000000;
  power_print_system_params(out);

  for(uint32_t core_id = 0; core_id < NUM_CORES; core_id++) {
    power_print_core_params(out, core_id);
  }

  /* Scarab does not model l1/l2 directories, so passing 0. If modeled,
   * number_of_L1Directories and number_of_L2Directories should be passed*/
  power_print_cache_directory_params(out, 1, 1);

  for(uint32_t l2_id = 0; l2_id < num_l2_caches; ++l2_id) {
    power_print_l2_params(out, l2_id);
  }
  if(num_l3_caches == 1) {
    power_print_l3_params(out);
  }

  power_print_noc_params(out);
  power_print_mc_params(out);
  power_print_io_params(out);

  END_OF_COMPONENT(out, header);
  END_OF_COMPONENT(out, header);

  out.close();
}

void power_print_cacti_cfg_infile() {
  std::string   cacti_infile_name = std::string(FILE_TAG) + "cacti_infile.cfg";
  std::ofstream out;
  out.open(cacti_infile_name, std::ofstream::out | std::ofstream::trunc);

  power_print_memory_parts(out);

  out.close();
}

#undef ADD_XML_COMPONENT
#undef END_OF_COMPONENT
#undef ADD_XML_PARAM
#undef ADD_XML_PARAM_str
#undef ADD_XML_STAT
#undef ADD_XML_CORE_STAT
#undef ADD_XML_ACCUM_STAT
#undef XML_PARAM_NAME_WIDTH
#undef XML_PARAM_VALUE_WIDTH
#undef ADD_CACTI_PARAM
#undef ADD_CACTI_PARAM_str

//<?xml version="1.0" ?>
//<component id="root" name="root">
//
//
//		</component>
//</component>
//

/******************************************************************************/
// McPat v1.0 does not model the system.mem component (it's commented out). We
// rely on CACTI for modeling main memory power model.  Keeping the old
// interface here.
/******************************************************************************/
//                <!-- Scarab: McPAT does not model memory, this is bogus -->
//		<component id="system.mem" name="mem">
//			<!-- Main memory property -->
//			<param name="mem_tech_node" value="32"/>
//			<param name="device_clock" value="$MEMORY_FREQ_IN_MHZ"/><!--MHz, this is
// clock rate of the actual memory device, not the FSB --> 			<param
// name="peak_transfer_rate"
// value="$MEMORY_PEAK_RATE_IN_MB_PER_SEC"/><!--MB/S--> 			<param
// name="internal_prefetch_of_DRAM_chip" value="8"/>
//			<!-- 2 for DDR, 4 for DDR2, 8 for DDR3...-->
//			<!-- the device clock, peak_transfer_rate, and the internal prefetch
// decide the DIMM property -->
//			<!-- above numbers can be easily found from Wikipedia -->
//			<param name="capacity_per_channel" value="4096"/> <!-- MB -->
//			<!--
// capacity_per_Dram_chip=capacity_per_channel/number_of_dimms/number_ranks/Dram_chips_per_rank
//			Current McPAT assumes single DIMMs are used.-->
//			<param name="number_ranks" value="2"/>  <!-- FIXME -->
//			<param name="num_banks_of_DRAM_chip" value="$RAMULATOR_BANKS"/>
//			<param name="Block_width_of_DRAM_chip" value="64"/> <!-- B -->
//			<param name="output_width_of_DRAM_chip" value="8"/>
//			<!--number of Dram_chips_per_rank=" 72/output_width_of_DRAM_chip-->
//			<!--number of Dram_chips_per_rank=" 72/output_width_of_DRAM_chip-->
//			<param name="page_size_of_DRAM_chip" value="8"/> <!-- 8 or 16 -->
//			<param name="burstlength_of_DRAM_chip" value="8"/>
//			<stat name="memory_accesses" value="$POWER_MEMORY_ACCESS"/>     <!--
// FIXME: what about row hits etc. --> 			<stat name="memory_reads"
// value="$POWER_MEMORY_READ_ACCESS"/> 			<stat name="memory_writes"
// value="$POWER_MEMORY_WRITE_ACCESS"/>
//		</component>
