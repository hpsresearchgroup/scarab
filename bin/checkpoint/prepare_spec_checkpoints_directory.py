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

import argparse
import distutils.dir_util
import glob
from itertools import product
import math
import multiprocessing
import os
import subprocess
import re
import sys

sys.path.append(os.path.dirname(os.path.dirname(__file__)))

from scarab_globals import *

run_commands = None

def define_benchmarks_and_suites():
  spec2006_int_benchmarks = [
      'perlbench_06', 'bzip2_06', 'gcc_06', 'mcf_06', 'gobmk_06', 'hmmer_06',
      'sjeng_06', 'libquantum_06', 'h264ref_06', 'omnetpp_06', 'astar_06',
      'xalancbmk_06']
  spec2006_fp_benchmarks = [
      'bwaves_06', 'gamess_06', 'milc_06', 'zeusmp_06', 'gromacs_06',
      'cactusADM_06', 'leslie3d_06', 'namd_06', 'dealII_06', 'soplex_06',
      'povray_06', 'calculix_06', 'GemsFDTD_06', 'tonto_06', 'lbm_06',
      'wrf_06', 'sphinx3_06']
  spec2006_benchmarks = spec2006_int_benchmarks + spec2006_fp_benchmarks

  spec2017_int_benchmarks = [
      'perlbench_17', 'gcc_17', 'mcf_17', 'omnetpp_17', 'xalancbmk_17',
      'x264_17', 'deepsjeng_17', 'leela_17', 'exchange2_17', 'xz_17']
  spec2017_fp_benchmarks  = [
      'bwaves_17', 'cactuBSSN_17', 'lbm_17', 'wrf_17', 'cam4_17', 'pop2_17',
      'imagick_17', 'nab_17', 'fotonik3d_17', 'roms_17']
  spec2017_benchmarks = spec2017_int_benchmarks + spec2017_fp_benchmarks

  all_benchmarks = spec2006_benchmarks + spec2017_benchmarks

  benchmark_suites_dict = {
    'spec06': spec2006_benchmarks,
    'spec06_int': spec2006_int_benchmarks,
    'spec06_fp': spec2006_fp_benchmarks,
    'spec17': spec2017_benchmarks,
    'spec17_int': spec2017_int_benchmarks,
    'spec17_fp': spec2017_fp_benchmarks,
    'spec_all': all_benchmarks,
  }
  return all_benchmarks, benchmark_suites_dict


def define_commandline_arguments(all_benchmarks, benchmark_suites_dict):
  parser = argparse.ArgumentParser(
      description='Copies SPEC and creates requried descriptor '
                  'file to generate SPEC checkpoints')

  ##################### Environment Configuration #####################
  parser.add_argument(
      '-o', '--output_dir',
      default='./checkpoints',
      help='Path to the parent directory of checkpoint directories')
  parser.add_argument(
      '-f', '--force_write',
      action='store_true',
      help='Overwrite checkpoints if they exists')

  ##################### SPEC Configuration ######################
  parser.add_argument(
      '--spec17_path',
      help='Path to SPEC2017 directory')
  parser.add_argument(
      '--spec06_path',
      help='Path to SPEC2006 directory')
  parser.add_argument(
      '-c', '--spec_config_name',
      default='*',
      help='The SPEC configuration you want to run, used by glob '
          '(the name of the directory in benchspec/CPU*/*/run/run_base_<input>*_<config_name>*)')
  parser.add_argument(
      '--inputs',
      nargs='+',
      choices=['test', 'train', 'ref'],
      default=['ref'],
      help='The list of inputs to run')

  parser_group = parser.add_mutually_exclusive_group(required=True)
  parser_group.add_argument(
      '--suite',
      choices=benchmark_suites_dict.keys(),
      help='The benchmark suites to run')
  parser_group.add_argument(
      '--benchmarks',
      nargs='+',
      choices=all_benchmarks,
      help='The list of benchmarks to run')
  return parser.parse_args()

