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
* File         : globals/enum.h
* Author       : HPS Research Group
* Date         : 04/10/2012
* Description  : Utility macros that automatically generate enum
* declarations, string arrays, and relevant functions for enumerations

Introduction
------------

This enum library makes using enums easier. It supplies the following
boilerplate code for enum types:

1. the enum declaration itself, with every element prefixed by a specified
prefix
2. a function that takes an enum element and returns it as a string
3. a function that takes a string and returns the corresponding enum element
4. a function used by param_parser.c to handle parameters of this enum type

When using this library, we no longer have to maintain two copies of the enum
list (the enum declaration in the .h file and the strings array in the .c file)
and we do not require extra .def files.

To add or remove an enum element, we only need to add or remove a single line of
code. Hence, we avoid potential bugs that may happen if the enum and the string
array are maintained separately.

The library has a limit of 40 elements per enum, which can be extended if
needed.

Example
-------

Suppose we want a Pref_Aggr enum that has values PREF_AGGR_MILD,
PREF_AGGR_MEDIUM, and PREF_AGGR_HIGH.

We declare the enum in the .h file like so:

    #define PREF_AGGR_LIST(ELEM)                    \
        ELEM(MILD)                                  \
        ELEM(MEDIUM)                                \
        ELEM(HIGH)                                  \

    DECLARE_ENUM(Pref_Aggr, PREF_AGGR_LIST, PREF_AGGR_);

This declaration expands to the enum declaration and two function prototypes:

    typedef enum Pref_Aggr_enum {
        PREF_AGGR_MILD,
        PREF_AGGR_MEDIUM,
        PREF_AGGR_HIGH,
        PREF_AGGR_NUM_ELEMS
    } Pref_Aggr;

    const char *Pref_Aggr_str(Enum_Aggr); // convert enum element to string
(without the prefix) Enum_Aggr Pref_Aggr_parse(const char*); // parse enum
element from string (case-insentive)

