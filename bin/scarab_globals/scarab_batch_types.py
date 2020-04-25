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

sys.path.append(os.path.dirname(__file__))
import scarab_paths
import scarab_utils
import command
import progress
import scarab_stats

programs_list = []
checkpoints_list = []

class SystemConfig:
  """
  System Settings for JobFiles
  """
  submission_system = command.SubmissionSystems.LOCAL
  pintool_args = ""
  scarab_args = ""

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

class ScarabParams:
  def __init__(self, command_line_params="", params_file=None):
    self.command_line_params = command_line_params
    self.params_file = params_file

  def __add__(self, rhs):
    if type(rhs) == ScarabParams:
      rhs_str = " " + rhs.command_lime_params
    else:
      rhs_str = " " + rhs

    x = ScarabParams(self.command_line_params, self.params_file)
    x.command_line_params += rhs_str
    return x

  def args(self):
    return self.command_line_params

################################################################################
# Below are Scarab Job objects
################################################################################

class JobManager:
  """
  Static class that manages all Jobs that have been created
  """
  job_pool = []

  @classmethod
  def register_job(cls, job):
    cls.job_pool.append(job)

  @classmethod
  def make(cls):
    for job in cls.job_pool:
      job.make()

  @classmethod
  def run(cls):
    for job in cls.job_pool:
      job.run()

  @classmethod
  def print_progress(cls):
    for job in cls.job_pool:
      job.print_progress()

  @classmethod
  def get_stats(cls):
    job_stat = scarab_stats.StatFrame()
    for job in cls.job_pool:
      job_stat.push(job.job_name, job.get_stats())
    return job_stat

  @classmethod
  def print_commands(cls):
    for job in cls.job_pool:
      job.print_commands()

class Job:
  """
  Top level object for Suites, Benchmarks, Mixes, Programs and Checkpoints
  """
  def __init__(self, job_name, job, params, results_dir=os.getcwd()):
    self.job_name = job_name
    self.job = job
    self.params = params
    self.results_parent_dir = os.path.abspath(results_dir)
    self.results_dir = self.results_parent_dir + "/" + self.job_name
    JobManager.register_job(self)

  def make(self):
    os.makedirs(self.results_parent_dir, exist_ok=True)
    os.makedirs(self.results_dir, exist_ok=True)
    self.job.make(self.results_dir)

  def run(self):
    self.cmd_list = self.job.create_joblist(self.results_dir, self.params)
    cmd = command.launch(SystemConfig.submission_system, self.cmd_list, jobname=self.job_name)

  def print_progress(self):
    progress = self.job.get_progress(self.results_dir)
    progress.sort()
    for p in progress:
      print(p)

  def get_stats(self):
    suite_stat = self.job.get_stats(self.results_dir)
    return suite_stat

  def print_commands(self):
    self.cmd_list = self.job.create_joblist(self.results_dir, self.params)
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
    scarab_args = self.scarab_args + scarab_params.args() + " --num_cores {num_cores} --output_dir {output_dir}".format(num_cores=self.num_cores, output_dir=results_dir)
    return [ generate_run_command(self, results_dir, scarab_params.params_file, scarab_args) ]

  def get_progress(self, basename):
    results_dir = self._results_dir(basename)
    return [ progress.Progress(results_dir) ]

  def get_stats(self, basename):
    results_dir = self._results_dir(basename)
    return scarab_stats.ExecutableStat(results_dir=results_dir, weight=self.weight, label=self.name)

class Checkpoint(Executable):
  def __init__(self, name, path, scarab_args="", pintool_args="", weight=1.0):
    super().__init__(name, path, scarab_args, pintool_args, weight)
    checkpoints_list.append(self)

class Program(Executable):
  def __init__(self, name, run_cmd, path=None, scarab_args="", pintool_args="", weight=1.0, copy=False):
    super().__init__(name, path, scarab_args, pintool_args, weight)
    self.run_cmd = run_cmd
    self.copy = copy
    programs_list.append(self)

  def _run_program_at_path(self):
    """Returns True if PIN process needs to be run at directory specified by path attribute."""
    return self.path and not self.copy

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
  def __init__(self, name, exec_list):
    self.name = name
    self.exec_list = exec_list

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

  def get_stats(self, basename):
    results_dir = self._results_dir(basename)
    stat_list = []
    for executable in self.exec_list:
      stat_list.append(executable.get_stats(results_dir))
    return stat_list

class Benchmark(Collection):
  """
  A Benchmark is a collection of weighted checkpoints or programs. When stats are collected,
  the weighted average of stats must be collected
  """
  def get_stats(self, basename):
    stat_list = super().get_stats(basename)
    bench_stat = scarab_stats.BenchmarkStat(label=self.name, collection=stat_list)
    return bench_stat

class Suite(Collection):
  """
  A Suite is a collection of checkpoints, programs, mixes, or benchmarks. When stats are
  collected, the stats should be reported together as a group
  """
  def get_stats(self, basename):
    bench_list = super().get_stats(basename)
    suite_stat = scarab_stats.SuiteStat(label=self.name, collection=bench_list)
    return suite_stat

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
    run_exec = " --program=\"" + job.run_cmd + "\""
  elif type(job) is Checkpoint:
    run_exec = " --checkpoint=\"" + job.path + "\""
  elif type(job) is Mix:
    for mix in job.mix_list:
      run_exec += " " + get_program_or_checkpoint_options(mix)
  else:
    assert False, "Attempting to get_program_or_checkpoint_option on an invalid Type!"

  return run_exec

def generate_run_command(job, results_dir, params_file, scarab_args):
  scarab_launch_cmd = SystemConfig.python_bin + " " +                                       \
          SystemConfig.scarab_launch +                                                      \
          " --scarab " + SystemConfig.scarab +                                              \
          " --frontend_pin_tool " + SystemConfig.frontend_pin_tool +                        \
          " --pintool_args=\"" + SystemConfig.pintool_args + " " + job.pintool_args + "\""  \
          " --scarab_args=\""  + SystemConfig.scarab_args  + " " + scarab_args  + "\""      \
          " --params " + params_file +                                                      \
          " --scarab_stdout " + results_dir + "/" + SystemConfig.scarab_stdout +            \
          " --scarab_stderr " + results_dir + "/" + SystemConfig.scarab_stderr +            \
          " --pin_stdout " + results_dir + "/" + SystemConfig.pin_stdout +                  \
          " --pin_stderr " + results_dir + "/" + SystemConfig.pin_stderr +                  \
          get_program_or_checkpoint_options(job)

  launch_out = results_dir + "/" + SystemConfig.scarab_launch_stdout
  launch_err = results_dir + "/" + SystemConfig.scarab_launch_stderr
  cmd = command.generate(SystemConfig.submission_system, scarab_launch_cmd, run_dir=results_dir, results_dir=results_dir, stdout=launch_out, stderr=launch_err)
  return cmd
