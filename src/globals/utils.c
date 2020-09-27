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
 * File         : globals/utils.c
 * Author       : HPS Research Group
 * Date         : 10/19/1997
 * Description  : Utility functions.
 ***************************************************************************************/

#include <stdlib.h>
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"

#include "core.param.h"
#include "general.param.h"

#define CMP_ADDR_MASK (((Addr)-1) << 58)
/**************************************************************************************/
/* breakpoint: A function to help debugging. */

void breakpoint(const char file[], const int line) {
  ;
}


/**************************************************************************************/
/* reverse_bits: */

uns64 reverse64(uns64 num) {
  uns64 ans = 0x0;
  uns   ii;

  for(ii = 0; ii < 64; ii++)
    ans |= ((num & 0x1ULL << ii) >> ii) << (63 - ii);

  return ans;
}

uns32 reverse32(uns32 num) {
  uns32 ans = 0x0;
  uns   ii;

  for(ii = 0; ii < 32; ii++)
    ans |= ((num & 0x1ULL << ii) >> ii) << (31 - ii);

  return ans;
}

uns64 reverse(uns64 num, uns size) {
  uns64 ans = 0x0;
  uns   ii;

  for(ii = 0; ii < size; ii++)
    ans |= ((num & 0x1ULL << ii) >> ii) << (size - 1 - ii);

  return ans;
}

/**************************************************************************************/
/* popcount: */

uns popcount32(uns32 num) {
  uns ans = 0;
  uns ii;
  for(ii = 0; ii < sizeof(num) * CHAR_BIT; ii++) {
    ans += (num >> ii) & 1;
  }
  return ans;
}

  /**************************************************************************************/
  /* byte_swap: This function exists for support of big endian (ie. stupid)
   * machines.  it will reorder the bytes of a structure given a pointer to it
   * and its size.  If not compiled with -DBYTE_SWAP, the function is empty and
   * does nothing.  */

#ifdef BYTE_SWAP

void byte_swap(void* ptr, size_t size) {
  char t, *p = (char*)ptr;
  uns  ii;

  ASSERT(0, size);
  ASSERT(0, ptr);

  switch(size) {
    case 1:
      break;
    case 2:
      t = p[0], p[0] = p[1], p[1] = t;
      break;
    case 4:
      t = p[0], p[0] = p[3], p[3] = t;
      t = p[1], p[1] = p[2], p[2] = t;
      break;
    default:
      for(ii = 0; ii < size / 4; ii++, p += 4) {
        t = p[0], p[0] = p[3], p[3] = t;
        t = p[1], p[1] = p[2], p[2] = t;
      }
      break;
  }
}

#else

void byte_swap(void* ptr, size_t size) {
  ;
}

#endif


/**************************************************************************************/
/* hexstr64: This little function exists to convert a 64-bit integer into a
 string.  Its implementation is a little ugly, but it is intended to be called
 from printf, so I need it to return a pointer to a string.  */

char* hexstr64(uns64 value) {
  static char hex64_buffer[MAX_SIMULTANEOUS_STRINGS][MAX_STR_LENGTH + 1];
  static int  counter = 0;

  counter = CIRC_INC2(counter, MAX_SIMULTANEOUS_STRINGS);
  snprintf(hex64_buffer[counter], MAX_STR_LENGTH, "%08x%08x",
           (uns)(value >> 32), (uns)(value & 0xffffffff));
  return hex64_buffer[counter];
}


/**************************************************************************************/
/* hexstr64s:  Just like hexstr64, except it strips off leading zeros */

char* hexstr64s(uns64 value) {
  static char hex64_buffer[MAX_SIMULTANEOUS_STRINGS][MAX_STR_LENGTH + 1];
  static int  counter = 0;
  char*       temp;

  counter = CIRC_INC2(counter, MAX_SIMULTANEOUS_STRINGS);
  snprintf(hex64_buffer[counter], MAX_STR_LENGTH, "%08x%08x",
           (uns)(value >> 32), (uns)(value & 0xffffffff));
  for(temp = hex64_buffer[counter]; *temp == '0' && *(temp + 1) != '\0'; temp++)
    ;
  return temp;
}


/**************************************************************************************/
/* binstr64: This little function exists to convert a 64-bit integer into a
 string.  Its implementation is a little ugly, but it is intended to be called
 from printf, so I need it to return a pointer to a string.  */

