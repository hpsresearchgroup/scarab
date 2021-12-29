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
import re

from scarab_globals import *
from scarab_globals.scarab_batch_types import *

# Set the following variables before running.
CHECKPOINT_DESCRIPTOR_PATH = ''
OUTPUT_DIR = ''
SUITE_NAME = ''

def checkpoint_start_rip(path):
  with open (path + '/main') as f:
    for line in f:
      m = re.match('\s*rip (0x[\da-f]+)', line.strip())
      if not m: continue
      return int(m.group(1), 16)
  
  raise RuntimeError(f'Did not find a value corresponding to the start rip in the checkpoint at {path}')

def scarab_inst_limit(scarab_args):
  m = re.search('--inst_limit ([\d]+)', scarab_args)
  assert m
  return int(m.group(1))

def trace_line(name, path, scarab_args, weight):
  return f'{name} = Trace("{name}", "{path}", scarab_args="{scarab_args}", weight={weight})\n'

def benchmark_inp_line(benchmark):
  return f'{benchmark.name} = Benchmark("{benchmark.name}", [{", ".join(c.name for c in benchmark.exec_list)}], weight={benchmark.weight})\n\n'

def benchmark_line(benchmark):
  return f'{benchmark.name} = Benchmark("{benchmark.name}", [{", ".join(b.name for b in benchmark.exec_list)}])\n\n'

def suite_line(suite):
  return f'{suite.name} = Suite("{suite.name}", [{", ".join(b.name for b in suite.exec_list)}])\n\n'

def trace_command(checkpoint_path, trace_path, inst_limit, start_rip):
  return Command(f'{scarab_paths.checkpoint_loader_bin} --run_external_pintool {checkpoint_path} {scarab_paths.pin_trace} --pintool_args "-start_rip {hex(start_rip)} -trace_len {inst_limit} -o {trace_path} "')

class FakeScarabRun:
  def __init__(self, cmd_list):
    self.cmd_list = cmd_list
    scarab_run_manager.register(self)

  def create_snapshot(self):
    return

  def make(self):
    return

  def get_commands(self):
    return self.cmd_list

  def process_command_list(self):
    return self.get_commands()

  def print_progress(self):
    assert False

  def get_stats(self, flat=False):
    assert False

  def print_commands(self):
    self.get_commands()
    for cmd in self.cmd_list:
      print(cmd)

import_descriptor(CHECKPOINT_DESCRIPTOR_PATH)
suite = globals()[SUITE_NAME]

output_descriptor_txt = ''
cmd_list = []

for benchmark in suite.exec_list:
  assert isinstance(benchmark, Benchmark)
  for benchmark_inp in benchmark.exec_list:
    assert isinstance(benchmark_inp, Benchmark)
    for checkpoint in benchmark_inp.exec_list:
      assert isinstance(checkpoint, Checkpoint)
      start_rip = checkpoint_start_rip(checkpoint.path)
      trace_path = f'{OUTPUT_DIR}/{os.path.basename(checkpoint.path)}_scarab_trace.bz2'
      output_descriptor_txt += trace_line(checkpoint.name, trace_path, checkpoint.scarab_args, checkpoint.weight)
      inst_limit = scarab_inst_limit(checkpoint.scarab_args)
      cmd_list.append(trace_command(checkpoint.path, trace_path, inst_limit, start_rip))
    output_descriptor_txt += benchmark_inp_line(benchmark_inp)

for benchmark in suite.exec_list:
  output_descriptor_txt += benchmark_line(benchmark)

output_descriptor_txt += suite_line(suite)

os.makedirs(OUTPUT_DIR, exist_ok=True)
with open(f'{OUTPUT_DIR}/descriptor.def', 'w') as f:
  f.write(output_descriptor_txt)

for cmd in cmd_list:
  print(cmd)

FakeScarabRun(cmd_list)
BatchManager(processor_cores_per_node=20)