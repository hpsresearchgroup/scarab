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
import subprocess
import multiprocessing
import time
from enum import Enum
import shlex

"""
This file declares a wrapper class for arbitrary Bash Commands. It is meant to be the interface between
python and Bash. Ultimately, all commands run directly in the shell are passed through here.
"""

class CommandDefaults:
  walltime          = None # Ammount of time to run process before killing it
  memory_per_core   = None # Expected memory required by process
  cores             = None # Expected number of cores required by process

class Command:
  def __init__(self, cmd_str, name=None, run_dir=None, results_dir=os.getcwd(), stdout=None, stderr=None, stdout_fp=None, stderr_fp=None):
    self.stdout      = stdout
    self.stderr      = stderr
    self.stdout_fp   = stdout_fp
    self.stderr_fp   = stderr_fp
    self.run_dir     = run_dir
    self.results_dir = os.path.abspath(results_dir)
    self.cmd         = cmd_str
    self.process     = None
    self.name        = name
    self.snapshot_log = None

    # Batch Variables
    self.walltime        = CommandDefaults.walltime
    self.memory_per_core = CommandDefaults.memory_per_core
    self.cores           = CommandDefaults.cores

    if self.stdout:
      self.stdout = os.path.join(self.results_dir, self.stdout)
    if self.stderr:
      self.stderr = os.path.join(self.results_dir, self.stderr)

    self.cwd = None
    if self.run_dir:
      self.run_dir     = os.path.abspath(run_dir)

  def __open_stdout(self):
    if self.stdout:
      self.stdout_fp = open(self.stdout, "w")

  def __open_stderr(self):
    if self.stderr:
      self.stderr_fp = open(self.stderr, "w")

  def __close_files(self):
    if self.stdout:
      self.stdout_fp.close()
    if self.stderr:
      self.stderr_fp.close()

  def __str__(self):
    return "Command:\n{cmd}\nRUNDIR:{run_dir}\nRESULTS_DIR:{results_dir}\nSTDOUT:{stdout}\nSTDERR:{stderr}\n".format(
        cmd=self.cmd,
        run_dir=self.run_dir,
        results_dir=self.results_dir,
        stdout=self.stdout,
        stderr=self.stderr
      )

  def __prepare_to_run(self):
    self.__open_stdout()
    self.__open_stderr()
    self.cwd = os.getcwd()

    if self.run_dir:
      os.chdir(self.run_dir)

  def __clean_up(self):
    self.__close_files()
    if self.cwd:
      os.chdir(self.cwd)

  def process_command_list(self):
    return [ self ]

  def run(self):
    self.__prepare_to_run()
    self.returncode = subprocess.call(shlex.split(self.cmd), stdout=self.stdout_fp, stderr=self.stderr_fp)
    self.__clean_up()
    return self.returncode

  def run_in_background(self):
    self.__prepare_to_run()
    self.process = subprocess.Popen(shlex.split(self.cmd), stdin=None, stdout=self.stdout_fp, stderr=self.stderr_fp)
    self.__clean_up()
    return self.process

  def append_to_jobfile(self, f):
    f.write(self.cmd)
    f.write('\n')

  def write_to_snapshot_log(self, job_id):
    if self.snapshot_log:
      with open(self.snapshot_log, "a+") as fp:
        print("{job_id}\t{results_dir}".format(job_id=job_id, results_dir=self.results_dir),
              file=fp)

  def write_to_jobfile(self, jobfile_name="jobfile", jobfile_permissions=0o760, prefix=None, suffix=None):
    if self.name:
      jobfile_name = "{cmd_name}.{jobfile_name}".format(
        cmd_name=self.name,
        jobfile_name=jobfile_name
      )
    self.jobfile_path = self.results_dir + "/" + jobfile_name
    f = open(self.jobfile_path, "w+")

    if prefix:
      f.write(prefix)

    self.append_to_jobfile(f)

    if suffix:
      f.write(suffix)

    f.close()
    os.chmod(self.jobfile_path, jobfile_permissions)
    return self.jobfile_path

  def poll(self):
    assert self.process, "Error: Cannot poll Command that has not been run!"
    self.process.poll()
    self.returncode = self.process.returncode

  def wait(self):
    assert self.process, "Error: Cannot wait on Command that has not been run!"

    while True:
      self.poll()
      if None != self.returncode:
        if 0 != self.returncode:
          print("TERMINATING BECAUSE NON-ZERO RETURN CODE FOR:\n" + self.cmd + '\n')
          return
      time.sleep(1)

  def kill(self):
    assert self.process, "Error: Cannot kill Command that has not been run!"

    if self.process.poll() is None:
      self.process.kill()

class CommandTracker:
  """
  Tracks a list of commands by polling them in a round robin fasion.
  This way if one command fails, the entire list of commands can be killed early.
  """

  def __init__(self, list_of_commands=[]):
    self.proc_list = list_of_commands

  def push(self, command):
    self.proc_list.append(command)


  def wait_on_processes(self):
    assert len(self.proc_list) != 0, "Error: Cannot wait on an empty list of commands!"

    done = [False] * len(self.proc_list)

    while not all(done):
      time.sleep(1)
      for i, proc in enumerate(self.proc_list):
        proc.poll()
        if None != proc.returncode and not done[i]:
          done[i] = True
          print("RETURN CODE " + str(proc.returncode) + ": " + proc.cmd + '\n'); 
          
          if 0 != proc.returncode:
            print("Error: TERMINATING BECAUSE NON-ZERO RETURN CODE FOR:\n" + proc.cmd + '\n')
            return 1

    return 0

  def kill_all_processes(self):
    time.sleep(1) # Delay to ensure processes have started before sending kill
    for proc in self.proc_list:
      proc.kill()