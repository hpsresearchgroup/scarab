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

"""
Author: HPS Research Group
Date: 10/21/2019
Description: Declares important objects for scarab_batch

Scarab Batch Objects:
  -CheckPoint: {name, path, pintool_args}, specifies a checkpoint to be run
  -Program: {name, path, pintool_args, copy, run_cmd}, specifies a program to be run
  -Benchmark: A collection of checkpoints and/or programs to be run as separate scarab_launch jobs.
  -Mix: A collection of checkpoints and/or programs to be run as a single multi-core scarab_launch job.
  -Suite: A collection of Benchmarks and/or Mixes to be run as separate scarab_launch jobs.
  -ScarabParams: A collection of Scarab knobs to run scarab_launch with

Results Dir:
  <results_dir>/<JobName>/<Bechmark | Mix>/<Checkpoint | Program name>/<stats files, run_cmd, log_files>
"""

import os
import sys
import random

sys.path.append(os.path.dirname(__file__))
import scarab_paths
import scarab_utils
import command
import progress
import scarab_stats
import scarab_snapshot
from object_manager import *

class ScarabParamDefaults:
  """
  Default Scarab Parameter Values

  These are values that are not likely to change, so we provide defaults for them.
  """
  scarab_stdout = "scarab.stdout"
  scarab_stderr = "scarab.stderr"
  pin_stdout = "pin.stdout"
  pin_stderr = "pin.stderr"
  scarab_launch_stdout = "launch.stdout"
  scarab_launch_stderr = "launch.stderr"

  python_bin = "python3"
  scarab_launch = scarab_paths.bin_dir + "/scarab_launch.py"
  scarab = scarab_paths.scarab_bin
  frontend_pin_tool = scarab_paths.pin_bin

  # Batch Variables
  walltime = None
  memory_per_core = None
  cores = 10

class ScarabParams:
  def __init__(self, scarab_args="", pintool_args="", params_file=None):
    self.scarab_args = scarab_args
    self.pintool_args = pintool_args
    self.params_file = params_file

    self.scarab_stdout = ScarabParamDefaults.scarab_stdout
    self.scarab_stderr = ScarabParamDefaults.scarab_stderr
    self.pin_stdout    = ScarabParamDefaults.pin_stdout
    self.pin_stderr    = ScarabParamDefaults.pin_stderr
    self.scarab_launch_stdout = ScarabParamDefaults.scarab_launch_stdout
    self.scarab_launch_stderr = ScarabParamDefaults.scarab_launch_stderr

    self.python_bin = ScarabParamDefaults.python_bin
    self.scarab_launch = ScarabParamDefaults.scarab_launch
    self.scarab = ScarabParamDefaults.scarab
    self.frontend_pin_tool = ScarabParamDefaults.frontend_pin_tool

    self.walltime = ScarabParamDefaults.walltime
    self.memory_per_core = ScarabParamDefaults.memory_per_core
    self.cores = ScarabParamDefaults.cores

    self.snapshot_log = None

  def __add__(self, rhs):
    if type(rhs) == ScarabParams:
      rhs_str = " " + rhs.scarab_args
    else:
      rhs_str = " " + rhs

    x = ScarabParams(scarab_args=self.scarab_args, params_file=self.params_file)
    x.scarab_args += rhs_str
    return x

################################################################################
# Below are Scarab Job objects
################################################################################

class ScarabRun:
  """
  Top level object for Suites, Benchmarks, Mixes, Programs and Checkpoints
  """
  def __init__(self, job_name, job, params, results_dir=os.getcwd()):
    self.job_name = job_name
    self.job = job
    self.params = params
    self.results_parent_dir = os.path.abspath(results_dir)
    self.results_dir = self.results_parent_dir + "/" + self.job_name
    scarab_run_manager.register(self)

  def create_snapshot(self):
    scarab_snapshot.create_snapshot(self.params, self.results_dir)

  def make(self):
    os.makedirs(self.results_parent_dir, exist_ok=True)
    os.makedirs(self.results_dir, exist_ok=True)
    self.job.make(self.results_dir)

  def get_commands(self):
    self.cmd_list = self.job.create_joblist(self.results_dir, self.params)
    return self.cmd_list

  def process_command_list(self):
    self.make()
    return self.get_commands()

  def print_progress(self):
    progress = self.job.get_progress(self.results_dir)
    progress.sort()
    print("JOB: {name}".format(name=self.job_name))
    print('-'*70)
    for p in progress:
      print(p)
    print('='*70+"\n")

  def get_stats(self, flat=False):
    suite_stat = self.job.get_stats(self.results_dir, flat=flat)
    return suite_stat

  def print_commands(self):
    self.get_commands()
    for cmd in self.cmd_list:
      print(cmd)

