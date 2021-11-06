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

""" BatchManager

The BatchManager is responsible for creating and maintaining an order of parallel Commands/ScarabRuns to be launched
on a job submission system (such as your local machine, PBS, or SBATCH). This file defines several objects (listed
under Interface below) which are used to specify if jobs can be run in parallel or must be run sequentially.

The BatchManager tracks what we refer to in the documentation as "jobs". Specifically, jobs are either a Command object
(defined in scarab_globals/command.py) or a ScarabRun object (defined in scarab_globals/scarab_batch_types.py). A command
is simply a wrapper class around any arbitrary Unix command. A ScarabRun is specifically a set of Commands meant to launch
several instances of Scarab on the same set of paramters (but accross multiple benchmarks). 

The BatchManager organizes jobs into Phases. A Phase (defined in this file) is a set of Commands or ScarabRuns that can
be run in any order or in parallel. The BatchManager maintains a list of phases, which must be run sequentially. That is
a phase cannot begin execution until all of the jobs within the previous phase have terminated (either in success or failure).

Interface:
  Command: A wrapper class around an arbitrary Unix command (defined in scarab_globals/command.py).
  ScarabRun: A set of Commands meant to launch several instances of Scarab on the same set of ScarabParams,
           but across multiple benchmarks.
  Phase: A set of jobs (Commands or ScarabRun objects) that can be run in any order or in parallel. The phase is complete
           once all of the jobs have completed (either in success or failure). Phases must be run sequentially. That is, 
           the next phase cannot begin until the current phase has completed.

Types of BatchManagers:
  BatchManager: This will run jobs on locally on your machine. By default, BatchManager will greedily launch one job per core
          on your local system. Although this can be configured (see the constructor for BatchManager).
  PBSBatchManager: This will launch jobs on a PBS system. All jobs will be launched at once, however the dependencies between
          jobs will be conveyed to PBS. See the constructor for PBSJobManager for more details.
  SBATCHJobManager: Coming Soon!
"""

import os
import sys
import subprocess
import multiprocessing
import time
from enum import Enum
import shlex

sys.path.append(os.path.dirname((__file__)))
from scarab_batch_types import *
from command import *
import object_manager

class JobDefaults:
  """A set of constant default values that BatchManager will use if no override is provided.

  Args:
    email: The email address to contact when the job status is updated
    walltime: The max time to run a job before timeout occurs
    processor_cores_per_node: The number of processors to consume per node
    precommands: A bash script to run before the job is started
    postcommands: A bash script to run once the job is completed
    trapcommands: A bash script to run if the job is terminated early (e.g., timeout)
  """
  email=None
  walltime=None
  processor_cores_per_node=None
  precommands="echo Start Job"
  postcommands="echo End Job"
  trapcommands="echo Job Terminated Early!!!"

class PBSJobDefaults:
  """Additional default values that are needed for PBS systems. (See JobDefaults for other default values)

  Args:
    pbs_args: args to be passed to qsub on the command line
    queue: the default queue to submit jobs to (None will leave the queue unspecified).
    processor_cores_per_node: number of processor cores per 1 PBS node to use.
    memory_per_core: the ammount of memory that one core will require.
  """
  pbs_args="-V"
  queue=None
  processor_cores_per_node=10
  memory_per_core=None

class Phase:
  """ A list of Commands or ScarabRuns that can be run in any order or in parallel.
  """
  def __init__(self, job_list, name=None):
    self.name = name
    self.job_list = job_list

  def append(self, job):
    self.job_list.append(job)

  def process_command_list(self):
    flat_command_list = []
    for job in self.job_list:
      flat_command_list += job.process_command_list()
    return flat_command_list

