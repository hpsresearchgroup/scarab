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
 * File         : isa/isa_macros.h
 * Author       : HPS Research Group
 * Date         : 3/30/2000
 * Description  :
 ***************************************************************************************/

#ifndef __ISA_MACROS_H__
#define __ISA_MACROS_H__

/**************************************************************************************/
/* ISA definition macros */

#define NUM_INT_REGS 32 /* default 32 */ /* number of integer registers */
#define NUM_FP_REGS 32 /* default 32 */  /* number of float registers */
#define NUM_SPEC_REGS 8                  /* guess, may be unnecessary */
#define NUM_SPARE_REGS 8
#define NUM_EXTRA_REGS          \
  (NUM_INT_REGS + NUM_FP_REGS + \
   NUM_SPARE_REGS) /* number of extra (fake) registers */
#define NUM_REG_IDS \
  (NUM_INT_REGS + NUM_FP_REGS + NUM_SPEC_REGS + NUM_EXTRA_REGS)


#define SRC_REG_ID(src)                    \
  (((src).type > INT_REG) * NUM_INT_REGS + \
   ((src).type > FP_REG) * NUM_FP_REGS +   \
   ((src).type > SPEC_REG) * NUM_SPEC_REGS + ((src).reg))
#define INT_REG_ID(reg) (reg)
#define FP_REG_ID(reg) (reg + NUM_INT_REGS)
#define SPEC_REG_ID(reg) (reg + NUM_INT_REGS + NUM_FP_REGS)
#define EXTRA_REG_ID(reg) (reg + NUM_INT_REGS + NUM_FP_REGS + NUM_SPEC_REGS)

#define IS_CALLSYS(tab) ((tab)->cf_type == CF_SYS)
#define IS_NOP(tab) ((tab)->op_type == OP_NOP)

/**************************************************************************************/

#endif /* #ifndef __ISA_MACROS_H__ */
