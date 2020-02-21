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

class SubmissionSystems(Enum):
  LOCAL = 0
  PBS = 1
  SBATCH = 2

class LocalConfig:
  num_threads = None

class PBSConfig:
  PBS_args ="-V -l nodes=1:ppn=5"

def generate(submission_system, cmd_str, run_dir=None, results_dir=os.getcwd(), stdout=None, stderr=None):
  if submission_system == SubmissionSystems.LOCAL:
    cmd =    Command(cmd_str, run_dir=run_dir, results_dir=results_dir, stdout=stdout, stderr=stderr)
  elif submission_system == SubmissionSystems.PBS:
    cmd = PBSCommand(cmd_str, run_dir=run_dir, results_dir=results_dir, stdout=stdout, stderr=stderr)
  else:
    assert False, "Error: Attempting to generate command for unknown submission system: {}".format(submission_system)
  return cmd

def launch(submission_system, cmd_list):
  executor = JobPoolExecutor(cmd_list)
  if submission_system == SubmissionSystems.LOCAL:
    executor.run_parallel_commands(num_threads=LocalConfig.num_threads)
  elif submission_system == SubmissionSystems.PBS:
    executor.run_serial_commands()
  else:
    assert False, "Error: Attempting to generate command for unknown submission system"

class Command:
  def __init__(self, cmd_str, run_dir=None, results_dir=os.getcwd(), stdout=None, stderr=None, stdout_fp=None, stderr_fp=None):
    self.stdout      = stdout
    self.stderr      = stderr
    self.stdout_fp   = stdout_fp
    self.stderr_fp   = stderr_fp
    self.run_dir     = run_dir
    self.results_dir = os.path.abspath(results_dir)
    self.cmd         = cmd_str
    self.process     = None

    if self.run_dir:
      self.run_dir     = os.path.abspath(run_dir)

  def __open_stdout(self):
    if self.stdout:
      self.stdout = os.path.abspath(self.stdout)
      self.stdout_fp = open(self.stdout, "w")

  def __open_stderr(self):
    if self.stderr:
      self.stderr = os.path.abspath(self.stderr)
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

    if self.run_dir:
      os.chdir(self.run_dir)

  def run(self):
    self.__prepare_to_run()
    self.returncode = subprocess.call(shlex.split(self.cmd), stdout=self.stdout_fp, stderr=self.stderr_fp)
    self.__close_files()
    return self.returncode

  def run_in_background(self):
    self.__prepare_to_run()
    self.process = subprocess.Popen(shlex.split(self.cmd), stdin=None, stdout=self.stdout_fp, stderr=self.stderr_fp)
    self.__close_files()
    return self.process

  def append_to_jobfile(self, f):
    f.write(self.cmd)
    f.write('\n')

  def write_to_jobfile(self, jobfile_name="jobfile", jobfile_permissions=0o760, prefix=None):
    self.jobfile_path = self.results_dir + "/" + jobfile_name
    f = open(self.jobfile_path, "w+")

    if prefix:
      f.write(prefix)

    self.append_to_jobfile(f)
    f.close()
    os.chmod(self.jobfile_path, jobfile_permissions)
    return self.jobfile_path

  def poll(self):
    assert self.process, "Error: Cannot poll Command that has not been run!"
    self.process.poll();
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

class PBSCommand(Command):

  def __str__(self):
    return "PBS" + super().__str__()

  def run(self, pbs_args=PBSConfig.PBS_args):
    prefix = "source /export/software/anaconda3/bin/activate\n"
    if self.run_dir:
      prefix += "cd {}\n".format(self.run_dir)

    self.write_to_jobfile(prefix=prefix)

    self.pbs_cmd = "qsub " + pbs_args + \
          " -o " + self.stdout +        \
          " -e " + self.stderr +        \
          " " + self.jobfile_path
    
    print(self.pbs_cmd)
    proc = subprocess.Popen(shlex.split(self.pbs_cmd), stdout=subprocess.PIPE)
    proc_out, proc_err = proc.communicate()

    self.pbs_job_id = proc_out.rstrip().decode('utf-8');
    print(self.pbs_job_id)

    return self.pbs_job_id
    

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

class JobPoolExecutor:
  """
  Runs a Pool of Jobs on a Submission System.
  """
  def __init__(self, cmds):
    self.cmds = cmds

  def push(self, cmd):
    self.cmds.append(cmd);

  @staticmethod
  def run_command(cmd):
    returncode = cmd.run()
    print("Finished command with return code: {}".format(returncode))
    print(cmd)
    return returncode

  def run_serial_commands(self):
    for cmd in self.cmds:
      cmd.run()

  def run_parallel_commands(self, num_threads=None):
    """
    If num_threads is None, Pool() uses number of cores in the system.
    """
    print("Starting Run Parallel Commands: num_cmds={}, num_threads={}".format(len(self.cmds), num_threads))
    with multiprocessing.Pool(num_threads) as pool:
      pool.map(JobPoolExecutor.run_command, self.cmds)
