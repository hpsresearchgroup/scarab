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

#include "pin/pin_lib/x87_stack_delta.h"
#include <iostream>

#define REG(x) SCARAB_REG_##x,
typedef enum Reg_Id_struct {
#include "isa/x86_regs.def"
  SCARAB_NUM_REGS
} Reg_Id;
#undef REG

#include "pin/pin_lib/x86_decoder.h"

#define ASSERTX assert

/* x87 stack deltas for floating point opcodes */
static struct {
  const char* name;
  int         delta;
} opcode_infos[] = {
  // copied from Rustam's dynamic_data_flow_graph/src/x86.cpp
  {"F2XM1", 0},     // FP 2*x - 1
  {"FABS", 0},      // FP absolute value
  {"FADD", 0},      // FP add
  {"FADDP", +1},    // FP add (pop)
  {"FBLD", -1},     // FP BCD load
  {"FBSTP", +1},    // FP BCD store (pop)
  {"FCHS", 0},      // FP negation
  {"FCOM", 0},      // FP compare
  {"FCOMP", +1},    // FP compare (pop)
  {"FCOMPP", +2},   // FP compare (pop twice)
  {"FCOMI", 0},     // FP compare (eflags)
  {"FCOMIP", +1},   // FP compare (eflags, pop)
  {"FDECSTP", -1},  // FP decrement stack ptr
  {"FUCOM", 0},     // FP compare
  {"FUCOMP", +1},   // FP compare (pop)
  {"FUCOMPP", +2},  // FP compare (pop twice)
  {"FUCOMI", 0},    // FP compare (eflags)
  {"FUCOMIP", +1},  // FP compare (eflags, pop)
  {"FCOS", 0},      // FP cosine
  {"FDIV", 0},      // FP divide
  {"FDIVP", +1},    // FP divide (pop)
  {"FIDIV", 0},     // FP divide by integer
  {"FDIVR", 0},     // FP reverse divides
  {"FDIVRP", +1},   // ...
  {"FIDIVR", 0},    // ...
  {"FIADD", 0},     // FP add an integer reg
  {"FICOM", 0},     // FP compare to integer
  {"FICOMP", +1},   // FP compare to integer (pop)
  {"FILD", -1},     // FP integer load
  {"FINCSTP", +1},  // FP increment stack ptr
  {"FIST", 0},      // FP integer store
  {"FISTP", +1},    // FP integer store (pop)
  {"FISTTP", +1},   // FP integer store (truncate, pop)
  {"FLD", -1},      // FP load
  {"FLD1", -1},     // FP load 1
  {"FLDL2T", -1},   // FP load log_2(10)
  {"FLDL2E", -1},   // FP load log_2(e)
  {"FLDPI", -1},    // FP load pi
  {"FLDLG2", -1},   // FP load log_10(2)
  {"FLDLN2", -1},   // FP load ln(2)
  {"FLDZ", -1},     // FP load 0
  {"FMUL", 0},      // FP multiply
  {"FMULP", +1},    // FP multiply (pop)
  {"FIMUL", 0},     // FP integer multiply
  {"FNOP", 0},      // FP nop
  {"FPATAN", +1},   // FP arctan
  {"FPTAN", -1},    // FP tan
  {"FRNDINT", 0},   // FP round to integer
  {"FSCALE", 0},    // FP scale
  {"FSIN", 0},      // FP sine
  {"FSINCOS", -1},  // FP sine and cosine
  {"FSQRT", 0},     // FP square root
  {"FST", 0},       // FP store
  {"FSTP", +1},     // FP store (pop)
  {"FSUB", 0},      // FP subtract
  {"FSUBP", +1},    // FP subtract (pop)
  {"FISUB", 0},     // FP integer subtract
  {"FSUBR", 0},     // FP reverse subtracts
  {"FSUBRP", +1},   // ...
  {"FISUBR", 0},    // ...
  {"FTST", 0},      // FP test
  {"FXAM", 0},      // FP classify value
  {"FXCH", 0},      // FP exchange
  {"FXTRACT", -1},  // FP extract
  {"FYL2X", +1},    // FP y*log_2(x)       (pop)
  {"FYL2XP1", +1},  // FP y*log_2(x + 1)   (pop)
  // Extras to cover all instructions starting with F in Pin 2.0
  {"FEMMS", 0},     // Clear MMX state
  {"FXSAVE", 0},    // Save x87/MMX/SSE state
  {"FXSAVE64", 0},  // Save x87/MMX/SSE state
  {"FNSAVE", 0},
  {"FXRSTOR", 0},    // Restore x87/MMX/SSE state (can actually change x87 stack
                     // pointer but the delta is not known statically)
  {"FXRSTOR64", 0},  // Restore x87/MMX/SSE state (can actually change x87 stack
                     // pointer but the delta is not known statically)
  {"FRSTOR", 0},
  {"FLDENV", 0},  // Load x87 environment (can actually change x87 stack pointer
                  // but the delta is not known statically)
  {"FLDCW", 0},   // Load x87 control word
  {"FNSTENV", 0},  // Store x87 environment
  {"FNSTCW", 0},   // Store x87 control word
  {"FLDLPI", -1},  // Misspelled version of FLDPI above
  {"FPREM", 0},    // Partial remainder
  {"FPREM1", 0},   // Partial remainder (different rounding)
  {"FCMOVB", 0},   // FP conditional moves...
  {"FCMOVE", 0},
  {"FCMOVBE", 0},
  {"FCMOVU", 0},
  {"FCMOVNB", 0},
  {"FCMOVNE", 0},
  {"FCMOVNBE", 0},
  {"FCMOVNU", 0},
  {"FNCLEX", 0},  // Clear exceptions
  {"FNINIT", 0},  // Initialize x87 (actually sets the x87 stack pointer to
                  // zero, but should be very rare)
  {"FSETPM287_NOP", 0},  // Set protected mode in 287 (nowadays a NOP)
  {"FNSTSW", 0},         // Store status word
  {"FFREE", 0},          // Free FP reg
  {"FREEP", +1},         // Free FP reg (pop)
  // Extras to cover all instructions starting with F in Pin 2.8
  {"FDISI8087_NOP", 0},
  {"FENI8087_NOP", 0},
  {"FFREEP", +1},  // Free FP reg (pop)
  {"FSTPNCE",
   +1},  // Could not find description, assume it's a form of FSTP (with a pop)
  {"FWAIT", 0},
  {NULL, 0}  // this is the sentinel, must be last
};

