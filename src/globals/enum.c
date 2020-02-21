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
 * File         : globals/enum.c
 * Author       : HPS Research Group
 * Date         : 04/10/2012
 * Description  : Utility macros that automatically generate enum
 * declarations, string arrays, and relevant functions for enumerations
 ***************************************************************************************/

#include "globals/enum.h"
#include <strings.h> /* for strcasecmp */
#include <unistd.h>  /* for optarg */

const char* enum_str(const char* enum_strs[], size_t value, size_t num,
                     const char* enum_type_name) {
  if(value >= num)
    FATAL_ERROR(0, "Unknown %s enum value %ld\n", enum_type_name,
                (long int)value);
  return enum_strs[value];
}

size_t enum_parse(const char* enum_strs[], const char* str, size_t num,
                  const char* enum_type_name) {
  size_t i;
  for(i = 0; i < num; ++i) {
    if(strcasecmp(enum_strs[i], str) == 0)
      return i;
  }
  FATAL_ERROR(0, "Could not match \"%s\" to an element of %s enum\n", str,
              enum_type_name);
}

void get_enum_param(const char* param_name, const char* enum_strs[], uns* var,
                    size_t num, const char* enum_type_name) {
  if(optarg)
    *var = enum_parse(enum_strs, optarg, num, enum_type_name);
  else
    WARNINGU(0, "Parameter '%s' missing value --- Ignored.\n", param_name);
}