char* binstr64(uns64 value) {
  static char bin64_buffer[MAX_SIMULTANEOUS_STRINGS][MAX_STR_LENGTH + 1];
  static int  counter = 0;
  int         ii      = 0;
  counter             = CIRC_INC2(counter, MAX_SIMULTANEOUS_STRINGS);
  while(ii < 64) {
    bin64_buffer[counter][ii] = value & 0x1ULL << (63 - ii) ? '1' : '0';
    ii++;
  }
  ASSERT(0, ii < MAX_STR_LENGTH);
  bin64_buffer[counter][ii] = '\0';
  return bin64_buffer[counter];
}


/**************************************************************************************/
/* binstr64s:  Just like binstr64, except it strips off leading zeros */

char* binstr64s(uns64 value) {
  static char bin64_buffer[MAX_SIMULTANEOUS_STRINGS][MAX_STR_LENGTH + 1];
  static int  counter = 0;
  char*       temp;
  int         ii = 0;
  counter        = CIRC_INC2(counter, MAX_SIMULTANEOUS_STRINGS);
  while(ii < 64) {
    bin64_buffer[counter][ii] = value & 0x1ULL << (63 - ii) ? '1' : '0';
    ii++;
  }
  ASSERT(0, ii < MAX_STR_LENGTH);
  bin64_buffer[counter][ii] = '\0';
  for(temp = bin64_buffer[counter]; *temp == '0' && *(temp + 1) != '\0'; temp++)
    ;
  return temp;
}


  /**************************************************************************************/
  /* print_ull_guts: Print the least significant digit at d and proceed to
   higher significant digits lower in memory.  Return a pointer to the first
   char of the printed value.  */

#define BILLION (1000000000ULL)

static char* print_ull_guts(char* d, uns64 ull, unsigned zero_p) {
  unsigned work, orig_work;
  unsigned digits = 0;

  /*
   * Fill in the lowest 9 digits backwards.  These are
   * what fit in a 32 bit int.
   */
  work = orig_work = ull % BILLION;
  while(work) {
    digits++;
    *d-- = '0' + (work % 10);
    work = work / 10;
  }
  /*
   * If there was nothing there to start with and
   * nothing left to do then we're done.
   */
  ull = ull / BILLION;
  if(!ull && !orig_work) {
    if(zero_p)
      *d-- = '0';
    return d + 1;
  }

  /*
   * Are there more digits to fill in?
   */
  if(ull) {
    /*
     * Pad out to billions.
     */
    while(digits < 9) {
      digits++;
      *d-- = '0';
    }
    /*
     * Print the rest.
     */
    return print_ull_guts(d, ull, 0);
  } else {
    return d + 1;
  }
}


/**************************************************************************************/
/* unsstr64:  Prints a 64-bit number in decimal format. */

char* unsstr64(uns64 value) {
  static char uns64_buffer[MAX_SIMULTANEOUS_STRINGS][MAX_STR_LENGTH + 1];
  static int  counter = 0;
  char*       temp;

  counter = CIRC_INC2(counter, MAX_SIMULTANEOUS_STRINGS);
  uns64_buffer[counter][MAX_STR_LENGTH] = '\0';
  temp = print_ull_guts(&uns64_buffer[counter][MAX_STR_LENGTH - 1], value, 1);

  return temp;
}


/**************************************************************************************/
/* unsstr64c:  Prints a 64-bit number in decimal format with commas. */

char* unsstr64c(uns64 value) {
  static char uns64_buffer[MAX_SIMULTANEOUS_STRINGS][MAX_STR_LENGTH + 1];
  static int  counter = 0;
  char        buffer[MAX_STR_LENGTH + 1];
  char *      temp, *temp2, *temp3;
  uns         comma_count = 0;

  counter                = CIRC_INC2(counter, MAX_SIMULTANEOUS_STRINGS);
  buffer[MAX_STR_LENGTH] = '\0';
  uns64_buffer[counter][MAX_STR_LENGTH] = '\0';
  temp = print_ull_guts(&buffer[MAX_STR_LENGTH - 1], value, 1);

  for(temp2 = &buffer[MAX_STR_LENGTH - 1],
  temp3     = &uns64_buffer[counter][MAX_STR_LENGTH - 1];
      temp2 >= temp; temp2--, temp3--) {
    *temp3 = *temp2;
    comma_count++;
    if(comma_count == 3 && temp2 != temp) {
      temp3--;
      *temp3      = ',';
      comma_count = 0;
    }
  }
  return temp3 + 1;
}