int       opcode_to_delta_map[XED_ICLASS_LAST];
int       x87_stack_ptr  = 0;
const int X87_STACK_SIZE = 8;

int pops_x87_stack(int opcode) {
  return opcode_to_delta_map[opcode] > 0;
}

int absolute_reg(int reg, int opcode, bool write) {
  if(reg >= SCARAB_REG_FP0 && reg < SCARAB_REG_FP0 + X87_STACK_SIZE) {
    ASSERTX(opcode < XED_ICLASS_LAST);
    int correction = 0;
    if(write && opcode_to_delta_map[opcode] < 0) {
      // Pin reports destinations of x87 stack pushes after TOP is changed
      // This variable corrects that.
      correction = opcode_to_delta_map[opcode];
    }
    return (X87_STACK_SIZE + reg - (int)SCARAB_REG_FP0 + x87_stack_ptr +
            correction) %
             X87_STACK_SIZE +
           (int)SCARAB_REG_FP0;
  }
  return reg;
}

void update_x87_stack_state(int opcode) {
  ASSERTX(opcode < XED_ICLASS_LAST);
  x87_stack_ptr = (x87_stack_ptr + X87_STACK_SIZE +  // avoid negative result
                   opcode_to_delta_map[opcode]) %
                  X87_STACK_SIZE;
  ASSERTX(x87_stack_ptr >= 0);
}

void init_x87_stack_delta() {
  for(size_t opcode = XED_ICLASS_INVALID; opcode < XED_ICLASS_LAST; ++opcode) {
    opcode_to_delta_map[opcode] = 0;
    xed_iclass_enum_t iclass    = static_cast<xed_iclass_enum_t>(opcode);
    const std::string opcode_name(xed_iclass_enum_t2str(iclass));
    size_t            i;
    for(i = 0; opcode_infos[i].name; ++i) {
      if(opcode_name == std::string(opcode_infos[i].name)) {
        opcode_to_delta_map[opcode] = opcode_infos[i].delta;
        break;
      }
    }
    if(!opcode_infos[i].name && opcode_name.c_str()[0] == 'F') {
      //      std::clog << "Possible unmatched x87 opcode: " << opcode_name
      //        << std::endl;
    }
  }
}
