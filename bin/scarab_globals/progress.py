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
import glob
import re
import argparse
import enum

parser = argparse.ArgumentParser(description="Scarab Batch")
parser.add_argument('results_dir', help="Jobfile to operator on.")

class JobStatus(enum.IntEnum):
  HAS_NOT_STARTED = 0
  RUNNING = 1
  SUCCESS = 2
  FAIL = 3

def notify(message):
  print("Notify: {}".format(message))

def warning(message):
  print("Warning: {}".format(message))

def error(message):
  print("Error: {}".format(message))

keywords = ["Notify:", "Warning:", "Heartbeat:", "Finished:"]
failwords = ["Error:", "ASSERT"]
search_files = ["*.stdout", "*.stderr"]

class Progress:
  """
  Tracks progress of Scarab Jobs.

  Reports if Job is still running, not started, or has finished.
  If job is still running, then % complete is reported.
  If job has finished, then SUCCESS or FAIL is reported.
  """
  def __init__(self, results_dir, _search_files=[], _keywords=[], _failwords=[]):
    self.results_dir = os.path.abspath(results_dir)
    self.search_files = search_files + _search_files
    self.failwords = failwords + _failwords
    self.keywords = keywords + _keywords + self.failwords
    self.status = JobStatus.HAS_NOT_STARTED
    self.progress = 0.0 # % completed, only valid if status is Running
    self.message = ""
    self.get_progress()

  def __lt__(self, rhs):
    return self.status < rhs.status

  def __str__(self):
    return "{:<40s}: {message}".format(os.path.basename(self.results_dir), message=self.message)

  def get_progress(self):
    if not os.path.isdir(self.results_dir):
      self.message = "Results directory does not exist."
    else:
      self._read_search_files()

      if self._is_finished():
        self._get_completion_status()
      else:
        self._get_progress()

    return self.status, self.progress
      

  def _read_search_files(self):
    self.matching_lines = {}
    for keyword in self.keywords:
      self.matching_lines[keyword] = []

    for search_file_glob in self.search_files:
      for search_file in glob.glob(os.path.join(self.results_dir, search_file_glob)):
        with open(search_file) as f:
          self._parse_file_for_keywords(f)

  def _parse_file_for_keywords(self, fp):
    for line in fp:
      for keyword in self.keywords:
        m = re.search(keyword, line)
        if m:
          self.matching_lines[keyword].append(line.rstrip())

  def _is_finished(self):
    """
    scarab launch stdout will tell if job is running or has finished
     - scarab launch should always call notify with message: Scarab run terminated, cleaning up...
    """
    return len(self.matching_lines["Finished:"]) > 0
    for line in self.matching_lines["Notify:"]:
      m = re.search("Scarab run terminated", line)
      if m:
        return True
    return False

  def _has_not_started(self):
    """
    NOT_SCHEDULED: No scarab files in directory.
      -Specifically, search for PARAMS.out. Will need this to get inst_limit
    """
    self.params_out = os.path.join(self.results_dir, "PARAMS.out")
    return not os.path.exists(self.params_out)

  def _get_progress(self):
    """
    Progress % is reported in scarab stdout Heartbeat.
    No Heartbeat, but scarab files: Probably Running...
    """
    if self._has_not_started():
      self.message = "Scarab has not started."
      self.status = JobStatus.HAS_NOT_STARTED
      self.progress = 0
    else:
      if len(self.matching_lines["Heartbeat:"]) == 0:
        self.progress = 0
        self.message = "No Heartbeat found. Scarab is probably running..."
      else:
        last_heartbeat_line = self.matching_lines["Heartbeat:"][-1]
        m = re.search("Heartbeat:\s+([0-9]+)\%.*--(.*)$", last_heartbeat_line)
        self.progress = int(m.group(1))
        self.message = generate_progress_bar(self.progress, 100, m.group(2))
      self.status = JobStatus.RUNNING
      
  def _get_inst_limit(self):
    """
    Get inst limit from PARAMS.out so we can compare it to the number of instructions Scarab ran
    to be sure job completed successfully/
    """
    self.params_out = os.path.join(self.results_dir, "PARAMS.out")
    assert os.path.exists(self.params_out), "PARAMS.out file does not exist"
    self.inst_limit = 0
    with open(self.params_out) as f:
      for line in f:
        m = re.search("^--inst_limit\s+([0-9]+)", line)
        if m:
          self.inst_limit = int(m.group(1))
          break

  def _scarab_failed(self):
    """
    Finished, but Failed if:
     -Core dumped: look for .core file?
     -Failword found in results file: ASSERT, Error, etc...
       -Exit code non-zero in scarab launch stdout (Will see Error is Scarab, Warning if PIN)
     -Finish not printed or printed but instruction count is less than inst limit or cycle count is 0
    """
    core_dump_files = glob.glob(os.path.join(self.results_dir, "*core"))
    if len(core_dump_files) != 0:
      self.message = "Scarab Failed: Core Dumped."
      return True

    for failword in self.failwords:
      if len(self.matching_lines[failword]) != 0:
        self.message = "Scarab Failed.\n\t" + "\n\t".join(self.matching_lines[failword])
        return True

    for line in self.matching_lines["Finished:"]:
      m = re.search("Finished:\s+insts:([0-9]+)\s+cycles:([0-9]+)", line)
      inst_count = int(m.group(1))
      cycle_count = int(m.group(2))
      self._get_inst_limit()

      if inst_count < self.inst_limit:
        self.message = "Scarab Finished, but did not reach inst_limit: inst_limit={}, scarab reached={}".format(self.inst_limit, inst_count)
        return True

      if cycle_count == 0:
        self.message = "Scarab Finished, but cycle_count is 0"
        return True

    return False

  def _get_completion_status(self):
    """
    Finished Successfully:
     -Print Warnings
    """
    if self._scarab_failed():
      self.status = JobStatus.FAIL
      return

    self.status = JobStatus.SUCCESS
    self.message = "Scarab Succeeded."

    if len(self.matching_lines["Warning:"]):
      self.message += "\n\t" + "\n\t".join(self.matching_lines["Warning:"])

def generate_progress_bar(p, total, message):
  bar_unit = ['=']
  not_bar_unit = [' ']
  max_bar_units = 30
  num_bars = int((p * max_bar_units) / total)
  progress_bar = "[{bar}{not_bar}] - {message}".format(
      bar="".join(bar_unit*num_bars),
      not_bar="".join(not_bar_unit*(max_bar_units - num_bars)),
      message=message)
  return progress_bar

#####################################################################
#####################################################################

def __main():
  args = parser.parse_args()
  progress = Progress(args.results_dir)
  print(progress)

if __name__ == "__main__":
  __main()
