#  Copyright 2020 HPS/SAFARI Research Groups
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy of
#  this software and associated documentation files (the "Software"), to deal in
#  the Software without restriction, including without limitation the rights to
#  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
#  of the Software, and to permit persons to whom the Software is furnished to do
#  so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.

import os
import sys

from scarab_globals import *
from scarab_globals.scarab_batch_types import *

import_descriptor(scarab_paths.utils_dir + "/qsort/scarab_batch_descriptor.def")

#SystemConfig.submission_system = command.SubmissionSystems.PBS

baseline_params = ScarabParams(params_file=scarab_paths.sim_dir + "/src/PARAMS.kaby_lake")
baseline_params += "--heartbeat 1000"
perfect_bp_params="--perfect_bp 1"
perfect_params = baseline_params + perfect_bp_params

ScarabRun("Baseline", suite1, baseline_params, results_dir="./results")
ScarabRun("Perfect",  suite1, perfect_params,  results_dir="./results")

BatchManager()