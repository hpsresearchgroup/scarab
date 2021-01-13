/**
 * @file cbp_to_scarab.h
 * @author Stephen Pruett (stephen.pruett@utexas.edu)
 * @brief Convert CBP interface to Scarab Interface
 * @version 0.1
 * @date 2021-01-12
 *
 * The goal of this file is to make transitioning between CBP to Scarab as
 * painless as possible. This file was written with CBP 2016 in mind.
 *
 * USAGE:
 *   1. Add CBP files to scarab
 *      a. Download files from CBP (predictor.h and predictor.cc) and place them
 * in src/bp/ directory. b. Rename files to more descriptive name (e.g., mtage.h
 * and mtage.cc) c. Search and replace all occurances of PREDICTOR with new
 * name (e.g., s/PREDICTOR/MTAGE/g) d. Rename any conflicting names (e.g.,
 * ASSERT)
 *   2. Add the CBP header file to cbp_to_scarab.cpp
 *   3. Add entry for CBP predictor in cbp_table.def (make sure names do not
 * conflict)
 */

#ifndef __CBP_TO_SCARAB_H__
#define __CBP_TO_SCARAB_H__

// Random libraries that may be useful
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include "stdint.h"
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "core.param.h"
#include "globals/assert.h"
#include "globals/utils.h"
#include "table_info.h"

#ifdef __cplusplus
}

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Library needed for Scarab Interface
#include "bp/bp.h"

#define SCARAB_BP_INTF_FUNC(CBP_CLASS, FCN_NAME) bp_##CBP_CLASS##_##FCN_NAME

/*************Interface to Scarab***************/
#define DEF_CBP(CBP_NAME, CBP_CLASS)                         \
  void SCARAB_BP_INTF_FUNC(CBP_CLASS, init)();               \
  void SCARAB_BP_INTF_FUNC(CBP_CLASS, timestamp)(Op * op);   \
  uns8 SCARAB_BP_INTF_FUNC(CBP_CLASS, pred)(Op*);            \
  void SCARAB_BP_INTF_FUNC(CBP_CLASS, spec_update)(Op * op); \
  void SCARAB_BP_INTF_FUNC(CBP_CLASS, update)(Op * op);      \
  void SCARAB_BP_INTF_FUNC(CBP_CLASS, retire)(Op * op);      \
  void SCARAB_BP_INTF_FUNC(CBP_CLASS, recover)(Recovery_Info*);
#include "cbp_table.def"
#undef DEF_CBP

#ifdef __cplusplus
}
#endif

/*************CPB 2016 UTILS**********************/

#define UINT32 unsigned int
#define INT32 int
#define UINT64 unsigned long long
#define COUNTER unsigned long long

#define NOT_TAKEN 0
#define TAKEN 1

#define FAILURE 0
#define SUCCESS 1

typedef enum {
  OPTYPE_OP = 2,

  OPTYPE_RET_UNCOND,
  OPTYPE_JMP_DIRECT_UNCOND,
  OPTYPE_JMP_INDIRECT_UNCOND,
  OPTYPE_CALL_DIRECT_UNCOND,
  OPTYPE_CALL_INDIRECT_UNCOND,

  OPTYPE_RET_COND,
  OPTYPE_JMP_DIRECT_COND,
  OPTYPE_JMP_INDIRECT_COND,
  OPTYPE_CALL_DIRECT_COND,
  OPTYPE_CALL_INDIRECT_COND,

  OPTYPE_ERROR,

  OPTYPE_MAX
} OpType;

static inline UINT32 SatIncrement(UINT32 x, UINT32 max) {
  if(x < max)
    return x + 1;
  return x;
}

static inline UINT32 SatDecrement(UINT32 x) {
  if(x > 0)
    return x - 1;
  return x;
}

static inline Flag is_conditional_branch(Op* op) {
  return op->table_info->cf_type == CF_CBR;
}

static inline OpType scarab_to_cbp_optype(Op* op) {
  Cf_Type cf_type = op->table_info->cf_type;
  OpType  optype  = OPTYPE_OP;

  switch(cf_type) {
    case CF_BR:
      optype = OPTYPE_JMP_DIRECT_UNCOND;
      break;
    case CF_CALL:
      optype = OPTYPE_CALL_DIRECT_UNCOND;
      break;
    case CF_CBR:
      optype = OPTYPE_JMP_DIRECT_COND;
      break;
    case CF_IBR:
      optype = OPTYPE_JMP_INDIRECT_UNCOND;
      break;
    case CF_ICALL:
      optype = OPTYPE_CALL_INDIRECT_UNCOND;
      break;
    case CF_ICO:
      optype = OPTYPE_CALL_INDIRECT_UNCOND;
      break;
    case CF_RET:
      optype = OPTYPE_RET_UNCOND;
      break;
    case CF_SYS:
      optype = OPTYPE_CALL_INDIRECT_UNCOND;
      break;
    default:
      // Should never see non-control flow instructions or invalid CF
      // types in the branch predictor.
      ASSERT(op->proc_id, 0);
      break;
  }
  return optype;
}

#endif