We define the enum in the .c file (which should #include the .h file) like so:

    DEFINE_ENUM(Pref_Aggr, PREF_AGGR_LIST)

This expands to the enum string array and the definitions of the above
functions.

Details
-------

Do not define any macros starting with "ENUM_", since they may conflict with the
many macros defined in this library.

For an enum "Example" with prefix "PREFIX_", the library introduces the
following names into the global namespace:

    enum Example_enum
    Example
    Example_str
    Example_parse
    get_Example_param
    PREFIX_NUM_ELEMS
    Example_num_elems

This code was initially adapted from http://stackoverflow.com/a/202511 . I added
the macro linking to enable automatic enum element prefixing.
***************************************************************************************/

#ifndef __ENUM_H__
#define __ENUM_H__

#include <string.h>
#include "globals/utils.h"

/**************************************************************************************/
/* Macros */

/// @cond
/// above statement hides these internal macros in Doxygen docs

// expansion macro for enum values
#define ENUM_VALUE(prefix, name) prefix##name,

// expansion macro for enumeration item string
#define ENUM_STR(name) #name,

#define ENUM_CONCAT(a, b) a##b
#define ENUM_XCONCAT(a, b) ENUM_CONCAT(a, b)

#define ENUM_LINK0_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK1_##rest
#define ENUM_LINK1_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK2_##rest
#define ENUM_LINK2_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK3_##rest
#define ENUM_LINK3_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK4_##rest
#define ENUM_LINK4_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK5_##rest
#define ENUM_LINK5_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK6_##rest
#define ENUM_LINK6_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK7_##rest
#define ENUM_LINK7_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK8_##rest
#define ENUM_LINK8_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK9_##rest
#define ENUM_LINK9_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK10_##rest
#define ENUM_LINK10_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK11_##rest
#define ENUM_LINK11_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK12_##rest
#define ENUM_LINK12_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK13_##rest
#define ENUM_LINK13_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK14_##rest
#define ENUM_LINK14_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK15_##rest
#define ENUM_LINK15_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK16_##rest
#define ENUM_LINK16_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK17_##rest
#define ENUM_LINK17_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK18_##rest
#define ENUM_LINK18_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK19_##rest
#define ENUM_LINK19_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK20_##rest
#define ENUM_LINK20_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK21_##rest
#define ENUM_LINK21_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK22_##rest
#define ENUM_LINK22_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK23_##rest
#define ENUM_LINK23_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK24_##rest
#define ENUM_LINK24_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK25_##rest
#define ENUM_LINK25_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK26_##rest
#define ENUM_LINK26_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK27_##rest
#define ENUM_LINK27_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK28_##rest
#define ENUM_LINK28_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK29_##rest
#define ENUM_LINK29_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK30_##rest
#define ENUM_LINK30_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK31_##rest
#define ENUM_LINK31_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK32_##rest
#define ENUM_LINK32_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK33_##rest
#define ENUM_LINK33_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK34_##rest
#define ENUM_LINK34_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK35_##rest
#define ENUM_LINK35_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK36_##rest
#define ENUM_LINK36_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK37_##rest
#define ENUM_LINK37_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK38_##rest
#define ENUM_LINK38_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK39_##rest
#define ENUM_LINK39_ENUM_APPLY(MACRO, prefix, name, rest) \
  MACRO(prefix, name) ENUM_LINK40_##rest
#define ENUM_LINK40_ENUM_APPLY(MACRO, prefix, name, rest) \
  ENUM_ERROR_TOO_MANY_ELEMENTS = (1 / 0),

#define ENUM_LINK0_ENUM_LAST(x)
#define ENUM_LINK1_ENUM_LAST(x)
#define ENUM_LINK2_ENUM_LAST(x)
#define ENUM_LINK3_ENUM_LAST(x)
#define ENUM_LINK4_ENUM_LAST(x)
#define ENUM_LINK5_ENUM_LAST(x)
#define ENUM_LINK6_ENUM_LAST(x)
#define ENUM_LINK7_ENUM_LAST(x)
#define ENUM_LINK8_ENUM_LAST(x)
#define ENUM_LINK9_ENUM_LAST(x)
#define ENUM_LINK10_ENUM_LAST(x)
#define ENUM_LINK11_ENUM_LAST(x)
#define ENUM_LINK12_ENUM_LAST(x)
#define ENUM_LINK13_ENUM_LAST(x)
#define ENUM_LINK14_ENUM_LAST(x)
#define ENUM_LINK15_ENUM_LAST(x)
#define ENUM_LINK16_ENUM_LAST(x)
#define ENUM_LINK17_ENUM_LAST(x)
#define ENUM_LINK18_ENUM_LAST(x)
#define ENUM_LINK19_ENUM_LAST(x)
#define ENUM_LINK20_ENUM_LAST(x)
#define ENUM_LINK21_ENUM_LAST(x)
#define ENUM_LINK22_ENUM_LAST(x)
#define ENUM_LINK23_ENUM_LAST(x)
#define ENUM_LINK24_ENUM_LAST(x)
#define ENUM_LINK25_ENUM_LAST(x)
#define ENUM_LINK26_ENUM_LAST(x)
#define ENUM_LINK27_ENUM_LAST(x)
#define ENUM_LINK28_ENUM_LAST(x)
#define ENUM_LINK29_ENUM_LAST(x)
#define ENUM_LINK30_ENUM_LAST(x)
#define ENUM_LINK31_ENUM_LAST(x)
#define ENUM_LINK32_ENUM_LAST(x)
#define ENUM_LINK33_ENUM_LAST(x)
#define ENUM_LINK34_ENUM_LAST(x)
#define ENUM_LINK35_ENUM_LAST(x)
#define ENUM_LINK36_ENUM_LAST(x)
#define ENUM_LINK37_ENUM_LAST(x)
#define ENUM_LINK38_ENUM_LAST(x)
#define ENUM_LINK39_ENUM_LAST(x)
#define ENUM_LINK40_ENUM_LAST(x)

#define ENUM_DECLARE_ELEM_PART1(prefix) ENUM_APPLY(ENUM_VALUE,prefix,
#define ENUM_DECLARE_ELEM_PART2(name) name,
#define ENUM_DECLARE_CLOSE_BRACKET(name) )

/// below statement re-enables Doxygen for the rest of the file
/// @endcond

/** Declare the access function and define enum values */
#define DECLARE_ENUM(Enum_Type, ENUM_LIST, prefix)                     \
  typedef enum Enum_Type##_enum{ENUM_XCONCAT(                          \
    ENUM_LINK0_,                                                       \
    ENUM_LIST(ENUM_DECLARE_ELEM_PART1(prefix) ENUM_DECLARE_ELEM_PART2) \
      ENUM_LAST(ENUM_DUMMY) ENUM_LIST(ENUM_DECLARE_CLOSE_BRACKET))     \
                                  prefix##NUM_ELEMS} Enum_Type;        \
  enum { Enum_Type##_num_elems = prefix##NUM_ELEMS };                  \
  const char* Enum_Type##_str(Enum_Type value);                        \
  Enum_Type   Enum_Type##_parse(const char* str);                      \
  void        get_##Enum_Type##_param(const char* str, Enum_Type* val);

/** Define the access function names and functions to print and parse
    the enum */
#define DEFINE_ENUM(Enum_Type, ENUM_LIST)                                   \
  static const char* Enum_Type##_names[] = {ENUM_LIST(ENUM_STR)};           \
  const char*        Enum_Type##_str(Enum_Type value) {                     \
    return enum_str(Enum_Type##_names, value, Enum_Type##_num_elems, \
                    #Enum_Type);                                     \
  }                                                                         \
  Enum_Type Enum_Type##_parse(const char* str) {                            \
    return (Enum_Type)enum_parse(Enum_Type##_names, str,                    \
                                 Enum_Type##_num_elems, #Enum_Type);        \
  }                                                                         \
  void get_##Enum_Type##_param(const char* str, Enum_Type* val) {           \
    get_enum_param(str, Enum_Type##_names, val, Enum_Type##_num_elems,      \
                   #Enum_Type);                                             \
  }


/**************************************************************************************/
/* Prototypes */

const char* enum_str(const char* enum_strs[], size_t value, size_t num,
                     const char* enum_type_name);
size_t      enum_parse(const char* enum_strs[], const char* str, size_t num,
                       const char* enum_type_name);
void get_enum_param(const char* param_name, const char* enum_strs[], uns* value,
                    size_t num, const char* enum_type_name);

#endif  // __ENUM_H__