def initialize_globals():
  global __args__
  global __benchmarks__

  all_benchmarks, benchmark_suites_dict = define_benchmarks_and_suites()
  __args__ = define_commandline_arguments(all_benchmarks, benchmark_suites_dict)
  if __args__.suite:
    __benchmarks__ = benchmark_suites_dict[__args__.suite]
  else:
    __benchmarks__ = __args__.benchmarks

def verify_spec_run_dirs_exist():
  for benchmark, input_name in product(__benchmarks__, __args__.inputs):
    find_spec_run_dir(benchmark, input_name)

def verify_workload_output_dirs_do_no_exist():
  for benchmark, input_name in product(__benchmarks__, __args__.inputs):
    workload_path = '{}/{}/{}'.format(__args__.output_dir, benchmark, input_name)
    if os.path.exists(workload_path):
      print('ERROR: output directory already exists, if you really want to '
            're-create new checkpoints, clean up the ouput directory or use -f flag.')
      sys.exit(1)

def create_run_dir_glob_search_path(benchmark, input_name):
  benchmark_name, spec_version = parse_benchmark_name_version(benchmark)
  if spec_version == '06':
    if __args__.spec06_path is None:
      print('Command-line argument spec06_path should be specified for'
            ' SPEC2006 benchmarks')
      sys.exit(1)

    path_template = (__args__.spec06_path + '/benchspec/CPU*/*{benchmark}*/run'
                                            '/run_base_{input_name}*_{config}*')

  elif spec_version == '17':
    if __args__.spec17_path is None:
      print('Command-line argument spec17_path should be specified for'
            ' SPEC2017 benchmarks')
      sys.exit(1)

    path_template = (__args__.spec17_path + '/benchspec/CPU/*{benchmark}_s/run'
                                            '/run_base_{input_name}*_{config}')

  else:
    assert False

  return path_template.format(benchmark=benchmark_name,
                              input_name=input_name,
                              config=__args__.spec_config_name)

def find_spec_run_dir(benchmark, input_name):
  glob_search_path = create_run_dir_glob_search_path(benchmark, input_name)
  glob_result = glob.glob(glob_search_path)
  if len(glob_result) == 0:
    print('Could not find the SPEC run directory for workload {},{} with '
          'glob search path {}'.format(
              benchmark, input_name, glob_search_path))
    sys.exit(1)
  elif len(glob_result) > 1:
    print('Found multiple SPEC run directories for workload {},{} with '
          'glob search path {}'.format(
              benchmark, input_name, glob_search_path))
    print('Using latest version: ', glob_result[len(glob_result)-1])
  return glob_result[len(glob_result)-1]

def setup_run_dir(benchmark, input_name, run_dir_path):
  os.makedirs(run_dir_path, exist_ok=True)
  spec_run_dir = find_spec_run_dir(benchmark, input_name)
  print("Copying {} to {} ...".format(spec_run_dir, run_dir_path))
  distutils.dir_util.copy_tree(spec_run_dir, run_dir_path)

def parse_benchmark_name_version(benchmark):
  m = re.match(r'(.+)_(\d+)', benchmark)
  if m is None:
    print('Error: ill-formed benchmark name: {}'.format(benchmark))
    sys.exit(1)

  benchmark_name = m.group(1)
  spec_version = m.group(2)
  if spec_version not in ['06', '17']:
    print('Error: Unrecognized spec version in benchmark: {}'.format(benchmark))
    sys.exit(1)

  return benchmark_name, spec_version

def extract_run_commands_spec17(speccmds_out_file_path):
  cmds = []
  with open(speccmds_out_file_path) as f:
    for line in f:
      m = re.match("child started:.*'(.*)'", line)
      if m is not None:
        orig_cmd = m.group(1)
        cmd_words = orig_cmd.split()
        cmd_words[0] = './' + os.path.basename(cmd_words[0])
        cmd = ' '.join(cmd_words)
        cmd, num_subs = re.subn(r' > .+ 2>> .+\b', r'', cmd)
        cmds.append(cmd)

        if num_subs != 1:
          print('WARNING: unexpected run command in file {}'.format(
              speccmds_out_file_path))
          print('Original command: {}'.format(orig_cmd))
          print('Transformed command: {}'.format(cmd))
  return cmds

