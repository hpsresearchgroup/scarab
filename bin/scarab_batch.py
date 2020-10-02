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
Date: 12/03/2019
Description:
"""

import os
import argparse

from scarab_globals import *
from scarab_globals.scarab_batch_types import *
from scarab_globals.batch_manager import *
from scarab_globals.command import *
from scarab_globals.object_manager import *

parser = argparse.ArgumentParser(description="Scarab Batch")
parser.add_argument('jobfile', help="Jobfile to operator on.")
parser.add_argument('--run', action='store_true', help="Run all jobs in jobfile.")
parser.add_argument('--progress', action='store_true', help="Print progress of all jobs in jobfile.")
parser.add_argument('--stat', action='append', default=None, help="Print stat from results of all jobs in jobfile.")
parser.add_argument('--base', default=None, help="Normalize all runs to this run.")
parser.add_argument('--results_dir', nargs='*', default=None, help="Results directory(s) to parse stats from. Only valid with --stat option.")
parser.add_argument('--core', action='append', default=None, help="Core(s) to get stats for. Only valid with --stat option.")
args = parser.parse_args()

if not args.run and not args.progress and not args.stat:
  print("Usage: Must supply either --run, --stat, or --progress option.")
if args.results_dir and not args.stat:
  print("Usage: --results_dir is only valid with --stat option.")
if args.core and not args.stat:
  print("Usage: --core is only valid with --stat option.")
if args.base and not args.stat:
  print("Usage: --base is only valid with --stat option.")

###############################################

def exec_file_using_variables(path, variables):
  with open(path, 'r') as f:
    exec(f.read(), variables, variables)

def import_descriptor(path):
  exec_file_using_variables(path, globals())

def import_jobfile(jobfile):
  exec_file_using_variables(jobfile, globals())

###############################################

def run_all():
  scarab_run_manager.make()
  scarab_run_manager.run()

def show_progress():
  scarab_run_manager.print_progress()

def get_stats(stat_name, cores, results_dirs):
  if not cores:
    cores = [0]

  if not results_dirs:
    job_stat = scarab_run_manager.get_stats()
  else:
    job_stat = scarab_stats.StatFrame()
    for results_dir in results_dirs:
      basename = os.path.basename(results_dir)
      parent_dir = os.path.dirname(results_dir)
      stats = globals()[basename].get_stats(parent_dir)
      job_stat.push(parent_dir, stats)

  df = job_stat.get(stat_name, cores)
  print(df)

def __main():
  import_jobfile(args.jobfile)

  if (args.run):
    run_all()
  if (args.progress):
    show_progress()
  if (args.stat):
    get_stats(args.stat, args.core, args.results_dir)

if __name__ == "__main__":
  __main()