/**************************************************************************************/
/* intstr64:  Prints a 64-bit number in decimal format. */

char* intstr64(int64 value) {
  char* temp;
  Flag  neg = FALSE;

  if(value < 0) {
    neg  = TRUE;
    temp = unsstr64(-value);
  } else
    temp = unsstr64(value);

  if(neg) {
    temp--;
    *temp = '-';
  }

  return temp;
}


/**************************************************************************************/
/* byte_mask_8_to_bit_mask_64 */

inline uns64 byte_mask_8_to_bit_mask_64(uns8 byte_mask) {
  uns64 bits = 0xff;
  uns64 rval = 0x0;

  for(; byte_mask; byte_mask >>= 1, bits <<= 8)
    if(bits & 0x1)
      rval |= bits;

  return rval;
}


/**************************************************************************************/
/* xor_fold_bits: fold a number onto itself such that it occupies n bits */

inline uns64 xor_fold_bits(uns64 src, uns n) {
  uns64 result = 0x0;
  uns   ii;
  ASSERT(0, n < 64);
  for(ii = 0; ii < 64; ii += n) {
    result ^= src & N_BIT_MASK(n);
    src >>= MIN2(n, 64 - ii - n);
  }

  return result;
}


/**************************************************************************************/
/* strin: "String in" -- Takes a string and an array of strings and returns the
   index if the string is in the list, -1 if not. */

int strin(const char* s0, const char* const sarray[], const uns size) {
  uns ii;

  for(ii = 0; ii < size; ii++)
    if(!strncmp(s0, sarray[ii], MAX_STR_LENGTH))
      return ii;
  return -1;
}


/**************************************************************************************/
/* log2_ctr: */

uns log2_ctr(Counter n) {
  uns power;

  for(power = 0; n >>= 1; power++)
    ;
  return power;
}


  /**************************************************************************************
   * cfprintf: This is a general function to print output in a "columnized"
   *format.
   *
   * It expects printf-style format strings and interprets them based on
   * a special set of cfprintf characters:
   *
   *     '&' - column divider (if followed immediately by a number, the
   * 	number determines the minimum column size.  Otherwise, whitespace is
   * 	always stripped from the beginning and end of the column.
   *
   *     '$'  - row terminator (ends the row and equates to a newline in the
   *printout)
   *
   *     '\n' - treated as a normal character (doesn't end a row)
   *
   ***************************************************************************************/

#include <stdarg.h>

#define MAX_LINE_CHARS 2048
#define BASE_MAX_TABLE_LINES 1024
#define MAX_SEP_CHARS 128
#define DEFAULT_COL_SEPARATOR "  "