class Executable:
  """
  Declares a Executable Object.
  """
  def __init__(self, name, path, scarab_args="", pintool_args="", weight=1.0):
    self.name = name
    self.scarab_args = scarab_args
    self.pintool_args = pintool_args
    self.weight = weight
    self.num_cores = 1

    if path:
      self.path = os.path.abspath(path)
    else:
      self.path = None

  def _results_dir(self, basename):
    return os.path.join(os.path.abspath(basename), self.name)

  def make(self, basename):
    results_dir = self._results_dir(basename)
    os.makedirs(results_dir, exist_ok=True)

  def create_joblist(self, basename, scarab_params):
    assert scarab_params.params_file, "Must provide a PARAMS file to create_joblist for {name}".format(name=self.name)
    results_dir = self._results_dir(basename)
    return [ generate_run_command(self, results_dir, scarab_params) ]

  def get_progress(self, basename):
    results_dir = self._results_dir(basename)
    return [ progress.Progress(results_dir) ]

  def get_stats(self, basename, flat=False):
    results_dir = self._results_dir(basename)
    stat_frame = scarab_stats.StatFrame(self.name, results_dir, weight=self.weight)
    return stat_frame

  def get_random_workload(self):
    return self

class Checkpoint(Executable):
  def __init__(self, name, path, scarab_args="", pintool_args="", weight=1.0):
    super().__init__(name, path, scarab_args, pintool_args, weight)
    checkpoint_manager.register(self)

  def typestr(self):
    return "Checkpoint"

class Trace(Executable):
  def __init__(self, name, path, scarab_args="", pintool_args="", weight=1.0):
    super().__init__(name, path, scarab_args, pintool_args, weight)
    trace_manager.register(self)

  def typestr(self):
    return "Trace"

class Program(Executable):
  def __init__(self, name, run_cmd, path=None, scarab_args="", pintool_args="", weight=1.0, copy=False):
    super().__init__(name, path, scarab_args, pintool_args, weight)
    self.run_cmd = run_cmd
    self.copy = copy
    program_manager.register(self)

  def typestr(self):
    return "Program"

  def make(self, basename):
    super().make(basename)
    results_dir = self._results_dir(basename)
    if self.copy:
      scarab_utils.recursive_copy(self.path, results_dir)

class Mix(Executable):
  """
  A collection of Checkpoints and/or Programs to be run as a single multi-core
  scarab_launch job.

  Attributes:
    -name: The name of the benchmark. Used for the results directory.
    -mix_list: The list of Checkpoints and/or Programs.
  """
  def __init__(self, name, mix_list, scarab_args="", pintool_args=""):
    self.name = name
    self.mix_list = mix_list
    self.scarab_args = scarab_args
    self.pintool_args = pintool_args
    self.num_cores = len(self.mix_list)
    self.weight = 1.0
    mix_manager.register(self)

  def typestr(self):
    return "Checkpoint"

  def make(self, basename):
    super().make(basename)
    results_dir = self._results_dir(basename)

    for mix_item in self.mix_list:
      if type(mix_item) == Program:
        if mix_item.copy:
          scarab_utils.recursive_copy(mix_item.path, results_dir)

