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

#ifndef PIN_EXEC_READ_MEM_MAP_H__
#define PIN_EXEC_READ_MEM_MAP_H__

#include <algorithm>
#include <cassert>
#include <fstream>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

using namespace std;

struct pageTableEntryStruct {
  uint64_t addr_begin;
  uint64_t addr_end;
  uint8_t  permissions;

  bool writtenToOnRightPath;

  string path;
  string procMapsLine;

  pageTableEntryStruct() {
    addr_begin           = 0;
    addr_end             = 0;
    permissions          = 0;
    writtenToOnRightPath = false;
  }

  pageTableEntryStruct(uint64_t a, uint64_t b, uint8_t c, string _path,
                       string _procMapsLine) {
    addr_begin           = a;
    addr_end             = b;
    permissions          = c;
    path                 = _path;
    procMapsLine         = _procMapsLine;
    writtenToOnRightPath = false;
  }

  bool operator<(const pageTableEntryStruct& entry) const {
    return (addr_end <= entry.addr_begin);
  }

  bool operator==(const pageTableEntryStruct& entry) const {
    if(procMapsLine == entry.procMapsLine) {
      assert(addr_begin == entry.addr_begin);
      assert(addr_end == entry.addr_end);
      assert(permissions == entry.permissions);
      assert(path == entry.path);
      return true;
    } else {
      return false;
    }
  }
};


struct pageTableStruct {
  vector<pageTableEntryStruct> entries;

  bool get_entry(uint64_t address, pageTableEntryStruct** p_p_entry) {
    auto overlapping_entries = equal_range(
      entries.begin(), entries.end(),
      pageTableEntryStruct(address, address + 1, 0, "", ""));

    if(overlapping_entries.first != overlapping_entries.second) {
      *p_p_entry = overlapping_entries.first;
      // make sure only one overlaping entry

      if((overlapping_entries.first + 1) != overlapping_entries.second) {
        cout << "ERROR: multiple matching entries for address 0x" << hex
             << address << endl;
        for(auto entry = overlapping_entries.first;
            entry != overlapping_entries.second; ++entry) {
          cout << "\tMatching entry: 0x" << hex << entry->addr_begin << "-0x"
               << hex << entry->addr_end << endl;
        }
      }

      assert((overlapping_entries.first + 1) == overlapping_entries.second);
      return true;
    }

    return false;
  }

  void write_entry(uint64_t addr_b, uint64_t addr_e, uint8_t perm, string _path,
                   string _procMapsLine) {
    pageTableEntryStruct new_e(addr_b, addr_e, perm, _path, _procMapsLine);

    // check for duplicates
    auto overlapping_entries = equal_range(entries.begin(), entries.end(),
                                           new_e);
    if(overlapping_entries.first == overlapping_entries.second) {
      // no existing entry
      entries.push_back(new_e);
    } else {
      // make sure only 1 existing entry
      assert((overlapping_entries.first + 1) == overlapping_entries.second);
      assert(*(overlapping_entries.first) == new_e);
    }
  }

  void print() {
    for(uint32_t i = 0; i < entries.size(); i++) {
      printf("0x%" PRIx64 " 0x%" PRIx64 " %" PRIx8 "\n", entries[i].addr_begin,
             entries[i].addr_end, entries[i].permissions);
    }
  }

  void clear() { entries.clear(); }
};


inline void update_page_table(pageTableStruct* ptable) {
  ptable->clear();
  ifstream* ptable_file = new ifstream("/proc/self/maps");
  string    line;
  string    delimiter;
  while(getline(*ptable_file, line)) {
    string orig_line = line;

    delimiter    = " ";
    int    pos   = line.find(delimiter);
    string range = line.substr(0, pos);
    line.erase(0, pos + delimiter.length());

    pos         = line.find(delimiter);
    string perm = line.substr(0, pos);

    line.erase(0, pos + delimiter.length());
    size_t found = line.find_first_of("/[");  // either find a path that
                                              // starts with '/', or
                                              // something that starts liek
                                              //'[' like '[stack]' or
                                              //'[vdso]
    string path;
    if(found != std::string::npos) {
      path = line.substr(found, line.length());
    }

    delimiter = "-";
    pos       = range.find(delimiter);
    string r1 = range.substr(0, pos);
    range.erase(0, pos + delimiter.length());
    string r2 = range;

    const char* temp     = perm.c_str();
    uint8_t     perm_val = 0;
    if(temp[0] == 'r')
      perm_val = perm_val + 4;
    if(temp[1] == 'w')
      perm_val = perm_val + 2;
    if(temp[2] == 'x')
      perm_val = perm_val + 1;
    ptable->write_entry(strtoul(r1.c_str(), 0, 16), strtoul(r2.c_str(), 0, 16),
                        perm_val, path, orig_line);
  }

  ptable_file->clear();
}

#endif  // PIN_EXEC_READ_MEM_MAP_H__
