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

/*utils.h
 * 04/17/19
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <cstdint>
#include <getopt.h>
#include <iostream>
#include <set>
#include <string>

#define PG_SIZE (1ULL << 12)
#define MAX_PG_SIZE (1ULL << 21)
#define ROUND_UP_TO_PAGE_BOUNDARY(address, page_size) \
  ((address + page_size - 1) & ~(page_size - 1))
#define PAGE_ALIGNED(address, page_size) ((address & (page_size - 1)) == 0)

typedef uint64_t ADDR;

// void fatal(const char* fmt, ...);
void vfatal(const char* fmt, ...);
void fatal_and_kill_child(pid_t child_pid, const char* fmt, ...);
void debug(const char* fmt, ...);
void assertm(bool p, const char* message);

void print_string_array(const char* name, const char* const str_array[]);
int  count_longest_option_length(const struct option long_options[]);
void turn_aslr_off();

#ifndef DEBUG_EN
#define DEBUG_EN 1
#endif

#if DEBUG_EN == 1
#define DEBUG(x)                 \
  do {                           \
    std::cout << x << std::endl; \
  } while(0)
#else
#define DEBUG(x) \
  do {           \
  } while(0)
#endif

struct AddressRange {
  ADDR inclusive_lower_bound;
  ADDR exclusive_upper_bound;

  AddressRange(ADDR lower, ADDR upper) :
      inclusive_lower_bound(lower), exclusive_upper_bound(upper) {
    assertm(inclusive_lower_bound < exclusive_upper_bound,
            "Must specify valid range!\n");
  }

  bool     operator<(const AddressRange& rhs) const;
  bool     operator<(const ADDR& rhs) const;
  bool     operator>(const ADDR& rhs) const;
  bool     contains(const ADDR& rhs) const;
  uint64_t size() const;
};

// TODO: add a boolean to indicate anonymous mapping
struct RegionInfo {
  AddressRange range;
  int          prot;
  ADDR         offset;
  std::string  file_name;

  RegionInfo() : range(0, 1), prot(0), offset(0) {}
  RegionInfo(ADDR _start_address, ADDR _end_address, int _prot,
             uint32_t _offset, std::string _file_name) :
      range(_start_address, _end_address),
      prot(_prot), offset(_offset), file_name(_file_name) {}
};

inline std::ostream& operator<<(std::ostream&     os,
                                const RegionInfo& region_info) {
  return os << "Start: " << std::hex << region_info.range.inclusive_lower_bound
            << ", End: " << region_info.range.exclusive_upper_bound
            << ", Offset: " << region_info.offset
            << ", Protection: " << region_info.prot
            << ", path: " << region_info.file_name;
}

class FreeList {
 private:
  typedef std::set<AddressRange> free_list_type;
  static const ADDR              init_lower_bound;
  static const ADDR              init_upper_bound;
  static const uint64_t          padding;
  std::set<AddressRange>         free_list;

 public:
  FreeList();
  void allocate_range(const AddressRange& range);
  ADDR find_free_region(uint64_t size, ADDR start, ADDR end) const;
  ADDR find_free_region(uint64_t size) const;
};

#endif
