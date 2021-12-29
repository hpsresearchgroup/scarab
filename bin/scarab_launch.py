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

from __future__ import print_function
import argparse
import os
import shutil
import subprocess
import sys
import time

from scarab_globals import *

#Known Issues:
# 1) --frontend trace, does this work?

parser = argparse.ArgumentParser(description="Launch Scarab")
parser.add_argument('--program', default=None, action='append', help="Command line for the program to be simulated.")
parser.add_argument('--checkpoint', default=None, action='append', help="Path to the checkpoint to be simulated.")
parser.add_argument('--trace', default=None, action='append', help="Path to the program trace to be simulated.")
parser.add_argument('--params', default=None, help="Path to PARAMS file. Will copy to currect directory and name PARAMS.in")

parser.add_argument('--scarab_args', default="", help="Arguments to pass to scarab directly.")
parser.add_argument('--pintool_args', default="", help="Arguments to pass to the pintool directly.")

parser.add_argument('--simdir', default=os.getcwd(), help="Path to the directory to simulate in.")
parser.add_argument('--scarab_stdout', default=None, help="Path to redirect scarabs stdout to. Default is stdout.")
parser.add_argument('--scarab_stderr', default=None, help="Path to redirect scarabs stderr to. Default is stderr.")
parser.add_argument('--pin_stdout', default=None, help="Path to redirect pins stdout to. Default is stdout.")
parser.add_argument('--pin_stderr', default=None, help="Path to redirect pins stderr to. Default is stderr.")

parser.add_argument('--enable_aslr', action='store_true', help="Enable ASLR for the application and pintool.")

parser.add_argument('--scarab', default=scarab_paths.scarab_bin, help="Path to the scarab binary. Defaults to src/scarab.")
parser.add_argument('--pin', default=scarab_paths.pin_dir + "/pin", help="Path to the pin binary. Default is $PIN_ROOT/pin.")
parser.add_argument('--frontend_pin_tool', default=scarab_paths.pin_bin, help="Path to the pin tool that will act as the frontend.")
parser.add_argument('--checkpoint_loader', default=scarab_paths.checkpoint_loader_bin, help="Path to checkpoint loader executable.")
parser.add_argument('--frontend', default="exec", choices=["exec", "trace"], help="Selects between the Trace fronend and the Exec-driven frontend.")

args = parser.parse_args()

def determine_frontend():
  trace_mode = args.trace
  exec_driven_mode = args.program or args.checkpoint
  
  if not trace_mode and not exec_driven_mode:
    print("Usage: At least one program (--program) or checkpoint (--checkpoint) or trace (--trace) must be specified")
    progress.notify("Scarab run terminated, cleaning up...")
    sys.exit(-1)
  
  elif trace_mode and exec_driven_mode:
    print("Usage: Cannot use exec_driven mode (--program or --checkpoint) along with trace mode (--trace)")
    progress.notify("Scarab run terminated, cleaning up...")
    sys.exit(-1)
  
  elif trace_mode:
    return 'trace'
  
  else:
    return 'exec_driven'

def get_num_cores():
  cores = 0
  if args.program:
    cores += len(args.program)
  if args.checkpoint:
    cores += len(args.checkpoint)
  return cores

def make_checkpoint_loader():
  if not os.path.exists(args.checkpoint_loader):
    print("Compiling Checkpoint Loader: {}".format(args.checkpoint_loader))
    checkpoint_loader_dir = os.path.dirname(args.checkpoint_loader)
    make_cmd = "make --directory={checkpoint_loader_dir}".format(checkpoint_loader_dir=checkpoint_loader_dir)
    cmd = command.Command(make_cmd)
    cmd.run()

class Scarab:
  """
  Setup the Scarab process from command line args and launch it.
  """
  def __init__(self, frontend, *, trace_list=None, socket_path=None):
    assert frontend in ['trace', 'exec_driven']
    self.frontend = frontend
    if frontend == 'trace':
      assert trace_list is not None
      self.trace_list = [os.path.abspath(trace) for trace in trace_list]
    if frontend == 'exec_driven':
      assert socket_path is not None
      self.socket_path = os.path.abspath(socket_path)

  def __copy_params_file_to_simdir(self):
    if args.params:
      shutil.copy2(args.params, args.simdir + "/PARAMS.in") 
    else:
      scarab_utils.warn("Using existing PARAMS.in file in current directory!")

  def __get_scarab_command(self):
    if self.frontend == 'trace':
      frontend_specific_args = f'--fetch_off_path_ops 0 --frontend trace'
      for i, trace in enumerate(self.trace_list):
        frontend_specific_args += f' --cbp_trace_r{i} {trace}'
    
    elif self.frontend == 'exec_driven':
      frontend_specific_args = f'--frontend pin_exec_driven --pin_exec_driven_fe_socket {self.socket_path}'

    self.cmd = "{scarab} --num_cores {num_cores} {frontend_args} --bindir {bin_dir} {additional_args}".format(
      scarab=args.scarab,
      num_cores=get_num_cores(),
      frontend_args=frontend_specific_args,
      bin_dir=scarab_paths.bin_dir,
      additional_args=args.scarab_args
    )
    return self.cmd

  def launch(self):
    self.__copy_params_file_to_simdir()
    self.__get_scarab_command()
    
    print('\nLaunching Scarab:\n' + self.cmd + '\n')

    cmd = command.Command(self.cmd, run_dir=args.simdir, stdout=args.scarab_stdout, stderr=args.scarab_stderr)
    cmd.run_in_background()
    return cmd

