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
* File         : globals/global_types.h
* Author       : HPS Research Group
* Date         : 10/15/1997
* Description  : Global type declarations intended to be included in every
source file. Be really careful about putting stuff in here.  You don't want to
        have to recompile the entire simulator every time you want to change
        a type.
***************************************************************************************/

#ifndef __GLOBAL_TYPES_H__
#define __GLOBAL_TYPES_H__

/**************************************************************************************/

/* Forward declarations of typedefs. They are in this file to avoid
   typedef re-definition. Typedef re-definition is actually OK in gcc 4.4+,
   but since the base HPS gcc is only 4.1.2, we are using this solution. */
typedef struct Inst_Info_struct     Inst_Info;
typedef struct Mem_Req_struct       Mem_Req;
typedef struct Op_Info_struct       Op_Info;
typedef struct Op_struct            Op;
typedef struct Pb_Data_struct       Pb_Data;
typedef struct Ports_struct         Ports;
typedef struct Pref_Mem_Req_struct  Pref_Mem_Req;
typedef struct Stream_Buffer_struct Stream_Buffer;
typedef struct Table_Info_struct    Table_Info;
typedef struct HWP_struct           HWP;
typedef struct HWP_Info_struct      HWP_Info;

/* Renames -- Try to use these rather than built-in C types in order to preserve
 * portability */
typedef unsigned           uns;
typedef unsigned char      uns8;
typedef unsigned short     uns16;
typedef unsigned           uns32;
typedef unsigned long long uns64;
typedef char               int8;
typedef short              int16;
typedef int                int32;
typedef int long long      int64;
typedef int                Generic_Enum;


/* Conventions */
typedef uns64 Addr;
typedef uns32 Binary;
typedef uns8  Flag;

typedef uns64 Counter;
typedef int64 SCounter;


/* Alpha types */
typedef int64 Quad;
typedef int32 Long;
typedef int16 Word;
typedef int8  Byte;
typedef uns64 UQuad;
typedef uns32 ULong;
typedef uns16 UWord;
typedef uns8  UByte;


/**************************************************************************************/

#endif /* #ifndef __GLOBAL_TYPES_H__ */
