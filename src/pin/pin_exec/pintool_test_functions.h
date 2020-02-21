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

#ifndef _PINTOOL_TEST_FUNCTIONS
#define _PINTOOL_TEST_FUNCTIONS

#include "../pin_lib/pin_scarab_common_lib.h"
#include "pin_fe_globals.h"
#include "rollback_structs.h"

//#define TESTING_RETIRE false
//#define TESTING_RECOVER false
//#define TESTING_REDIRECT false
//#define TESTING_EXCEPTION false
//#define TESTING_RETIRE_ILLOP false

#if TESTING_RETIRE || TESTING_REDIRECT || TESTING_RECOVER || TESTING_EXCEPTION

bool skip_main_loop_retire = false;
bool do_retire_test        = false;
bool do_redirect_test      = false;
bool do_recover_test       = false;
bool found_illop           = false;

int redirect_count = 10;
int retire_count   = 10;
int recover_count  = 10;

ADDRINT test_addr;
UINT64  test_uid;

/* ===================================================================== */

Scarab_To_Pin_Msg test_fake_scarab() {
  Scarab_To_Pin_Msg cmd;
  cmd.type = FE_FETCH_OP;

  if(TESTING_RETIRE && do_retire_test && retire_count > 0) {
    cmd.type       = FE_RETIRE;
    cmd.inst_uid   = test_uid;
    do_retire_test = false;
    retire_count--;
  }
  if(TESTING_REDIRECT && do_redirect_test && redirect_count > 0) {
    cmd.type         = FE_REDIRECT;
    cmd.inst_addr    = test_addr;
    cmd.inst_uid     = test_uid;
    do_redirect_test = false;
    redirect_count--;
  }
  if(TESTING_RECOVER && do_recover_test && recover_count > 0) {
    cmd.type        = FE_RECOVER_AFTER;
    cmd.inst_uid    = test_uid;
    do_recover_test = false;
    recover_count--;
  }
  if(TESTING_RETIRE_ILLOP && found_illop) {
    cmd.type     = FE_RETIRE;
    cmd.inst_uid = uid_ctr - 1;
    found_illop  = false;
  }
  if(TESTING_REDIRECT && redirect_count == 0) {
    on_wrongpath = false;
  }

  return cmd;
}

/* ===================================================================== */

VOID test_recover_analysis(bool save) {
  if(!fast_forward_count) {
    if((heartbeat & 0xFFUL) == 0) {
      printf("Size of checkpoints %" PRIi64 " \n", checkpoints_cir_buf_size);
    }

    if(save) {
      test_recover_uid = uid_ctr;
    } else {
      do_recover_test = true;
      test_uid        = test_recover_uid;
      // recover_to_past_checkpoint(test_recover_uid);
    }

    heartbeat++;
  }
}

/* ===================================================================== */

VOID test_redirect_analysis(bool save, ADDRINT eip, CONTEXT* ctxt) {
  if((heartbeat & 0xFFUL) == 0) {
    printf("Size of checkpoints %" PRIi64 " \n", checkpoints_cir_buf_size);
  }

  if(save) {
    saved_eip = eip;
  } else {
    do_redirect_test = true;
    test_addr        = saved_eip;
    test_uid         = uid_ctr;
    // redirect_to_inst(saved_eip, ctxt);
  }

  heartbeat++;
}


/* ===================================================================== */

VOID test_retire_analysis() {
  printf("Size of checkpoints %" PRIi64 " \n", checkpoints_cir_buf_size);

  do_retire_test = true;
  test_uid       = uid_ctr - 1;
  // retire_older_checkpoints(uid_ctr-1);

  heartbeat++;
}

/* ===================================================================== */

VOID TestRetire(INS ins, VOID* v) {
  if(INS_IsXchg(ins) && INS_OperandCount(ins) == 2 &&
     INS_OperandIsReg(ins, 0) && INS_OperandIsReg(ins, 1)) {
    if((INS_OperandReg(ins, 0) == REG_EAX) &&
       (INS_OperandReg(ins, 1) == REG_EAX)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)test_retire_analysis,
                     IARG_END);
    }
  }
}

/* ===================================================================== */

VOID TestRecover(INS ins, VOID* v) {
  if(INS_IsXchg(ins) && INS_OperandCount(ins) == 2 &&
     INS_OperandIsReg(ins, 0) && INS_OperandIsReg(ins, 1)) {
    if((INS_OperandReg(ins, 0) == REG_EAX) &&
       (INS_OperandReg(ins, 1) == REG_EAX)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)test_recover_analysis,
                     IARG_BOOL, true, IARG_END);
    } else if((INS_OperandReg(ins, 0) == REG_EBX) &&
              (INS_OperandReg(ins, 1) == REG_EBX)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)test_recover_analysis,
                     IARG_BOOL, false, IARG_END);
    }
  }
}

/* ===================================================================== */

VOID TestRedirect(INS ins, VOID* v) {
  if(INS_IsXchg(ins) && INS_OperandCount(ins) == 2 &&
     INS_OperandIsReg(ins, 0) && INS_OperandIsReg(ins, 1)) {
    if((INS_OperandReg(ins, 0) == REG_EAX) &&
       (INS_OperandReg(ins, 1) == REG_EAX)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)test_redirect_analysis,
                     IARG_BOOL, true, IARG_INST_PTR, IARG_CONTEXT, IARG_END);
    } else if((INS_OperandReg(ins, 0) == REG_EBX) &&
              (INS_OperandReg(ins, 1) == REG_EBX)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)test_redirect_analysis,
                     IARG_BOOL, false, IARG_INST_PTR, IARG_CONTEXT, IARG_END);
    }
  }
}


  /* ===================================================================== */
  /* ===================================================================== */

#endif
#endif
