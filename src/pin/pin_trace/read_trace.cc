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

#include <cstdio>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <stdio.h>
#include <string>
#include <tuple>
#include <unordered_map>

#include "../../ctype_pin_inst.h"
extern "C" {
#include "../../isa/isa.h"
}

using namespace std;

typedef std::tuple<int, int, int> tuple3;
typedef std::map<tuple3, bool>    map_type;
map_type                          occurance_map;

void print_regs(char const* const name, int number, compressed_reg_t* array) {
  cout << name << ": ";
  for(int i = 0; i < number; ++i) {
    cout << disasm_reg(Reg_Id(array[i])) << ",";
  }
  cout << "\n";
}

int main(int argc, char* argv[]) {
  if(argc < 2) {
    cerr << "Usage read <trace file name>" << endl;
    exit(1);
  }

  std::cout << sizeof(ctype_pin_inst_struct) << "\n";
  FILE* orig_stream;
  // orig_stream = fopen("test_trace.orig", "r");
  char cmdline[1024];
  sprintf(cmdline, "bzip2 -dc %s", argv[1]);
  orig_stream = popen(cmdline, "r");
  ctype_pin_inst_struct pin_inst;
  int                   inst_count = 0;

  while(fread(&pin_inst, sizeof(ctype_pin_inst), 1, orig_stream)) {
    tuple3 occurance_key = std::make_tuple(pin_inst.op_type, pin_inst.num_ld,
                                           pin_inst.num_st);

    // if (occurance_map.count(occurance_key) == 1) continue;

    // occurance_map[occurance_key] = true;

    cout << "*** beginning of the data structure *** count:" << inst_count
         << endl;
    inst_count++;

    cout << "EIP: " << hex << pin_inst.instruction_addr << "\n";
    cout << "Next EIP: " << hex << pin_inst.instruction_next_addr << "\n";
    cout << "OpType: " << (int)pin_inst.op_type << "\n";
    cout << "ICLASS: " << pin_inst.pin_iclass << "\n";
    cout << "Number of Loads: " << dec << (uint32_t)pin_inst.num_ld << "\n";
    cout << "Number of Store: " << (uint32_t)pin_inst.num_st << "\n";
    cout << "Load Size: " << (uint32_t)pin_inst.ld_size << "\n";
    cout << "Store Size: " << (uint32_t)pin_inst.st_size << "\n";
    cout << "Number of SIMD Lanes: " << dec << (int)pin_inst.num_simd_lanes
         << "\n";
    cout << "Lane Width: " << (int)pin_inst.lane_width_bytes << "\n";
    cout << "Is Repeat Instruction: "
         << (((int)pin_inst.is_repeat == 1) ? "Yes" : "No") << "\n";
    cout << "Control Flow Instruction: "
         << (((int)pin_inst.cf_type != 0) ? "Yes" : "No") << "\n";
    cout << "Branch Target: 0x" << hex << pin_inst.branch_target << "\n";
    cout << "Actually Taken: 0x" << dec << (int)pin_inst.actually_taken << "\n";

    print_regs("Source Regs", pin_inst.num_src_regs, pin_inst.src_regs);
    print_regs("Destination Regs", pin_inst.num_dst_regs, pin_inst.dst_regs);
    print_regs("First Load Address Regs", pin_inst.num_ld1_addr_regs,
               pin_inst.ld1_addr_regs);
    print_regs("Second Load Address Regs", pin_inst.num_ld2_addr_regs,
               pin_inst.ld2_addr_regs);
    print_regs("Store Address Regs", pin_inst.num_st_addr_regs,
               pin_inst.st_addr_regs);

    cout << "*** end of the data structure *** " << endl << endl;
  }

  pclose(orig_stream);
}
