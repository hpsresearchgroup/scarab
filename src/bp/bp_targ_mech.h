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
 * File         : bp/bp_targ_mech.h
 * Author       : HPS Research Group
 * Date         : 12/9/1998
 * Description  :
 ***************************************************************************************/
#ifndef __BP_TARG_MECH_H__
#define __BP_TARG_MECH_H__

#include "bp/bp.h"
#include "op.h"


/**************************************************************************************/
/* Local prototypes */

Addr bp_crs_pop(Bp_Data*, Op*);
void bp_crs_push(Bp_Data*, Op*);
void bp_crs_recover(Bp_Data*);
Addr bp_crs_realistic_pop(Bp_Data*, Op*);
void bp_crs_realistic_push(Bp_Data*, Op*);
void bp_crs_realistic_recover(Bp_Data*, Recovery_Info*);

void  bp_btb_gen_init(Bp_Data*);
Addr* bp_btb_gen_pred(Bp_Data*, Op*);
void  bp_btb_gen_update(Bp_Data*, Op*);

void bp_ibtb_tc_tagged_init(Bp_Data*);
Addr bp_ibtb_tc_tagged_pred(Bp_Data*, Op*);
void bp_ibtb_tc_tagged_update(Bp_Data*, Op*);
void bp_ibtb_tc_tagged_recover(Bp_Data*, Recovery_Info*);

void bp_ibtb_tc_tagless_init(Bp_Data*);
Addr bp_ibtb_tc_tagless_pred(Bp_Data*, Op*);
void bp_ibtb_tc_tagless_update(Bp_Data*, Op*);
void bp_ibtb_tc_tagless_recover(Bp_Data*, Recovery_Info*);

void bp_ibtb_tc_hybrid_init(Bp_Data*);
Addr bp_ibtb_tc_hybrid_pred(Bp_Data*, Op*);
void bp_ibtb_tc_hybrid_update(Bp_Data*, Op*);
void bp_ibtb_tc_hybrid_recover(Bp_Data*, Recovery_Info*);


/**************************************************************************************/

#endif /* #ifndef __BP_TARG_MECH_H__ */