class Pin:
  """
  Setup Pin from command line args and launch it
  """
  def __init__(self, core_id, socket_path, program_path, is_checkpoint):
    self.core_id = str(core_id)
    self.socket_path = os.path.abspath(socket_path)
    self.program_path = os.path.realpath(program_path)
    self.is_checkpoint = is_checkpoint

  def __get_pin_command(self):
    if self.is_checkpoint:
      self.__get_pin_checkpoint_command()
    else:
      self.__get_pin_program_command()

    if not args.enable_aslr:
      self.cmd = scarab_utils.get_disable_aslr_prefix() + " " + self.cmd
    
    print("{cmd}\n".format(cmd=self.cmd))

    return self.cmd

  def __get_pin_checkpoint_command(self):
    self.cmd = '{checkpoint_loader} {checkpoint_path} {socket} {core_id} {pin_tool} -pintool_args="{pintool_args}"'.format(
        checkpoint_loader=args.checkpoint_loader,
        checkpoint_path=self.program_path,
        socket=self.socket_path,
        core_id=self.core_id,
        pin_tool=args.frontend_pin_tool,
        pintool_args=args.pintool_args,
      )

    print("\nCore {core_id} is running checkpoint: {checkpoint_path}".format(
      core_id=self.core_id,
      checkpoint_path=self.program_path,
      ))
    return self.cmd

  def __get_pin_program_command(self):
    self.cmd = "{pin} -mt 0 -t {pin_tool} -socket_path {socket} -core_id {core_id} {additional_args} -- {program_command}".format(
        pin=args.pin,
        pin_tool=args.frontend_pin_tool,
        socket=self.socket_path,
        core_id=self.core_id,
        additional_args=args.pintool_args,
        program_command=self.program_path
      )

    print("\nCore {core_id} is running program: {program_command}".format(
      core_id=self.core_id,
      program_command=self.program_path,
      ))
    return self.cmd

  def __get_stdout(self):
    self.stdout = None
    if args.pin_stdout:
      self.stdout = args.pin_stdout + "." + str(self.core_id) + ".out"

  def __get_stderr(self):
    self.stderr = None
    if args.pin_stderr:
      self.stderr = args.pin_stderr + "." + str(self.core_id) + ".out"

  def launch(self):
    self.__get_pin_command()
    self.__get_stdout()
    self.__get_stderr()

    cmd = command.Command(self.cmd, run_dir=args.simdir, stdout=self.stdout, stderr=self.stderr)
    cmd.run_in_background()
    return cmd

def launch_programs(proc_list, core, socket_path):
  if (args.program):
    for program in args.program:
      proc_list.push(Pin(core, socket_path, program, False).launch())
      core += 1
  return core

def launch_checkpoints(proc_list, core, socket_path):
  if (args.checkpoint):
    for checkpoint in args.checkpoint:
      proc_list.push(Pin(core, socket_path, checkpoint, True).launch())
      core += 1
  return core

def main():
  frontend = determine_frontend()
  assert frontend in ['trace', 'exec_driven']

  proc_list = command.CommandTracker()

  if args.checkpoint:
    make_checkpoint_loader()

  return_code = 0
  try:
    if frontend == 'exec_driven':
      socket_path = scarab_utils.get_temp_socket_path()
      proc_list.push(Scarab(frontend, socket_path=socket_path).launch())
    elif frontend == 'trace':
      proc_list.push(Scarab(frontend, trace_list=args.trace).launch())


    if frontend == 'exec_driven':
      time.sleep(1)
      core = 0
      core = launch_programs(proc_list, core, socket_path)
      core = launch_checkpoints(proc_list, core, socket_path)

    return_code = proc_list.wait_on_processes()

  finally:
    progress.notify("Scarab run terminated, cleaning up...")
    proc_list.kill_all_processes()
  
  sys.exit(return_code)

if __name__ == "__main__":
  main()
