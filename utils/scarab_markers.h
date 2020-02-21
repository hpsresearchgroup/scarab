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

#ifndef __SCARAB_MARKERS_H__
#define __SCARAB_MARKERS_H__

#include <stdint.h>
#include <stdio.h>

#define COMPILER_BARRIER() \
  { __asm__ __volatile__("" ::: "memory"); }

/* Note: If these values change, must update
 * the macros in src/pin/pin_exec/* as well. */
#define SCARAB_MARKERS_PIN_BEGIN (1)
#define SCARAB_MARKERS_PIN_END (2)

static inline void scarab_marker(uint64_t op) {
  COMPILER_BARRIER();
  __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
  COMPILER_BARRIER();
}

static inline void scarab_begin() {
  printf("Scarab: Starting Simulation\n");
  scarab_marker(SCARAB_MARKERS_PIN_BEGIN);
}

static inline void scarab_end() {
  scarab_marker(SCARAB_MARKERS_PIN_END);
  printf("Scarab: Ending simulation\n");
}

#endif
