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
 * File         : globals/utils.h
 * Author       : HPS Research Group
 * Date         : 10/15/1997
 * Description  : Some useful macros and prototypes for some functions.
 ***************************************************************************************/

#ifndef __UTILS_H__
#define __UTILS_H__

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/types.h>
#include <time.h>
#include "globals/global_defs.h"
#include "globals/global_vars.h"
#include "statistics.h"

/**************************************************************************************/
/* Compiled breakpoint.  Calls breakpoint() when condition is true. */

#define BREAK(cond) \
  if(cond)          \
    breakpoint(__FILE__, __LINE__);


/**************************************************************************************/
/* Write printf-style args to status stream. */

#define WRITE_STATUS(args...)                      \
  {                                                \
    if(mystatus) {                                 \
      fprintf(mystatus, ##args);                   \
      fprintf(mystatus, " %d\n", (int)time(NULL)); \
      fflush(mystatus);                            \
    }                                              \
  }


/**************************************************************************************/
/* Prints out an error message, the file and line number and exits.
   Passes args directly to printf.  */

#define FATAL_ERROR(proc_id, args...)                                         \
  {                                                                           \
    fflush(mystdout);                                                         \
    fprintf(mystderr,                                                         \
            "%s:%u: FATAL ERROR (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
            __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),            \
            (inst_count ? inst_count[proc_id] : 0), cycle_count);             \
    fprintf(mystderr, ##args);                                                \
    fflush(mystderr);                                                         \
    breakpoint(__FILE__, __LINE__);                                           \
    WRITE_STATUS("FATAL");                                                    \
    exit(15);                                                                 \
  }


/**************************************************************************************/
/* Prints out an error message, the file and line number, but does not exit.
   Passes args directly to printf.  */

#define ERROR(proc_id, args...)                                         \
  {                                                                     \
    fflush(mystdout);                                                   \
    fprintf(mystderr,                                                   \
            "%s:%u: ERROR (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
            __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),      \
            (inst_count ? inst_count[proc_id] : 0), cycle_count);       \
    fprintf(mystderr, ##args);                                          \
  }


/**************************************************************************************/
/* Same as ERROR(), but prints only once (subsequent invocations are silent). */

#define ERROR_ONCE(proc_id, args...)                                      \
  {                                                                       \
    static Flag printed_once = FALSE;                                     \
    if(!printed_once) {                                                   \
      fflush(mystdout);                                                   \
      fprintf(mystderr,                                                   \
              "%s:%u: ERROR (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
              __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),      \
              (inst_count ? inst_count[proc_id] : 0), cycle_count);       \
      fprintf(mystderr, ##args);                                          \
      printed_once = TRUE;                                                \
    }                                                                     \
  }


/**************************************************************************************/
/* Prints a warning message with the file and line number. Passes
   args directly to printf. Suppressed if NO_DEBUG is defined. */

#ifndef NO_DEBUG
#define WARNING(proc_id, args...)                                         \
  {                                                                       \
    fflush(mystdout);                                                     \
    fprintf(mystderr,                                                     \
            "%s:%u: WARNING (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
            __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),        \
            (inst_count ? inst_count[proc_id] : 0), cycle_count);         \
    fprintf(mystderr, ##args);                                            \
  }
#else
#define WARNING(args...)
#endif


/**************************************************************************************/
/* Prints a warning message with the file and line number. Passes args
   directly to printf. Always on regardless of NO_DEBUG. */

#define WARNINGU(proc_id, args...)                                        \
  {                                                                       \
    fflush(mystdout);                                                     \
    fprintf(mystderr,                                                     \
            "%s:%u: WARNING (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
            __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),        \
            (inst_count ? inst_count[proc_id] : 0), cycle_count);         \
    fprintf(mystderr, ##args);                                            \
  }


/**************************************************************************************/
/* Same as WARNINGU(), but prints only once (subsequent invocations are silent).
 */

#define WARNINGU_ONCE(proc_id, args...)                                     \
  {                                                                         \
    static Flag printed_once = FALSE;                                       \
    if(!printed_once) {                                                     \
      fflush(mystdout);                                                     \
      fprintf(mystderr,                                                     \
              "%s:%u: WARNING (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
              __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),        \
              (inst_count ? inst_count[proc_id] : 0), cycle_count);         \
      fprintf(mystderr, ##args);                                            \
      printed_once = TRUE;                                                  \
    }                                                                       \
  }


/**************************************************************************************/
/* Prints a warning message with the file and line number if the
   condition is true. Passes args directly to printf. */

#define WARNINGCU(proc_id, cond, args...)                                      \
  {                                                                            \
    if(cond) {                                                                 \
      fflush(mystdout);                                                        \
      fprintf(mystderr,                                                        \
              "%s:%u: WARNING (P=%u  O=%llu  I=%llu  C=%llu): %s: ", __FILE__, \
              __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),           \
              (inst_count ? inst_count[proc_id] : 0), cycle_count, (#cond));   \
      fprintf(mystderr, ##args);                                               \
    }                                                                          \
  }


/**************************************************************************************/
/* Prints a "nice" message. Passes args directly to printf. Suppressed
   if NO_DEBUG is defined. */

#ifndef NO_DEBUG
#define MESSAGE(proc_id, args...)                                         \
  {                                                                       \
    fflush(mystderr);                                                     \
    fprintf(mystdout,                                                     \
            "%s:%u: MESSAGE (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
            __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),        \
            (inst_count ? inst_count[proc_id] : 0), cycle_count);         \
    fprintf(mystdout, ##args);                                            \
  }
#else
#define MESSAGE(args...)
#endif


/**************************************************************************************/
/* Prints a "nice" message unconditionally (regardless of
   NO_DEBUG). Passes args directly to printf. */

#define MESSAGEU(proc_id, args...)                                        \
  {                                                                       \
    fflush(mystderr);                                                     \
    fprintf(mystdout,                                                     \
            "%s:%u: MESSAGE (P=%u  O=%llu  I=%llu  C=%llu):  ", __FILE__, \
            __LINE__, proc_id, (op_count ? op_count[proc_id] : 0),        \
            (inst_count ? inst_count[proc_id] : 0), cycle_count);         \
    fprintf(mystdout, ##args);                                            \
  }


/**************************************************************************************/
/* Some macros. */

#define NUM_ELEMENTS(arr) (sizeof(arr) / sizeof(arr[0]))

#define ROUND_UP(N, ALIGN) (((N) + ((ALIGN)-1)) & ~((ALIGN)-1))
#define ROUND_DOWN(N, ALIGN) ((N) & ~((ALIGN)-1))

/* produces an integer with the N lowest-order bits set to 1's */
#define N_BIT_MASK(N) ((0x1ULL << (N)) - 1)
#define N_BIT_MASK_64 (0xffffffffffffffffULL)

/* extract v[h:l] */
#define EXTR_32(v, h, l) (((ULong)(v) >> (l)) & N_BIT_MASK((h) - (l) + 1))
#define EXTR_64(v, h, l) (((UQuad)(v) >> (l)) & N_BIT_MASK((h) - (l) + 1))

#define UNS_TESTBIT(v, b) (((v) >> (b)) & 0x1)
#define UNS_SETBIT(v, b) ((v) | 0x1 << (b))
#define UNS_CLRBIT(v, b) ((v) & ~(0x1 << (b)))

#define TESTBIT(v, b) (((v) >> (b)) & (typeof(v))1)
#define SETBIT(v, b) ((v) |= (typeof(v))1 << (b))
#define CLRBIT(v, b) ((v) &= ~((typeof(v))1 << (b)))
#define DEFBIT(v, b, x) \
  ((v) = ((v) & ~((typeof(v))1 << (b))) | ((typeof(v))((x) ? 1 : 0) << (b)))

// low and hi are relative to the direction you are incrementing
// (normally low is closer to the head) (note: CD(A,B) != -CD(B,A))
#define CIRC_DIFF(low, hi, num) \
  ((low) <= (hi) ? (hi) - (low) : ((num) - (low) + (hi)))
#define CIRC_MAX(val0, val1, head, num)                                   \
  (CIRC_DIFF((head), (val0), (num)) >= CIRC_DIFF((head), (val1), (num)) ? \
     (val0) :                                                             \
     (val1))
#define CIRC_MIN(val0, val1, head, num)                                   \
  (CIRC_DIFF((head), (val0), (num)) <= CIRC_DIFF((head), (val1), (num)) ? \
     (val0) :                                                             \
     (val1))

#define CIRC_INC(val, num) (((val) == ((num)-1)) ? 0 : (val) + 1)
#define CIRC_DEC(val, num) (((val) == 0) ? (num)-1 : (val)-1)

#define CIRC_INC2(val, pow2) (((val) + 1) & ((pow2)-1))
#define CIRC_DEC2(val, pow2) (((val)-1) & ((pow2)-1))

#define CIRC_ADD(val0, val1, num) (((val0) + (val1)) % (num))
#define CIRC_SUB(val0, val1, num) (((num) + (val0) - (val1)) % (num))

#define CIRC_ADD2(val0, val1, pow2) (((val0) + (val1)) & ((pow2)-1))
#define CIRC_SUB2(val0, val1, pow2) (((val0) - (val1)) & ((pow2)-1))

#define SAT_INC(val, max) ((val) == (max) ? (max) : (val) + 1)
#define SAT_DEC(val, min) ((val) == (min) ? (min) : (val)-1)

#define SAT_ADD(val1, val2, max) \
  ((val1 + val2) >= (max) ? (max) : (val1 + val2))
#define SAT_SUB(val1, val2, min) \
  ((int)((int)(val1)-val2) < (min) ? (min) : (val1 - val2))

#define MIN2(v0, v1) (((v0) < (v1)) ? (v0) : (v1))
#define MAX2(v0, v1) (((v0) > (v1)) ? (v0) : (v1))
#define MIN3(v0, v1, v2) (MIN2((v0), MIN2((v1), (v2))))
#define MAX3(v0, v1, v2) (MAX2((v0), MAX2((v1), (v2))))
#define MIN4(v0, v1, v2, v3) (MIN2(MIN2((v0), (v1)), MIN2((v2), (v3))))
#define MAX4(v0, v1, v2, v3) (MAX2(MAX2((v0), (v1)), MAX2((v2), (v3))))

#define BANK(a, num, int) ((a) >> LOG2(int) & N_BIT_MASK(LOG2(num)))
#define CHANNEL(bank, num) ((bank) >> LOG2(num))
#define BANK_IN_CHANNEL(bank, num) ((bank)&N_BIT_MASK(LOG2(num)))

/* Model 32 bit wraparound while maintaining the proc_id bits */
#define ADDR_PLUS_OFFSET(addr, offset) \
  (((addr)&0xFFFF000000000000ULL) |    \
   (((addr) + (offset)) & 0x0000FFFFFFFFFFFFULL))

/* do addresses 0 and 1 overlap?  */
#define BYTE_OVERLAP(a0, s0, a1, s1)          \
  ((uns32)(a0) - (uns32)(a1) < (uns32)(s1) || \
   (uns32)(a1) - (uns32)(a0) < (uns32)(s0))
/* does address 0 contain address 1? */
#define BYTE_CONTAIN(a0, s0, a1, s1) \
  ((a1) >= (a0) && (a1) + (s1) <= (a0) + (s0))

#define PCT_OF(x, y) ((float)(x)*100 / (y))
#define INV_PCT_OF(x, y) (PCT_OF(y - x, y))

#define ROTATE_LEFT(width, v, num) \
  ((((v) << (num)) & N_BIT_MASK(width)) | ((v) >> ((width) - (num))))

#define LOG10(x)                                          \
  ((x < 10) ? 0 :                                         \
              ((x < 100) ?                                \
                 1 :                                      \
                 ((x < 1000) ?                            \
                    2 :                                   \
                    ((x < 10000) ?                        \
                       3 :                                \
                       ((x < 100000) ?                    \
                          4 :                             \
                          ((x < 1000000) ?                \
                             5 :                          \
                             ((x < 10000000) ?            \
                                6 :                       \
                                ((x < 100000000ULL) ?     \
                                   7 :                    \
                                   ((x < 1000000000ULL) ? \
                                      8 :                 \
                                      ((x < 10000000000ULL) ? 9 : 10))))))))))

#define LOG2(x) (sizeof(uns) * 8 - __builtin_clz((x)) - 1)
#define LOG2_64(x) (sizeof(uns64) * 8 - __builtin_clzll((x)) - 1)

#define FLUSH_OP(op) (op->op_num > bp_recovery_info->recovery_op_num)

#define IS_FLUSHING_OP(op) ((op->op_num == bp_recovery_info->recovery_op_num))

#define ASSERT_PROC_ID_IN_ADDR(proc_id, addr)                                \
  ASSERTM(proc_id, proc_id == get_proc_id_from_cmp_addr(addr),               \
          "Proc ID (%d) does not match proc ID in address (%d)!\n", proc_id, \
          get_proc_id_from_cmp_addr(addr));

/**************************************************************************************/
/* Prototypes for functions in globals/utils.c */

uns64 reverse64(uns64);
uns32 reverse32(uns32);
uns64 reverse(uns64, uns);
void  byte_swap(void*, size_t);
uns   popcount32(uns32);
char* hexstr64(uns64);
char* hexstr64s(uns64);
char* binstr64(uns64);
char* binstr64s(uns64);
char* unsstr64(uns64);
char* unsstr64c(uns64);
char* intstr64(int64);
void  breakpoint(const char[], const int);
uns64 byte_mask_8_to_bit_mask_64(uns8);
uns64 xor_fold_bits(uns64, uns);
int   strin(const char*, const char* const[], const uns);
uns   log2_ctr(Counter);
void  cfprintf(FILE*, const char*, ...);
FILE* file_tag_fopen(char const* const, char const* const, char const* const);
uns   factorial(uns);
Flag  similar(float, float, float);
Flag  is_power_of_2(uns64);
Addr  convert_to_cmp_addr(uns8 proc_id, Addr addr);
uns   get_proc_id_from_cmp_addr(Addr addr);
Addr  check_and_remove_addr_sign_extended_bits(Addr virt_addr,
                                               uns  num_non_sign_extended_bits,
                                               Flag verify_bits_masked_out);
int   parse_int_array(int dest[], const void* str, int max_num);
int   parse_uns_array(uns dest[], const void* str, int max_num);
int   parse_uns64_array(uns64 dest[], const void* str, int max_num);
int   parse_float_array(float dest[], const void* str, int max_num);
int   parse_double_array(double dest[], const void* str, int max_num);
int   parse_string_array(char dest[][MAX_STR_LENGTH + 1], const void* str,
                         int max_num);

/* for use in qsort */
int compare_uns64(const void*, const void*);

#endif /* #ifndef __UTILS_H__ */
