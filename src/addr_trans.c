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
 * File         : addr_trans.c
 * Author       : HPS Research Group
 * Date         : 10/28/2012
 * Description  : "Fake" virtual to physical address translation. Uses a hash
 *function, and does not maintain page tables. Used to randomize DRAM bank
 *mappings.
 ***************************************************************************************/

#include "addr_trans.h"
#include "debug/debug_macros.h"
#include "globals/utils.h"
#include "memory/memory.param.h"
#include "ramulator.param.h"

#define DEBUG(proc_id, args...) _DEBUG(proc_id, DEBUG_ADDR_TRANS, ##args)

DEFINE_ENUM(Addr_Translation, ADDR_TRANSLATION_LIST);

static uns32 hsieh_hash(const char* data, int len);

/**************************************************************************************/
/* addr_translate: translate virtual address to physical address */

Addr addr_translate(Addr virt_addr) {
  Addr masked_virt_addr = check_and_remove_addr_sign_extended_bits(
    virt_addr, ADDR_NON_SIGN_EXTEND_NUM_BITS);

  if(ADDR_TRANSLATION == ADDR_TRANS_NONE)
    return masked_virt_addr;

  /* Generate DRAM bank bits by hashing all address bits */
  uns   num_page_offset_bits = LOG2(MEMORY_INTERLEAVE_FACTOR);
  Addr  page_index           = virt_addr >> num_page_offset_bits;
  uns   num_bank_bits        = LOG2(RAMULATOR_BANKS * RAMULATOR_CHANNELS);
  uns32 bank_bits            = page_index & N_BIT_MASK(num_bank_bits);
  Addr  hash_source;

  if(ADDR_TRANSLATION == ADDR_TRANS_RANDOM ||
     ADDR_TRANSLATION == ADDR_TRANS_FLIP) {
    hash_source = page_index;
  } else if(ADDR_TRANSLATION == ADDR_TRANS_PRESERVE_BLP ||
            ADDR_TRANSLATION == ADDR_TRANS_PRESERVE_STREAM) {
    /* excluding bank bits from hash source will preserve
       bank-level parallelism among requests with the same upper
       bits */
    hash_source = page_index >> num_bank_bits;
  } else {
    FATAL_ERROR(0, "Unknown ADDR_TRANSLATION: %s\n",
                Addr_Translation_str(ADDR_TRANSLATION));
  }
  uns32 hash;
  if(ADDR_TRANSLATION == ADDR_TRANS_FLIP) {
    hash = hash_source ^ N_BIT_MASK(sizeof(uns32) * CHAR_BIT);
  } else {
    hash = hsieh_hash((char*)&hash_source, sizeof(Addr));
  }
  uns32 new_bank_bits = (hash & N_BIT_MASK(num_bank_bits));
  if(ADDR_TRANSLATION == ADDR_TRANS_PRESERVE_BLP) {
    new_bank_bits ^= bank_bits;
  } else if(ADDR_TRANSLATION == ADDR_TRANS_PRESERVE_STREAM) {
    new_bank_bits ^= bank_bits;
    Addr top_bank_bit = (page_index >> (num_bank_bits - 1)) & 1;
    CLRBIT(new_bank_bits, num_bank_bits - 1);
    new_bank_bits |= top_bank_bit << (num_bank_bits - 1);
  }

  /* Construct the physical address subject to two constraints:
     1. the address should retain proc_id in the upper bits
     2. no two page indices should map to the same frame number (otherwise such
        collisions artifically reduce the application's working set) */
  uns  proc_id                = get_proc_id_from_cmp_addr(virt_addr);
  Addr page_offset            = virt_addr & N_BIT_MASK(num_page_offset_bits);
  Addr orig_virt_addr         = convert_to_cmp_addr(0, virt_addr);
  Addr orig_page_index        = orig_virt_addr >> num_page_offset_bits;
  Addr orig_hashed_page_index = (orig_page_index &
                                 (~N_BIT_MASK(num_bank_bits))) |
                                new_bank_bits;
  Addr orig_hashed_addr = ((Addr)bank_bits << (sizeof(uns32) * CHAR_BIT)) |
                          (orig_hashed_page_index << num_page_offset_bits) |
                          page_offset;

  Addr hashed_addr = convert_to_cmp_addr(proc_id, orig_hashed_addr);
  DEBUG(proc_id, "%llx => %llx\n", virt_addr, hashed_addr);
  return hashed_addr;
}

  /**************************************************************************************
   * The code below was adapted from
   *http://www.azillionmonkeys.com/qed/hash.html
   **************************************************************************************/

#if !defined(get16bits)
#define get16bits(d) \
  ((((uns32)(((const uns8*)(d))[1])) << 8) + (uns32)(((const uns8*)(d))[0]))
#endif

uns32 hsieh_hash(const char* data, int len) {
  uns32 hash = len, tmp;
  int   rem;

  if(len <= 0 || data == NULL)
    return 0;

  rem = len & 3;
  len >>= 2;

  /* Main loop */
  for(; len > 0; len--) {
    hash += get16bits(data);
    tmp  = (get16bits(data + 2) << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    data += 2 * sizeof(uns16);
    hash += hash >> 11;
  }

  /* Handle end cases */
  switch(rem) {
    case 3:
      hash += get16bits(data);
      hash ^= hash << 16;
      hash ^= ((signed char)data[sizeof(uns16)]) << 18;
      hash += hash >> 11;
      break;
    case 2:
      hash += get16bits(data);
      hash ^= hash << 11;
      hash += hash >> 17;
      break;
    case 1:
      hash += (signed char)*data;
      hash ^= hash << 10;
      hash += hash >> 1;
  }

  /* Force "avalanching" of final 127 bits */
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;

  return hash;
}
