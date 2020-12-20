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

/*utils.c
 * 4/17/19
 */

#include "utils.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <stdarg.h>
#include <sys/personality.h>

void vfatal(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);

  fprintf(stderr, "fatal: ");
  vfprintf(stderr, fmt, va);
  fprintf(stderr, "\n");
  va_end(va);
  exit(1);
}

void fatal_and_kill_child(pid_t child_pid, const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);

  if(0 != child_pid) {
    kill(child_pid, SIGKILL);
  }

  vfatal(fmt, va);
  va_end(va);
}

void debug(const char* fmt, ...) {
  if(DEBUG_EN) {
    va_list va;
    va_start(va, fmt);

    fprintf(stdout, "DEBUG: ");
    vfprintf(stdout, fmt, va);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(va);
  }
}

void assertm(bool p, const char* message) {
  if(!p) {
    printf("Assert Failed: %s\n", message);
    exit(1);
  }
}

void print_string_array(const char* name, const char* const str_array[]) {
  int i = 0;
  while(NULL != str_array[i]) {
    printf("%s[%d] = %s\n", name, i, str_array[i]);
    i++;
  }
}

int count_longest_option_length(const struct option long_options[]) {
  assert(NULL != long_options);

  int longest_option_length = -1;
  for(int i = 0; NULL != long_options[i].name; i++) {
    int length = strlen(long_options[i].name);
    if(length > longest_option_length) {
      longest_option_length = length;
    }
  }

  assert(longest_option_length > -1);
  return longest_option_length;
}

void turn_aslr_off() {
  const int current_persona = personality(0xffffffff);
  if(-1 == current_persona) {
    vfatal("could not get the current personality");
  }
  [[maybe_unused]] int ret_val = personality(current_persona |
                                             ADDR_NO_RANDOMIZE);
  assert(current_persona == ret_val);
}

bool AddressRange::operator<(const AddressRange& rhs) const {
  // Upper bound is not included in range.
  return exclusive_upper_bound <= rhs.inclusive_lower_bound;
}

bool AddressRange::operator<(const ADDR& rhs) const {
  // Upper bound is not included in range.
  return exclusive_upper_bound <= rhs;
}

bool AddressRange::operator>(const ADDR& rhs) const {
  // Upper bound is not included in range.
  return inclusive_lower_bound > rhs;
}

bool AddressRange::contains(const ADDR& rhs) const {
  // Upper bound is not included in range.
  return inclusive_lower_bound <= rhs && rhs < exclusive_upper_bound;
}

uint64_t AddressRange::size() const {
  return exclusive_upper_bound - inclusive_lower_bound;
}

// Valid User space region
const ADDR FreeList::init_lower_bound = PG_SIZE;
// const ADDR     FreeList::init_upper_bound = 0x0000000080000000; //highest
// address that will compile
const ADDR     FreeList::init_upper_bound = 0x0000800000000000;
const uint64_t FreeList::padding          = (0x1ULL << 36);

FreeList::FreeList() {
  AddressRange initial_range(init_lower_bound, init_upper_bound);
  free_list.insert(initial_range);
}

void FreeList::allocate_range(const AddressRange& range) {
  assertm(free_list.count(range), "Allocating range that is not free.");
  free_list_type::iterator free_range_it = free_list.find(range);
  AddressRange             free_range    = *free_range_it;

  free_list.erase(free_range_it);

  if(free_range.inclusive_lower_bound != range.inclusive_lower_bound) {
    AddressRange lower_range(free_range.inclusive_lower_bound,
                             range.inclusive_lower_bound);
    free_list.insert(lower_range);
  }

  if(range.exclusive_upper_bound != free_range.exclusive_upper_bound) {
    AddressRange upper_range(range.exclusive_upper_bound,
                             free_range.exclusive_upper_bound);
    free_list.insert(upper_range);
  }
}

// size is in bytes
ADDR FreeList::find_free_region(uint64_t size) const {
  return find_free_region(size, 0, 0);
}

// size is in bytes
ADDR FreeList::find_free_region(uint64_t size, ADDR start, ADDR end) const {
  start = (start == 0) ? init_lower_bound : start + padding;
  end   = (end == 0) ? init_upper_bound : end - padding;

  for(free_list_type::iterator b = free_list.begin(), e = free_list.end();
      b != e; ++b) {
    // Never allocate the first region.
    // if (b == free_list.begin()) continue;

    // Find range after the start address.
    if((*b) < start)
      continue;

    // Check if we have passed the end address.
    if((*b) > end)
      break;

    ADDR range_start = b->contains(start) ? start : b->inclusive_lower_bound;
    ADDR page_aligned_range_start = ROUND_UP_TO_PAGE_BOUNDARY(range_start,
                                                              MAX_PG_SIZE);
    ADDR range_end      = b->contains(end) ? end : b->exclusive_upper_bound;
    uint64_t range_size = range_end - page_aligned_range_start;

    if(!b->contains(page_aligned_range_start))
      continue;

    if(range_size >= size) {
      return page_aligned_range_start;
    }
  }

  assertm(0, "No free region found!\n");
  return 0;
}