def extract_run_commands_spec06(speccmds_out_file_path):
  cmds = []
  with open(speccmds_out_file_path) as f:
      for line in f:
        words = line.split()
        cmd_words = []
        if words[0]!="-C":
          for word in words:
            if "run_base" in word:
              temp = word.split("/")
              cmd_words.insert(0, './' + temp[2])
            elif len(cmd_words)!=0:
              cmd_words.append(word)
          if words[0]=="-i":
            cmd_words.append("< " + words[1])
          cmd = ' '.join(cmd_words)
          cmds.append(cmd)
  return cmds

def create_run_commands(run_dir_path, spec_version):
  global run_commands

  if spec_version == '17':
    run_commands = extract_run_commands_spec17(
        '{}/speccmds.out'.format(run_dir_path))
  elif spec_version == '06':
    run_commands = extract_run_commands_spec06(
        '{}/speccmds.cmd'.format(run_dir_path))
  else:
    assert False

  with open('{}/RUN_CMDS'.format(run_dir_path), 'w') as f:
    for cmd in run_commands:
      f.write(cmd + '\n')

def copy_programs():
  print('================================================')
  print('Finding required SPEC run directories...')
  verify_spec_run_dirs_exist()
  if not __args__.force_write:
    print('Verify that the output run directories do not conflict with '
          'current files')
    verify_workload_output_dirs_do_no_exist()

  print('Copying programs to output directory ...')
  for benchmark, input_name in product(__benchmarks__, __args__.inputs):
    workload_path = os.path.abspath(
        '{}/{}/{}'.format(__args__.output_dir, benchmark, input_name))
    run_dir_path = '{}/run_dir'.format(workload_path)
    setup_run_dir(benchmark, input_name, run_dir_path)
    _, spec_version = parse_benchmark_name_version(benchmark)
    create_run_commands(run_dir_path, spec_version)

def create_checkpoints_descriptor_file():
  print('================================================')
  print('Creating a descriptor file to generate checkpoints...')

  descriptor_str = 'import os\n\n'
  name_list = []
  suite_list = []
  for benchmark, input_name in product(__benchmarks__, __args__.inputs):
    run_dir_path = os.path.abspath(
        '{}/{}/{}/run_dir'.format(__args__.output_dir, benchmark, input_name))
    cmd_file_path = '{}/RUN_CMDS'.format(run_dir_path)
    with open(cmd_file_path, 'r') as f:
      run_commands = f.read().splitlines()
      bench_list = []

      for run_cmd_idx in range(len(run_commands)):
        benchmark_name = '{}_{}{}'.format(benchmark, input_name, run_cmd_idx)
        name_list.append(benchmark_name)
        descriptor_str += '{name} = Program("{name}", "{cmd}", path="{path}")\n'.format(
            name=benchmark_name, cmd=run_commands[run_cmd_idx], path=run_dir_path)
        bench_list.append(benchmark_name)

      benchmark_name = '{}_{}'.format(benchmark, input_name)
      descriptor_str += '{name} = Benchmark("{name}", [{bench_list}])\n'.format(
          name=benchmark_name, bench_list=', '.join(bench_list))
      suite_list.append(benchmark_name)
      descriptor_str += '\n'
  
  descriptor_str += '{name} = Suite("{name}", [{bench_list}])\n'.format(
      name=__args__.suite, bench_list=', '.join(suite_list))

  with open(__args__.output_dir + '/program_descriptor.def', 'w') as f:
    f.write(descriptor_str)
  return

def main():
  initialize_globals()
  copy_programs()
  create_checkpoints_descriptor_file()

if __name__ == '__main__':
  main()