class BatchManager:
  """Launches jobs on your local system. Runs phases sequentially, but jobs within a phase can be run in parallel.

  Args:
    phase_list: The ordered list of phases. Phases will be run sequentially in the provided order.
    email: The email to contact when job status updates. (currently not used)
    walltime: A time limit for each job. (currently not used)
    process_cores_per_node: the number of cores to use on your local machine.
    precommands: a set of Bash commands to run before each job begins. (currently not used)
    postcommands: a set of Bash commands to run at the end of each job. (currently not used)
    trapcommands: a set of Bash commands to run if a job terminates unnaturally (e.g., timeout). (currently not used)
  """
  def __init__(self,
               phase_list=[],
               email=JobDefaults.email,
               walltime=None,
               processor_cores_per_node=None,
               precommands=JobDefaults.precommands,
               postcommands=JobDefaults.postcommands,
               trapcommands=JobDefaults.trapcommands
              ):
    self.phase_list = phase_list
    self.email=email
    self.walltime=walltime
    self.processor_cores_per_node=processor_cores_per_node
    self.precommands = precommands
    self.postcommands = postcommands
    self.trapcommands = trapcommands
    object_manager.scarab_run_manager.register_batch_manager(self)

  def append(self, phase):
    self.phase_list.append(phase)

  @staticmethod
  def run_command(cmd):
    cmd.write_to_snapshot_log(0)
    returncode = cmd.run()
    print("Finished command with return code: {}".format(returncode))
    print(cmd)
    return returncode

  def run(self):
    """
    If num_threads is None, Pool() uses number of cores in the system.
    """
    phase_id = 0
    for phase in self.phase_list:
      if phase.name:
        print("Starting Phase {}".format(phase.name))
      else:
        print("Starting Phase {}".format(phase_id))
      phase_id += 1

      command_list = phase.process_command_list()

      print("Starting BatchManager: num_cmds={num_cmds}, num_threads={num_threads}".format(
        num_cmds=len(command_list),
        num_threads=self.processor_cores_per_node
        ))

      with multiprocessing.Pool(self.processor_cores_per_node) as pool:
        pool.map(BatchManager.run_command, command_list)

