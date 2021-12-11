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

#include "../scarab_markers.h"
#include "libc_qsort.h"

int32_t* A;
int32_t  N        = 1000000;
int32_t  elt_size = sizeof(int32_t);

#define DEBUG

int int32_compare(const void* A, const void* B, const void* arg) {
  const int32_t* A_ptr  = A;
  const int32_t* B_ptr  = B;
  int32_t        A_data = *A_ptr;
  int32_t        B_data = *B_ptr;
  return A_data < B_data ? -1 : (A_data == B_data) ? 0 : 1;
}

int main(void) {
  printf("Starting Main\n");

  srand(42);
  A = malloc(elt_size * N);

  for(uint32_t i = 0; i < N; ++i) {
    A[i] = (uint32_t)rand();
  }
  fflush(stdout);

  scarab_begin();
  libc_qsort(A, N, elt_size, int32_compare, NULL);
  scarab_end();
}