void cfprintf(FILE* stream, const char* passed_format, ...) {
  char* format = passed_format == NULL ? NULL : strdup(passed_format);

  static Flag   in_table = 0;
  static int    lines;
  static int    max_table_lines;
  static char** strings;
  static char*  cur;
  char*         start;
  char*         stop;
  int           ii, jj;
  int           col_count;
  int*          col_widths;
  char**        col_separators;
  int*          col_justifies;
  va_list       ap;

  if(format == NULL) {
    if(!in_table) {
      fprintf(stream, "\n");
      return;
    }
    *cur = '\0'; /* just to be sure */

    /* figure out how many columns there are */
    col_count = 1;
    for(ii = 0; ii < strlen(strings[0]); ii++)
      if(strings[0][ii] == '&')
        col_count++;

    /* init column widths to 0 */
    col_widths     = (int*)calloc(col_count, sizeof(int));
    col_separators = (char**)malloc(sizeof(char*) * col_count);
    for(ii = 0; ii < col_count; ii++)
      col_separators[ii] = (char*)calloc(MAX_SEP_CHARS, sizeof(char));
    col_justifies = (int*)calloc(col_count, sizeof(int));

    /* strip spaces */
    for(ii = 0; ii < lines; ii++) {
      int cur_col = 0;
      start       = strings[ii]; /* write point */
      stop        = start;       /* read point */

    BEGIN_COLUMN:
      /* skip starting whitespace */
      while(*stop == ' ' || *stop == '\t')
        stop++;
      /* copy up until & or \0 */
      while(*stop != '&' && *stop != '\0')
        *(start++) = *(stop++);
      /* kill trailing whitespace */
      if(start > strings[ii] && (*(start - 1) == ' ' || *(start - 1) == '\t')) {
        start--;
        while(*start == ' ' || *start == '\t')
          start--;
        start++;
      }
      /* copy \0 or & */
      *(start++) = *(stop);
      /* if starting a new column */
      if(*stop == '&') {
        stop++;
        cur_col++; /* need to increment first because & "starts" a column */
        if(*stop == '-') { /* it's a left justify */
          col_justifies[cur_col] = 1;
          stop++;
        }
        if(*stop >= '0' && *stop <= '9') { /* it's a min width argument */
          col_widths[cur_col] = atoi(stop);
          while(*stop >= '0' && *stop <= '9')
            stop++;
        }
        if(*stop == '\'') { /* it's a non-default separator */
          char* temp = &col_separators[cur_col][0];
          while(*(++stop) != '\'') {
            *(temp++) = *stop;
            ASSERTU(0, temp - &col_separators[cur_col][0] < MAX_SEP_CHARS);
          }
          stop++;
        } else
          strcpy(col_separators[cur_col], DEFAULT_COL_SEPARATOR);

        ASSERTUM(0, cur_col < col_count, "cur_col:%d  col_count:%d\n", cur_col,
                 col_count);
      } else
        continue; /* next row if we've hit \0 */
      goto BEGIN_COLUMN;
    }

    /* find the maximum column widths */
    {
      int cur_col;
      for(ii = 0; ii < lines; ii++) {
        start   = strings[ii];
        stop    = start;
        cur_col = 0;

        while(*stop != '\0') {
          if(*stop == '&') {
            col_widths[cur_col] = MAX2(col_widths[cur_col], stop - start);
            cur_col++;
            ASSERTUM(0, cur_col < col_count, "cur_col:%d  col_count:%d\n",
                     cur_col, col_count);
            stop++;
            start = stop;
          } else
            stop++;
        }
        col_widths[cur_col] = MAX2(col_widths[cur_col], stop - start);
      }
    }

    /* print out all of the columns */
    for(ii = 0; ii < lines; ii++) {
      int cur_col = 0;
      start       = strings[ii];
      stop        = start;

      while(*stop != '\0') {
        if(*stop == '&') {
          *stop = '\0';
          ASSERT(0, strlen(start) <= col_widths[cur_col]);
          if(!col_justifies[cur_col])
            for(jj = 0; jj < col_widths[cur_col] - strlen(start); jj++)
              fprintf(stream, " ");
          fprintf(stream, "%s", start);
          if(col_justifies[cur_col])
            for(jj = 0; jj < col_widths[cur_col] - strlen(start); jj++)
              fprintf(stream, " ");
          if(col_widths[cur_col] > 0) /* ignore empty columns */
            fprintf(stream, "%s", col_separators[cur_col]);
          cur_col++;
          ASSERT(0, cur_col < col_count);
          start = stop + 1;
          stop  = start;
        } else
          stop++;
      }
      ASSERT(0, strlen(start) <= col_widths[cur_col]);
      for(jj = 0; jj < col_widths[cur_col] - strlen(start); jj++)
        fprintf(stream, " ");
      fprintf(stream, "%s\n", start);
    }

    free(col_widths);
    for(ii = 0; ii < col_count; ii++)
      free(col_separators[ii]);
    free(col_separators);
    free(col_justifies);

    /* de-allocate string memory */
    for(ii = 0; ii < max_table_lines; ii++)
      free(strings[ii]);
    free(strings);

    in_table = 0;
    return;
  }

  if(!in_table) {
    /* allocate string memory */
    max_table_lines = BASE_MAX_TABLE_LINES;
    strings         = (char**)calloc(BASE_MAX_TABLE_LINES, sizeof(char*));
    for(ii = 0; ii < BASE_MAX_TABLE_LINES; ii++)
      strings[ii] = (char*)calloc(MAX_LINE_CHARS, sizeof(char));

    /* new table */
    lines    = 0;
    cur      = strings[0];
    in_table = 1;
  }

  va_start(ap, passed_format);

  start = format;
  stop  = format;

  while(*stop != '\0') {
    if(*stop == '$') {
      *stop = '\0';
      vsprintf(cur, start, ap);
      ASSERT(0, cur < strings[lines] + MAX_LINE_CHARS);
      stop++;
      start = stop;

      if(lines == max_table_lines - 1) {
        /* reallocate string memory */
        /*  		printf("realloc (%d --> %d)\n", max_table_lines, max_table_lines
         * * 2); */
        char** new_strings;
        new_strings = (char**)calloc(max_table_lines * 2, sizeof(char*));
        for(ii = 0; ii < max_table_lines * 2; ii++)
          new_strings[ii] = (char*)calloc(MAX_LINE_CHARS, sizeof(char));

        for(ii = 0; ii < max_table_lines; ii++) {
          for(jj = 0; jj < MAX_LINE_CHARS; jj++)
            new_strings[ii][jj] = strings[ii][jj];
          free(strings[ii]);
        }

        free(strings);
        strings = new_strings;
        max_table_lines *= 2;
      }

      cur = strings[++lines];
      ASSERT(0, lines < max_table_lines);
    } else
      stop++;
  }
  /* write out the end of the string */
  vsprintf(cur, start, ap);
  cur += strlen(cur);
  ASSERT(0, strlen(strings[lines]) <= MAX_LINE_CHARS);

  free(format);
}


