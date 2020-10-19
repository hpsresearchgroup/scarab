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

#include "Memory.h"
#include <bitset>
#include <cassert>
#include <vector>
#include "Controller.h"
#include "DDR4.h"
#include "Request.h"

using namespace ramulator;

namespace ramulator {

template <>
vector<int> Memory<DDR4>::BaRaBgCo7ChCo2(Request& req, long addr) {
  int         bits_sliced = 0;
  vector<int> start_pos(addr_bits.size(), -1);

  // // we assume 10 column bits total and a prefetch width of 8, meaning the
  // low 3 column bits are ignored. Hence 10 - 3 = 7. We then split the
  // columns 2 and 5, with a channel bit (if required) in between
  const int assumed_column_bits           = 7;
  const int right_column_bits_split_width = 2;
  const int left_column_bits_split_width  = assumed_column_bits -
                                           right_column_bits_split_width;
  assert(addr_bits[int(DDR4::Level::Column)] == assumed_column_bits);
  req.addr_vec[int(DDR4::Level::Column)] =
    slice_lower_bits_and_track_num_shifted(addr, right_column_bits_split_width,
                                           bits_sliced);
  if(addr_bits[int(DDR4::Level::Channel)] > 0) {
    start_pos[int(DDR4::Level::Channel)] = bits_sliced;
    req.addr_vec[int(DDR4::Level::Channel)] =
      slice_lower_bits_and_track_num_shifted(
        addr, addr_bits[int(DDR4::Level::Channel)], bits_sliced);
  }

  req.addr_vec[int(DDR4::Level::Column)] |=
    (slice_lower_bits_and_track_num_shifted(addr, left_column_bits_split_width,
                                            bits_sliced)
     << right_column_bits_split_width);

  vector<DDR4::Level> levels{DDR4::Level::BankGroup, DDR4::Level::Rank,
                             DDR4::Level::Bank};
  for(auto level = levels.cbegin(); level != levels.cend(); level++) {
    int lev = int(*level);
    if(addr_bits[lev] > 0) {
      start_pos[lev]    = bits_sliced;
      req.addr_vec[lev] = slice_lower_bits_and_track_num_shifted(
        addr, addr_bits[lev], bits_sliced);
    }
  }

  req.addr_vec[int(DDR4::Level::Row)] = slice_row_addr(addr);

  return start_pos;
}

template <>
void Memory<DDR4>::set_skylakeddr4_addr_vec(Request& req, long addr) {
  vector<int>                          start_pos = BaRaBgCo7ChCo2(req, addr);
  std::bitset<sizeof(addr) * CHAR_BIT> addr_bitset(addr);

  if(addr_bits[int(DDR4::Level::Channel)] > 0) {
    int ch_start_pos = start_pos[int(DDR4::Level::Channel)];
    assert(ch_start_pos > 0);
    // couldn't figure out a pattern for which bits are XORed together for the
    // channel bit from the DRAMA paper, so not sure how to extrapolate it to
    // 2 or more channel bits
    assert(addr_bits[int(DDR4::Level::Channel)] == 1);
    req.addr_vec[int(DDR4::Level::Channel)] ^= (addr_bitset[ch_start_pos + 1] ^
                                                addr_bitset[ch_start_pos + 4] ^
                                                addr_bitset[ch_start_pos + 5] ^
                                                addr_bitset[ch_start_pos + 10] ^
                                                addr_bitset[ch_start_pos + 11]);
  }

  {
    int bg_start_pos = start_pos[int(DDR4::Level::BankGroup)];
    assert(bg_start_pos > 0);
    assert(addr_bits[int(DDR4::Level::BankGroup)] == 2);
    std::bitset<2> bg_bitset(req.addr_vec[int(DDR4::Level::BankGroup)]);
    bg_bitset[0] = bg_bitset[0] ^
                   addr_bitset[1];  // bit 7 in the complete addr, but since we
    // shift out the low 6 bits, it's now bit 1
    bg_bitset[1] = bg_bitset[1] ^ addr_bitset[bg_start_pos + 1 + 4];
    req.addr_vec[int(DDR4::Level::BankGroup)] = bg_bitset.to_ulong();
  }

  if(addr_bits[int(DDR4::Level::Rank)] > 0) {
    int ra_start_pos = start_pos[int(DDR4::Level::Rank)];
    assert(ra_start_pos > 0);
    assert(addr_bits[int(DDR4::Level::Rank)] == 1);
    req.addr_vec[int(DDR4::Level::Rank)] ^= addr_bitset[ra_start_pos + 4];
  }

  {
    int ba_start_pos = start_pos[int(DDR4::Level::Bank)];
    assert(ba_start_pos > 0);
    assert(addr_bits[int(DDR4::Level::Bank)] == 2);
    std::bitset<2> ba_bitset(req.addr_vec[int(DDR4::Level::Bank)]);
    ba_bitset[0] = ba_bitset[0] ^ addr_bitset[ba_start_pos + 4];
    ba_bitset[1] = ba_bitset[1] ^ addr_bitset[ba_start_pos + 1 + 4];
    req.addr_vec[int(DDR4::Level::Bank)] = ba_bitset.to_ulong();
  }
}


} /* namespace ramulator */
