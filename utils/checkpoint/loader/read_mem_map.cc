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

/*4/23/19
 */

#include "read_mem_map.h"
#include <cstring>
#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include "utils.h"

struct pageTableStruct {
  std::vector<RegionInfo> entries;

  void write_entry(uint64_t addr_b, uint64_t addr_e, uint8_t perm,
                   uint64_t offset, std::string file_name);
  void clear();
  void update_page_table(std::ifstream& ptable_file);
  std::vector<RegionInfo> get_page_table_entries() const { return entries; };
};

void pageTableStruct::write_entry(uint64_t addr_b, uint64_t addr_e,
                                  uint8_t perm, uint64_t offset,
                                  std::string file_name) {
  RegionInfo new_e(addr_b, addr_e, perm, offset, file_name);
  entries.push_back(new_e);
}

void pageTableStruct::clear() {
  entries.clear();
}

void pageTableStruct::update_page_table(std::ifstream& ptable_file) {
  clear();
  std::string line;
  while(getline(ptable_file, line)) {
    std::string delimiter;
    std::string range;
    std::string perm;
    std::string offset;
    std::string file_name;

    char*    line_copy = strdup(line.c_str());
    char*    token     = strtok(line_copy, " \t\n\r\f\v");
    uint32_t token_id  = 0;
    while(token != NULL) {
      switch(token_id) {
        case 0:
          range = std::string(token);
          break;
        case 1:
          perm = std::string(token);
          break;
        case 2:
          offset = std::string(token);
          break;
        case 5:
          file_name = std::string(token);
          break;
        default:
          break;
      }

      token = strtok(NULL, " \t\n\r\f\v");
      token_id++;
    }
    free(line_copy);


    // parse strings
    delimiter       = "-";
    int32_t     pos = range.find(delimiter);
    std::string r1  = range.substr(0, pos);
    range.erase(0, pos + delimiter.length());
    std::string r2 = range;

    const char* temp     = perm.c_str();
    uint8_t     perm_val = 0;
    if(temp[0] == 'r')
      perm_val |= PROT_READ;
    if(temp[1] == 'w')
      perm_val |= PROT_WRITE;
    if(temp[2] == 'x')
      perm_val |= PROT_EXEC;
    write_entry(strtoul(r1.c_str(), 0, 16), strtoul(r2.c_str(), 0, 16),
                perm_val, strtoul(offset.c_str(), 0, 16), file_name);
  }
}

std::vector<RegionInfo> read_proc_maps_file(pid_t pid) {
  pageTableStruct page_table;
  std::ifstream   ifs;
  std::string filename = std::string("/proc/") + std::to_string(pid) + "/maps";

  ifs.open(filename, std::ifstream::in);
  page_table.update_page_table(ifs);
  return page_table.get_page_table_entries();
}

/*
int test_main (void) {
  std::vector<RegionInfo> v = read_proc_maps_file(25052);

  for (std::vector<RegionInfo>::iterator b = v.begin(), e = v.end(); b != e;
++b) { printf("%llx-%llx %x %llx %s\n", b->range.inclusive_lower_bound,
b->range.exclusive_upper_bound, b->prot, b->offset, b->file_name.c_str());
  }

  return 0;
}
*/