/**************************************************************************************/
// file_tag_fopen:

FILE* file_tag_fopen(char const* const dir, char const* const name,
                     char const* const mode) {
  char file_name[MAX_STR_LENGTH + 1];
  uns  len = strlen(FILE_TAG) + strlen(name) + strlen(".out") + 1;
  if(dir)
    len += strlen(dir) + 1;
  ASSERTM(0, len <= MAX_STR_LENGTH,
          "File name longer than MAX_STR_LENGTH (%d > %d)\n", len,
          MAX_STR_LENGTH);

  strncpy(file_name, "", MAX_STR_LENGTH);
  if(dir) {
    strncat(file_name, dir, MAX_STR_LENGTH);
    strncat(file_name, "/", MAX_STR_LENGTH);
  }
  strncat(file_name, FILE_TAG, MAX_STR_LENGTH);
  strncat(file_name, name, MAX_STR_LENGTH);
  strncat(file_name, ".out", MAX_STR_LENGTH);

  return fopen(file_name, mode);
}

/**************************************************************************************/
// factorial:

uns factorial(uns num) {
  ASSERT(0, num >= 0);
  return (num == 0 ? 1 : num * factorial(num - 1));
}

/**************************************************************************************/
// float compare:

Flag similar(float val1, float val2, float fudge_factor) {
  float diff = (val1 > val2) ? val1 - val2 : val2 - val1;

  if(diff < fudge_factor) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**************************************************************************************/
/* is_power_of_2 */

Flag is_power_of_2(uns64 x) {
  // relies on the fact that powers of 2 are single bit
  return (x != 0) && ((x & (x - 1)) == 0);
}

/**************************************************************************************/
/* convert_to_cmp_addr */

Addr convert_to_cmp_addr(uns8 proc_id, Addr addr) {
  if((addr & CMP_ADDR_MASK)) {
    addr = addr & ~CMP_ADDR_MASK;
  }

  return addr | (((Addr)proc_id) << 58);
}

/**************************************************************************************/
/* get_proc_id_from_cmp_addr */

uns get_proc_id_from_cmp_addr(Addr addr) {
  uns proc_id = addr >> 58;
  return proc_id;
}

/**************************************************************************************/
/* check_and_remove_addr_sign_extended_bits */

Addr check_and_remove_addr_sign_extended_bits(Addr virt_addr,
                                              uns  num_non_sign_extended_bits,
                                              Flag verify_bits_masked_out) {
  const uns  proc_id = get_proc_id_from_cmp_addr(virt_addr);
  const Addr mask    = CMP_ADDR_MASK | N_BIT_MASK(num_non_sign_extended_bits);

  // the bits we're masked out should be all 0s or all 1s. However, wrong path
  // or prefetch addresses might be non-sensical, so we should only check valid
  // right path addresses
  const Addr bits_masked_out = virt_addr & ~mask;
  Flag       all_0s_or_1s = (0 == bits_masked_out || mask == ~bits_masked_out);
  if(verify_bits_masked_out) {
    // we're calling this function expecting the addr to be valid, so we should
    // actually check
    ASSERT(proc_id, all_0s_or_1s);
  } else {
    // we're calling from addr_translate(), and it's possible for the addr to be
    // bad
    STAT_EVENT(proc_id, all_0s_or_1s ? GOOD_ADDRESS : KNOWN_BAD_ADDRESS);
  }

  return (virt_addr & mask);
}

/**************************************************************************************/
/* compare_uns64 */

int compare_uns64(const void* a, const void* b) {
  if(*(uns64*)a < *(uns64*)b)
    return -1;
  else if(*(uns64*)a > *(uns64*)b)
    return 1;
  else
    return 0;
}

/**************************************************************************************/
/* parse_array: parse comma-separated array, up to max_num, calling
   the parse_token() function on each token. */

static int parse_array(void* dest, const void* str, int max_num,
                       void (*parse_token)(void*, uns, const char*)) {
  ASSERT(0, strlen((const char*)str) < MAX_STR_LENGTH);
  const char* tok_start = (const char*)str;
  const char* tok_end   = (const char*)str;
  uns         idx       = 0;
  do {
    ASSERTM(0, idx < max_num, "Too many values in array\n");
    char buf[MAX_STR_LENGTH + 1];
    tok_end = strchr(tok_end, ',');
    if(!tok_end)
      tok_end = strchr(tok_start, 0);  // if no more commas, look for the end
    strncpy(buf, tok_start, tok_end - tok_start);
    buf[tok_end - tok_start] = 0;  // terminate the tok_start string
    parse_token(dest, idx, buf);
    tok_start = *tok_end ? ++tok_end :
                           0;  // skip the comma, unless we're at the end
    idx++;
  } while(tok_start);
  return idx;
}

/**************************************************************************************/
/* parse_int_token: parse string as an int and put it into destination array */

static void parse_int_token(void* dest, uns idx, const char* token) {
  int* array = (int*)dest;
  array[idx] = atoi(token);
}

/**************************************************************************************/
/* parse_int_array: parse comma-separated array of integers, up to max_num */

int parse_int_array(int dest[], const void* str, int max_num) {
  return parse_array(dest, str, max_num, parse_int_token);
}

/**************************************************************************************/
/* parse_uns_token: parse string as an uns and put it into destination array */

static void parse_uns_token(void* dest, uns idx, const char* token) {
  uns* array = (uns*)dest;
  array[idx] = atoi(token);
}

/**************************************************************************************/
/* parse_uns_array: parse comma-separated array of unsigned integers, up to
 * max_num */

int parse_uns_array(uns dest[], const void* str, int max_num) {
  return parse_array(dest, str, max_num, parse_uns_token);
}

/**************************************************************************************/
/* parse_uns64_token: parse string as an int and put it into destination array
 */

static void parse_uns64_token(void* dest, uns idx, const char* token) {
  uns64* array = (uns64*)dest;
  array[idx]   = atoll(token);
}

/**************************************************************************************/
/* parse_uns64_array: parse comma-separated array of integers, up to max_num */

int parse_uns64_array(uns64 dest[], const void* str, int max_num) {
  return parse_array(dest, str, max_num, parse_uns64_token);
}

/**************************************************************************************/
/* parse_float_token: parse string as float and put it into destination array */

static void parse_float_token(void* dest, uns idx, const char* token) {
  float* array = (float*)dest;
  array[idx]   = (float)atof(token);
}

/**************************************************************************************/
/* parse_float_array: parse comma-separated array of floats, up to max_num */

int parse_float_array(float dest[], const void* str, int max_num) {
  return parse_array(dest, str, max_num, parse_float_token);
}

/**************************************************************************************/
/* parse_double_token: parse string as double and put it into destination array
 */

static void parse_double_token(void* dest, uns idx, const char* token) {
  double* array = (double*)dest;
  array[idx]    = (double)atof(token);
}

/**************************************************************************************/
/* parse_double_array: parse comma-separated array of doubles, up to max_num */

int parse_double_array(double dest[], const void* str, int max_num) {
  return parse_array(dest, str, max_num, parse_double_token);
}

/**************************************************************************************/
/* parse_string_token: simply copy the string into the destination array */

static void parse_string_token(void* dest, uns idx, const char* token) {
  char(*array)[MAX_STR_LENGTH + 1] = (char(*)[MAX_STR_LENGTH + 1]) dest;
  strncpy(array[idx], token, MAX_STR_LENGTH);
}

/**************************************************************************************/
/* parse_string_array: parse comma-separated array of stringegers, up to max_num
 */

int parse_string_array(char dest[][MAX_STR_LENGTH + 1], const void* str,
                       int max_num) {
  return parse_array(dest, str, max_num, parse_string_token);
}
