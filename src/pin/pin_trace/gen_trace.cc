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

/*BEGIN_LEGAL
BSD License

Copyright (c)2013 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */

//
// @ORIGINAL_AUTHORS: Harish Patil
//

#include <iostream>
#include <stddef.h>
#include <stdio.h>
#include <string>

#include "pin.H"
#include "pinplay.H"

#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab

#include "../pin_lib/decoder.h"

#undef UNUSED   // there is a name conflict between PIN and Scarab
#undef WARNING  // there is a name conflict between PIN and Scarab

#include "../../ctype_pin_inst.h"
#include "../../table_info.h"

std::vector<std::string> iclass_prints;

#define KNOB_LOG_NAME "log"
#define KNOB_REPLAY_NAME "replay"
#define KNOB_FAMILY "pintool:pinplay-driver"

PINPLAY_ENGINE pinplay_engine;

// Knobs required to replay a pinball
KNOB<BOOL> KnobPinPlayLogger(KNOB_MODE_WRITEONCE, "pintool", "log", "0",
                             "Activate the pinplay logger");
KNOB<BOOL> KnobPinPlayReplayer(KNOB_MODE_WRITEONCE, "pintool", "replay", "0",
                               "Activate the pinplay replayer");

// Knobs that control trace generation
KNOB<string> Knob_output(KNOB_MODE_WRITEONCE, "pintool", "o", "trace.bz2",
                         "trace outputfilename");

/*** globals ***/
FILE* output_stream;

ctype_pin_inst mailbox;
bool           mailbox_full = false;

/*** function definitions ***/
INT32 Usage() {
  std::cerr << "This pin tool creates a trace that Scarab Trace frontend\n";
  std::cerr << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

LOCALFUN VOID Fini(int n, void* v) {
  pin_decoder_print_unknown_opcodes();
  if(output_stream) {
    if(mailbox_full) {
      fwrite(&mailbox, sizeof(mailbox), 1, output_stream);
    }
    pclose(output_stream);
  }
}

void dump_instruction() {
  ctype_pin_inst* info = pin_decoder_get_latest_inst();
  if(mailbox_full) {
    mailbox.instruction_next_addr = info->instruction_addr;
    fwrite(&mailbox, sizeof(mailbox), 1, output_stream);
  }
  mailbox      = *info;
  mailbox_full = true;
}

void insert_instrumentation(TRACE trace, void* v) {
  for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      pin_decoder_insert_analysis_functions(ins);
      if(output_stream) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)dump_instruction, IARG_END);
      }
    }
  }
}

int main(int argc, char* argv[]) {
  if(PIN_Init(argc, argv)) {
    return Usage();
  }

  pinplay_engine.Activate(argc, argv, KnobPinPlayLogger, KnobPinPlayReplayer);

  if(!Knob_output.Value().empty()) {
    char popename[1024];
    sprintf(popename, "/usr/bin/bzip2 > %s", Knob_output.Value().c_str());
    output_stream = popen(popename, "w");
  } else {
    cout << "No trace specified. Only verifying opcodes." << endl;
  }

  pin_decoder_init(true, &std::cerr);

  TRACE_AddInstrumentFunction(insert_instrumentation, 0);
  PIN_AddFiniFunction(Fini, 0);
  PIN_StartProgram();

  return 0;
}
