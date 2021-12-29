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

###################################################################################
# Private Variables: Do not use directly. Values may be overwritten
#   by environment variables below.
#
# Modify each temporary variable below to contain the default path for each item.
###################################################################################
__pin_dir__        = "/misc/pool1/pin/pinplay-drdebug-3.5-pin-3.5-97503-gac534ca30-gcc-linux"
__mcpat_bin__      = "/misc/pool1/mcpat/mcpat_v1.0/mcpat"
__cacti_bin__      = "/misc/pool1/mcpat/cacti65/cacti"
__scarab_bin_dir__ = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
__scarab_sim_dir__ = os.path.dirname(__scarab_bin_dir__)

###################################################################################
# Public Variables: Use the values below to get the path to each item in other
#   python scripts.
#
# DO NOT MODIFY BELOW
#   Paths below are computed using either the default value from above, or
#   environment variables.
###################################################################################

sim_dir                 = os.environ['SIMDIR']    if 'SIMDIR'    in os.environ else __scarab_sim_dir__
pin_dir                 = os.environ['PIN_ROOT']  if 'PIN_ROOT'  in os.environ else __pin_dir__
mcpat_bin               = os.environ['MCPAT_BIN'] if 'MCPAT_BIN' in os.environ else __mcpat_bin__
cacti_bin               = os.environ['CACTI_BIN'] if 'CACTI_BIN' in os.environ else __cacti_bin__

utils_dir               = sim_dir + "/utils"
bin_dir                 = sim_dir + "/bin"
src_dir                 = sim_dir + "/src"

scarab_bin              = src_dir + "/scarab"
pin_exec_dir            = src_dir + "/pin/pin_exec"
pin_bin                 = pin_exec_dir   + "/obj-intel64/pin_exec.so"
pin_trace_dir           = src_dir + "/pin/pin_trace"
pin_trace               = pin_trace_dir  + "/obj-intel64/gen_trace.so"

checkpoint_creator_dir  = utils_dir + "/checkpoint/creator"
checkpoint_loader_dir   = utils_dir + "/checkpoint/loader"
checkpoint_loader_bin   = utils_dir + "/checkpoint/loader/checkpoint_loader"
