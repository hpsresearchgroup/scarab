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
 * File         : libs/malloc_lib.h
 * Author       : HPS Research Group
 * Date         : 3/14/2000
 * Description  : A faster malloc for small repetitive allocations
 ***************************************************************************************/

#ifndef __MALLOC_LIB_H__
#define __MALLOC_LIB_H__

typedef struct SMalloc_Raw_struct {
  char* ptr;
  int   cur_size;
} SMalloc_Raw;

typedef struct SMalloc_Entry_struct {
  void*                        data;
  struct SMalloc_Entry_struct* next;
} SMalloc_Entry;

void* smalloc(int nbytes);
void  sfree(int nbytes, void* item);

#endif /* #ifndef __MALLOC_LIB_H__ */