class PBSBatchManager(BatchManager):
  """Launches jobs on a PBS system. Jobs are all launched at once, but the job dependency information is conveyed to the PBS system.

  Args:
    phase_list: The ordered list of phases. Phases will be run sequentially in the provided order.
    pbs_args: the command line args to pass to qsub.
    queue: the PBS queue to launch jobs to.
    email: The email to contact when job status updates.
    walltime: A time limit for each job.
    process_cores_per_node: the number of cores to use per 1 PBS node.
    precommands: a set of Bash commands to run before each job begins.
    postcommands: a set of Bash commands to run at the end of each job.
    trapcommands: a set of Bash commands to run if a job terminates unnaturally (e.g., timeout).
  """
  def __init__(self,
               phase_list=[],
               pbs_args=PBSJobDefaults.pbs_args,
               queue=PBSJobDefaults.queue,
               email=JobDefaults.email,
               walltime=JobDefaults.walltime,
               processor_cores_per_node=PBSJobDefaults.processor_cores_per_node,
               memory_per_core=PBSJobDefaults.memory_per_core,
               precommands=JobDefaults.precommands,
               postcommands=JobDefaults.postcommands,
               trapcommands=JobDefaults.trapcommands
              ):
    super().__init__(phase_list,
                     email=email,
                     walltime=walltime,
                     processor_cores_per_node=processor_cores_per_node,
                     precommands=precommands,
                     postcommands=postcommands,
                     trapcommands=trapcommands
                    )
    self.pbs_args = pbs_args
    self.queue = queue
    self.memory_per_core = memory_per_core

  def _prepare_pbs_jobscript_parameters(self, cmd):
    """Prepare the PBS header
    reference: https://www.msi.umn.edu/content/job-submission-and-scheduling-pbs-scripts
    """
    jobscript_text=""

    if cmd.results_dir:
      #set job name
      jobname = ""
      if cmd.name:
        jobname = cmd.name + "-"
      jobname += os.path.basename(cmd.results_dir)
      jobscript_text += "#PBS -N {var}\n".format(var=jobname)

    if self.queue:
      jobscript_text += "#PBS -q {var}\n".format(var=self.queue)

    if self.email:
      jobscript_text += "#PBS -m abe\n" # Send an email if job aborts (a), begins (b), or ends (e)
      jobscript_text += "#PBS -M {var}\n".format(var=self.email)

    if cmd.stdout:
      jobscript_text += "#PBS -o {var}\n".format(var=cmd.stdout)

    if cmd.stderr:
      jobscript_text += "#PBS -e {var}\n".format(var=cmd.stderr)

    if cmd.walltime:
      jobscript_text += "#PBS -l walltime={var}\n".format(var=cmd.walltime)

    if cmd.memory_per_core:
      jobscript_text += "#PBS -l pmem={var}\n".format(var=cmd.memory_per_core)

    if cmd.cores:
      jobscript_text += "#PBS -l nodes=1:ppn={var}\n".format(var=cmd.cores)

    return jobscript_text

  def _create_trap_command(self):
    """
    The trap command allows you to specify a command to run in case your job terminates    
    abnormally, for example if it runs out of wall time. It is typically used to copy      
    output files from a temporary directory to a home or project directory. The following  
    example creates a directory in $PBS_O_WORKDIR and copies everything from $TMPDIR into  
    it. This executes only if the job terminates abnormally.                               

    reference: https://www.osc.edu/sites/osc.edu/files/staff_files/kcahill/sample_job.pbs
    """
    trap_command=""
    if self.trapcommands:
      trap_command='trap "{final_commands}" TERM\n'.format(final_commands=self.trapcommands)
    return trap_command

  def _get_pbs_system_info_func(self):
    # Note: do not change whitespace withing triple quotes
    return """print_system_info() {
  echo ------------------------------------------------------
  echo -n 'Starting Job on '; date
  echo -n 'Job is running on node '; cat $PBS_NODEFILE /dev/null
  echo ------------------------------------------------------
  echo PBS: qsub is running on $PBS_O_HOST
  echo PBS: executing queue is $PBS_QUEUE
  echo PBS: working directory is $PBS_O_WORKDIR
  echo PBS: job identifier is $PBS_JOBID
  echo PBS: job name is $PBS_JOBNAME
  echo PBS: node file is $PBS_NODEFILE
  echo PBS: current home directory is $PBS_O_HOME
  echo PBS: PATH = $PBS_O_PATH
  echo ------------------------------------------------------
}
"""

  def _create_pbs_header(self, cmd):
    pbs_header = """{pbs_knobs}
{system_info_function}
{trap_command}

print_system_info
cd $PBS_O_WORKDIR
{precommands}

""".format(
      pbs_knobs=self._prepare_pbs_jobscript_parameters(cmd),
      system_info_function=self._get_pbs_system_info_func(),
      trap_command=self._create_trap_command(),
      precommands=self.precommands
    )
    return pbs_header

  def run_cmd(self, cmd, dep_ids=[]):
    cwd = os.getcwd()
    if cmd.run_dir:
      os.chdir(cmd.run_dir)

    cmd.write_to_jobfile(prefix=self._create_pbs_header(cmd), 
                          suffix="\n" + self.postcommands + "\n"
                         )
    
    dep_job_string=""
    if len(dep_ids):
      #afterok:  Execute current job after listed jobs have terminated without error
      #afternotok: Execute current job after listed jobs have terminated *with* an error
      #afterany: Execute current job after listed jobs have terminated for *any*reason
      #referece: https://www.nics.tennessee.edu/computing-resources/running-jobs/job-chaining
      dep_job_string = " -W depend=afterany:" + ":".join(dep_ids)

    pbs_cmd = "qsub {pbs_args}{dep_job_string} {jobfile}".format(
                pbs_args=self.pbs_args,
                dep_job_string=dep_job_string,
                jobfile=cmd.jobfile_path
              )
    print(pbs_cmd)

    proc = subprocess.Popen(shlex.split(pbs_cmd), stdout=subprocess.PIPE)
    proc_out, proc_err = proc.communicate()
    pbs_job_id = proc_out.rstrip().decode('utf-8')
    print(pbs_job_id)

    cmd.write_to_snapshot_log(pbs_job_id)
    os.chdir(cwd)

    return pbs_job_id

  def run_phase(self, phase, dep_ids):
    """
    Jobs within a phase can be run in any order or in parallel
    """
    pbs_job_id_list = []
    for cmd in phase.process_command_list():
      id = self.run_cmd(cmd, dep_ids=dep_ids)
      pbs_job_id_list.append(id)
    return pbs_job_id_list

  def run(self):
    ids = []
    for phase in self.phase_list:
      ids = self.run_phase(phase, ids)