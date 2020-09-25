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
 * File         : frontend/pin_trace_read.cc
 * Author       : HPS Research Group
 * Date         :
 * Description  :
 ****************************************************************************************/
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <iostream>
#include <string>

#include "frontend/pin_trace_read.h"
#include "isa/isa.h"

extern "C" {
#include "globals/utils.h"
}

#define CMP_ADDR_MASK (((uint64_t)-1) << 58)

FILE** pin_file;

// static Reg_Id convert_pin_reg_to_scarab_reg(uns pin_reg);
void pin_trace_file_pointer_init(unsigned char num_cores) {
  pin_file = (FILE**)malloc(num_cores * sizeof(FILE*));
}

void pin_trace_open(unsigned char proc_id, const char* name) {
  char cmdline[1024];
  sprintf(cmdline, "bzip2 -dc %s", name);
  pin_file[proc_id] = popen(cmdline, "r");
  printf("pin trace should be opened now for core %u: %s \n", proc_id, name);
  if(!pin_file[proc_id]) {
    printf("Cannot open trace file: %s\n", name);
    exit(1);
  }
}

void pin_trace_close(unsigned char proc_id) {
  pclose(pin_file[proc_id]);
}

int pin_trace_read(unsigned char proc_id, ctype_pin_inst* pi) {
  int read_size;

  read_size = fread(pi, sizeof(ctype_pin_inst), 1, pin_file[proc_id]);
  if(read_size != 1) {
    return 0;
  }
  return 1;
}