class Collection:
  """
  A collection of Checkpoints and/or Programs to be run in parallel as seperate
  scarab_launch jobs

  Attributes:
    -name: The name of the benchmark. Used for the results directory.
    -bench_list: The list of Checkpoints and/or Programs.
  """
  def __init__(self, name, exec_list, weight=1.0):
    self.name = name
    self.exec_list = exec_list
    self.weight=weight
    collection_manager.register(self)

  def _results_dir(self, basename):
    return os.path.join(os.path.abspath(basename), self.name)

  def make(self, basename):
    results_dir = self._results_dir(basename)
    os.makedirs(results_dir, exist_ok=True)

    for bench in self.exec_list:
      bench.make(results_dir)

  def create_joblist(self, basename, scarab_params):
    results_dir = self._results_dir(basename)
    joblist = []
    for executable in self.exec_list:
      joblist += executable.create_joblist(results_dir, scarab_params)
    return joblist

  def get_progress(self, basename):
    results_dir = self._results_dir(basename)
    progress_list = []
    for executable in self.exec_list:
      progress_list += executable.get_progress(results_dir)
    return progress_list

  def get_stats(self, basename, flat=False):
    results_dir = self._results_dir(basename)
    stat_collection = scarab_stats.StatCollection(self.name)
    for executable in self.exec_list:
      stat_collection.append(executable.get_stats(results_dir, flat=flat))
    return stat_collection

  def get_random_workload(self):
    return random.choice(self.exec_list).get_random_workload()

  def get_random_mix(self, size, join='+', seed=None):
    mix = []
    name = []
    scarab_args = ""
    pintool_args = ""

    random.seed(a=seed, version=2)

    for i in range(size):
      workload = self.get_random_workload()

      scarab_args = workload.scarab_args
      pintool_args = workload.pintool_args

      name.append(workload.name)
      mix.append(workload)

    return Mix(join.join(name), mix, scarab_args=scarab_args, pintool_args=pintool_args)

class Benchmark(Collection):
  """
  A Benchmark is a collection of weighted checkpoints or programs. When stats are collected,
  the weighted average of stats must be collected
  """
  def typestr(self):
    return "Benchmark"

  def get_stats(self, basename, flat=False):
    stat_collection = super().get_stats(basename, flat=flat)

    if flat:
      return stat_collection
    else:
      stat_frame = stat_collection.apply_weight(1.0).accumulate().normalize()
      stat_frame.weight = self.weight
      return stat_frame

class Suite(Collection):
  """
  A Suite is a collection of checkpoints, programs, mixes, or benchmarks. When stats are
  collected, the stats should be reported together as a group
  """
  def typestr(self):
    return "Suite"

  def get_stats(self, basename, flat=False):
    bench_collection = super().get_stats(basename, flat=flat)
    return bench_collection.normalize()

###########################################################################
# Helper Functions
###########################################################################

def get_program_or_checkpoint_options(job):
  """
  Returns the proper --program or --checkpoint option(s)
  job can be a Program, Checkpoint or Mix object.
  """
  run_exec = ""

  if type(job) is Program:
    run_exec = f' --program="{job.run_cmd}"'
  elif type(job) is Checkpoint:
    run_exec = f' --checkpoint="{job.path}"'
  elif type(job) is Trace:
    run_exec = f' --trace="{job.path}"'
  elif type(job) is Mix:
    for mix in job.mix_list:
      run_exec += " " + get_program_or_checkpoint_options(mix)
  else:
    assert False, "Attempting to get_program_or_checkpoint_option on an invalid Type!"

  return run_exec

def generate_run_command(job, results_dir, scarab_params):
  scarab_args = job.scarab_args + scarab_params.scarab_args + \
          " --num_cores {num_cores} --output_dir {output_dir}".format(num_cores=job.num_cores, output_dir=results_dir)
  params_file = scarab_params.params_file

  scarab_launch_cmd = scarab_params.python_bin + " " +                                       \
          scarab_params.scarab_launch +                                                      \
          " --scarab " + scarab_params.scarab +                                              \
          " --frontend_pin_tool " + scarab_params.frontend_pin_tool +                        \
          " --pintool_args=\"" + scarab_params.pintool_args + " " + job.pintool_args + "\""  \
          " --scarab_args=\""  + scarab_params.scarab_args  + " " + scarab_args  + "\""      \
          " --params " + params_file +                                                       \
          " --scarab_stdout " + results_dir + "/" + scarab_params.scarab_stdout +            \
          " --scarab_stderr " + results_dir + "/" + scarab_params.scarab_stderr +            \
          " --pin_stdout " + results_dir + "/" + scarab_params.pin_stdout +                  \
          " --pin_stderr " + results_dir + "/" + scarab_params.pin_stderr +                  \
          get_program_or_checkpoint_options(job)

  launch_out = results_dir + "/" + scarab_params.scarab_launch_stdout
  launch_err = results_dir + "/" + scarab_params.scarab_launch_stderr
  cmd = command.Command(scarab_launch_cmd, name=job.name, run_dir=results_dir, results_dir=results_dir, stdout=launch_out, stderr=launch_err)
  cmd.walltime = scarab_params.walltime
  cmd.memory_per_core = scarab_params.memory_per_core
  cmd.cores = scarab_params.cores
  cmd.snapshot_log = scarab_params.snapshot_log
  return cmd
