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
// #include "Request.h"

using namespace ramulator;

namespace ramulator {

template <>
void Memory<DDR4>::add_to_xored_row_bits_pos(int bit_pos, bool initialized) {
  if(!initialized) {
    if(bit_pos >= addr_bits_start_pos[int(DDR4::Level::Row)]) {
      xored_row_bits_pos.push_back(bit_pos);
    }
  }
}

template <>
void Memory<DDR4>::BaRaBgCo7ChCo2(vector<int>& addr_vec, long addr) {
  int bits_sliced = 0;

  // // we assume 10 column bits total and a prefetch width of 8, meaning the
  // low 3 column bits are ignored. Hence 10 - 3 = 7. We then split the
  // columns 2 and 5, with a channel bit (if required) in between
  const int assumed_column_bits           = 7;
  const int right_column_bits_split_width = 2;
  const int left_column_bits_split_width  = assumed_column_bits -
                                           right_column_bits_split_width;
  int level = int(DDR4::Level::Column);
  assert(addr_bits[level] == assumed_column_bits);
  addr_vec[level] = slice_lower_bits_and_track_num_shifted(
    addr, right_column_bits_split_width, bits_sliced, level);

  level = int(DDR4::Level::Channel);
  if(addr_bits[level] > 0) {
    addr_vec[level] = slice_lower_bits_and_track_num_shifted(
      addr, addr_bits[level], bits_sliced, level);
  }

  level = int(DDR4::Level::Column);
  addr_vec[level] |= (slice_lower_bits_and_track_num_shifted(
                        addr, left_column_bits_split_width, bits_sliced, level)
                      << right_column_bits_split_width);

  vector<DDR4::Level> remaining_levels{DDR4::Level::BankGroup,
                                       DDR4::Level::Rank, DDR4::Level::Bank};
  for(auto lev = remaining_levels.cbegin(); lev != remaining_levels.cend();
      lev++) {
    level = int(*lev);
    if(addr_bits[level] > 0) {
      addr_vec[level] = slice_lower_bits_and_track_num_shifted(
        addr, addr_bits[level], bits_sliced, level);
    }
  }

  level           = int(DDR4::Level::Row);
  addr_vec[level] = slice_row_addr(addr, bits_sliced);
}

template <>
void Memory<DDR4>::set_skylakeddr4_addr_vec(vector<int>& addr_vec, long addr) {
  BaRaBgCo7ChCo2(addr_vec, addr);
  std::bitset<sizeof(addr) * CHAR_BIT> addr_bitset(addr);
  static bool                          initialized = false;

  if(addr_bits[int(DDR4::Level::Channel)] > 0) {
    int ch_start_pos = addr_bits_start_pos[int(DDR4::Level::Channel)];
    assert(ch_start_pos > 0);
    // couldn't figure out a pattern for which bits are XORed together for the
    // channel bit from the DRAMA paper, so not sure how to extrapolate it to
    // 2 or more channel bits
    assert(addr_bits[int(DDR4::Level::Channel)] == 1);
    if(channel_xor_bits_pos.empty()) {
      assert(!initialized);
      channel_xor_bits_pos = vector<int>(
        {ch_start_pos, ch_start_pos + 1, ch_start_pos + 4, ch_start_pos + 5,
         ch_start_pos + 10, ch_start_pos + 11});
      assert(fixed_channel_xor_bits_pos.empty());
      assert(free_channel_xor_bits_pos.empty());

      int row_start_pos = addr_bits_start_pos[int(DDR4::Level::Row)];
      assert(row_start_pos >= 0);
      for(auto channel_xor_bit_pos : channel_xor_bits_pos) {
        if((channel_xor_bit_pos < 6) ||
           (channel_xor_bit_pos >= row_start_pos)) {
          fixed_channel_xor_bits_pos.push_back(channel_xor_bit_pos);
        } else {
          free_channel_xor_bits_pos.push_back(channel_xor_bit_pos);
        }
      }
    }

    addr_vec[int(DDR4::Level::Channel)] = 0;
    for(auto channel_xor_bit_pos : channel_xor_bits_pos) {
      addr_vec[int(DDR4::Level::Channel)] ^= (addr_bitset[channel_xor_bit_pos]);
    }
  }

  {
    int bg_start_pos = addr_bits_start_pos[int(DDR4::Level::BankGroup)];
    if(!initialized) {
      assert(bg_start_pos > 6);
      assert(bg_start_pos < addr_bits_start_pos[int(DDR4::Level::Row)]);
      assert(addr_bits[int(DDR4::Level::BankGroup)] == 2);
    }
    std::bitset<2> bg_bitset(addr_vec[int(DDR4::Level::BankGroup)]);
    bg_bitset[0] = bg_bitset[0] ^
                   addr_bitset[1];  // bit 7 in the complete addr, but since we
    // shift out the low 6 bits, it's now bit 1
    int bg1_high_xor_pos = bg_start_pos + 1 + stride_to_upper_xored_bit;
    bg_bitset[1]         = bg_bitset[1] ^ addr_bitset[bg1_high_xor_pos];
    addr_vec[int(DDR4::Level::BankGroup)] = bg_bitset.to_ulong();
    if(!initialized) {
      assert(xored_row_bits_pos.empty());
      add_to_xored_row_bits_pos(bg1_high_xor_pos, initialized);
    }
  }

  if(addr_bits[int(DDR4::Level::Rank)] > 0) {
    int ra_start_pos = addr_bits_start_pos[int(DDR4::Level::Rank)];
    if(!initialized) {
      assert(ra_start_pos > 0);
      assert(ra_start_pos < addr_bits_start_pos[int(DDR4::Level::Row)]);
      assert(addr_bits[int(DDR4::Level::Rank)] == 1);
    }
    int ra_high_xor_pos = ra_start_pos + stride_to_upper_xored_bit;
    addr_vec[int(DDR4::Level::Rank)] ^= addr_bitset[ra_high_xor_pos];
    add_to_xored_row_bits_pos(ra_high_xor_pos, initialized);
  }

  {
    int ba_start_pos = addr_bits_start_pos[int(DDR4::Level::Bank)];
    if(!initialized) {
      assert(ba_start_pos > 0);
      assert(ba_start_pos < addr_bits_start_pos[int(DDR4::Level::Row)]);
      assert(addr_bits[int(DDR4::Level::Bank)] == 2);
    }
    std::bitset<2> ba_bitset(addr_vec[int(DDR4::Level::Bank)]);
    int            ba0_high_xor_pos = ba_start_pos + stride_to_upper_xored_bit;
    int            ba1_high_xor_pos = ba0_high_xor_pos + 1;
    ba_bitset[0] = ba_bitset[0] ^ addr_bitset[ba0_high_xor_pos];
    ba_bitset[1] = ba_bitset[1] ^ addr_bitset[ba1_high_xor_pos];
    addr_vec[int(DDR4::Level::Bank)] = ba_bitset.to_ulong();
    add_to_xored_row_bits_pos(ba0_high_xor_pos, initialized);
    add_to_xored_row_bits_pos(ba1_high_xor_pos, initialized);
  }
  initialized = true;
}


} /* namespace ramulator */
