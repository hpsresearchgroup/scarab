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
import sys
import glob
import re
import argparse
import pandas as pd
import numpy as np

parser = argparse.ArgumentParser(description="Scarab Batch")
parser.add_argument('results_dir', help="Results directory to parse stats.")
parser.add_argument('--stat', default=None, action='append', help="Name of stat to return.")
parser.add_argument('--core_id', type=int, action='append', default=None, help="Core of stat files to parse.")

class StatConfig:
  stat_name_header = "Stat"
  stat_file_header = "File"
  core_header = "Core "
  weight_header = "Weight"
  benchmark_header = "Benchmark"
  job_name_header = "Job Name"

  @staticmethod
  def get_core_header(core_id):
    return StatConfig.core_header + str(core_id)

#Sets default to print all columns, regardless of terminal size
pd.set_option('display.expand_frame_repr', False)
pd.set_option('display.max_rows', None)
pd.set_option('display.max_columns', None)
pd.set_option('display.width', None)
pd.set_option('display.max_colwidth', None)

#####################################################################
# Stat Hierarchy
#####################################################################

class StatFrame:
  """
  A collection of SuiteStats, BenchmarkStats, and/or ExecutableStats.

  Each item represents a different jobs. Sub-items with the same label
  are treated as the same Suite/Benchmark/Executable and the resulting
  stats are intersected before being presented in a unified grid.

  suite1      Bench1  Bench2  Bench3 ....
  job1_name
  job2_name

  suite2      Bench1  Bench2  Bench3 ....
  job1_name
  job2_name
  """
  def __init__(self):
    self.collection = {}
    self.df = {}

  def push(self, job_name, suite_stat):
    """
    Args:
      job_name: Name of the job (row) which all stats are associated with
      suite_stat: The SuiteStat/BenchmarkStat/ExecutableStat associated with that job
    """
    if not suite_stat.label in self.collection:
      self.collection[suite_stat.label] = []
    self.collection[suite_stat.label].append({"job_name":job_name, "suite_stat":suite_stat})

  def get(self, stat_name=None, core_id=None):
    self.df = {}

    for suite_label in self.collection:
      flat_collection = {}

      for suite_stat_obj in self.collection[suite_label]:
        job_name = suite_stat_obj["job_name"]
        suite_stat = suite_stat_obj["suite_stat"]
        if not suite_stat.no_stat_files:
          flat_collection[job_name] = suite_stat.get(core_id=core_id, stat_name=stat_name)
          flat_collection[job_name][StatConfig.job_name_header] = job_name
          flat_collection[job_name].reset_index(level=flat_collection[job_name].index.names, inplace=True)

      self.df[suite_label] = pd.concat(flat_collection, sort=True, ignore_index=True)
      self.df[suite_label].set_index([StatConfig.job_name_header, StatConfig.benchmark_header, StatConfig.stat_name_header], drop=True, inplace=True)

    return self.df

  def get_frame(self, stat_name, core_id=[0], suites=None, job_list=slice(None), bench_list=slice(None)):
    self.get(stat_name=stat_name, core_id=core_id)

    if not suites:
      suites = list(self.df.keys())

    df_list = []
    for suite in suites:
      df = self.df[suite].loc[(job_list, bench_list)]

      if len(stat_name) == 1:
        df.reset_index(level=[StatConfig.stat_name_header], drop=True, inplace=True)

      df = df.unstack(level=StatConfig.benchmark_header)
      df_list.append(df)
    return df_list

  def rollup(self):
    for suite_label in self.collection:
      for i in range(len(self.collection[suite_label])):
        self.collection[suite_label][i]["suite_stat"].rollup()

  def intersect(self):
    for suite_label in self.collection:
      for i in range(len(self.collection[suite_label])):
        for j in range(i+1, len(self.collection[suite_label])):
          self.collection[suite_label][i]["suite_stat"].intersect( self.collection[suite_label][j]["suite_stat"] )

class StatCollection:
  """
  """
  def __init__(self, label=None, collection=[]):
    self.label = label
    self.flat = True
    self.no_stat_files = True
    self.collection = {}

    for c in collection:
      self.collection[c.label] = c
      if not c.no_stat_files:
        self.no_stat_files = False

  def push(self, c):
    self.collection[c.label] = c
    if not c.no_stat_files:
      self.no_stat_files = False

  def get(self, stat_name=None, core_id=None, benchmark_prefix=""):
    flat_collection = {}

    if self.no_stat_files:
      return pd.DataFrame()

    for label in self.collection:
      if not self.collection[label].no_stat_files:
        flat_collection[label] = self.collection[label].get(core_id=core_id, stat_name=stat_name)
        flat_collection[label].reset_index(level=flat_collection[label].index.names, inplace=True)

    self.df = pd.concat(flat_collection, sort=True, ignore_index=True)
    self.df[StatConfig.benchmark_header] = benchmark_prefix + self.df[StatConfig.benchmark_header]
    self.df.set_index([StatConfig.benchmark_header, StatConfig.stat_name_header], drop=True, inplace=True)
    return self.df

  def rollup(self):
    for label in self.collection:
      self.collection[label].rollup()

  def intersect(self, rhs):
    assert type(self) == type(rhs), "Cannot intersect objects of different type"

    if self.no_stat_files or rhs.no_stat_files:
      self.no_stat_files = True
      rhs.no_stat_files = True

    for rhs_label in rhs.collection:
        if rhs_label in self.collection:
          rhs.collection[rhs_label].intersect(self.collection[rhs_label])

class SuiteStat(StatCollection):
  """
  A collection of BenchmarkStats and/or ExecutableStats.

  Each item in collection is a unique run or runs of scarab.

                Bench1  Bench2  Bench3 ...
  suite_name
  """
  pass

class BenchmarkStat(StatCollection):
  """
  A group of stats that should be combined using a weighted average.
  """
  def get(self, stat_name=None, core_id=None):
    if self.flat:
      return super().get(stat_name=stat_name, core_id=core_id, benchmark_prefix="{}::".format(self.label))
    else:
      df = self.df_wrapper.get(stat_name=stat_name, core_id=core_id)
      df.reset_index(level=df.index.names, inplace=True)
      df[StatConfig.benchmark_header] = self.label
      df.set_index([StatConfig.benchmark_header, StatConfig.stat_name_header], drop=True, inplace=True)
      if StatConfig.stat_file_header in df:
        df[StatConfig.stat_file_header] = "---"
      return df

  def rollup(self):
    self.flat = False
    first = True

    self.df_wrapper = DataFrameWrapper(0.0, self.label)

    for label in self.collection:
      c = self.collection[label]

      if c.df_wrapper.no_stat_files:
        continue

      c.df_wrapper.apply_weight()
      if first:
        self.df_wrapper = c.df_wrapper
        first = False
      else:
        self.df_wrapper.accumulate(c.df_wrapper)

    if not first:
      self.df_wrapper.normalize()

    self.df_wrapper.label = self.label


class ExecutableStat:
  """
  Parse all the stats files in the results directory.

  The ExecutableStat object lines up with the Checkpoint or Program object.
  """
  def __init__(self, results_dir, weight=1.0, label=None):
    self.label = label
    self.weight = weight
    self.results_dir = os.path.abspath(results_dir)

    stat_file_parser = StatFileParser(self.results_dir, self.weight, self.label)
    self.df_wrapper = stat_file_parser.build_df()
    self.no_stat_files = self.df_wrapper.no_stat_files

  def get(self, stat_name=None, core_id=None):
    return self.df_wrapper.get(stat_name=stat_name, core_id=core_id)

  def rollup(self):
    return

  def intersect(self, rhs):
    if self.no_stat_files or rhs.no_stat_files:
      self.no_stat_files = True
      rhs.no_stat_files = True


#####################################################################
# Helper Objects
#####################################################################

class DataFrameWrapper:
  def __init__(self, weight, label):
    self.columns = []
    self.df = None
    self.no_stat_files = True
    self.weight = weight
    self.label = label

  def build_df(self, label, stat_names, stat_values, stat_weights):
    self.df = pd.DataFrame(data=stat_names)

    for core in stat_values:
      self.columns.append(StatConfig.get_core_header(core))
      self.df[StatConfig.get_core_header(core)] = self.df[StatConfig.stat_name_header].map(stat_values[core])
    self.df[StatConfig.weight_header] = self.df[StatConfig.stat_name_header].map(stat_weights)
    self.df[StatConfig.benchmark_header] = label
    self.df.set_index([StatConfig.benchmark_header, StatConfig.stat_name_header], drop=True, inplace=True)

  def apply_weight(self):
    for col in self.columns:
      self.df[col] *= self.df[StatConfig.weight_header]

  def normalize(self):
    for col in self.columns:
      self.df[col] /= self.df[StatConfig.weight_header]

  def accumulate(self, rhs):
    self.weight += rhs.weight
    self.df[self.colums] += rhs.df[self.columns]
    self.df[StatConfig.weight_header] += rhs.df[StatConfig.weight_header]

  def get(self, stat_name=None, core_id=None):
    """
    Returns a copy of the selected rows and columns of a DataFrame. If no rows
    or columns are selected, then a copy of the entire DataFrame is returned.
    """
    if self.no_stat_files:
      return pd.DataFrame()

    parsed_core_id = self._parse_core_params(core_id)
    parsed_stat_name = self._parse_stat_params(stat_name)

    try:
      df_copy = self.df.loc[(slice(None), parsed_stat_name), parsed_core_id].copy()
    except Exception as e:
      print("Error: Could not index dataframe {label} (containing columns {columns}) with stat_name={stat_name} and core={core_id}".format(
        label=self.label, columns=self.columns, stat_name=parsed_stat_name, core_id=core_id))
      print("Exception: " + str(e))
      sys.exit(1)

    return df_copy

  def _parse_stat_params(self, stat_params):
    stat_rows = []
    if stat_params:
      for stat in stat_params:
        if "=" in stat:
          stat_name = stat.split("=")[0].strip()
          self._process_equation_stat(stat_name, stat)
          stat_rows.append(stat_name)
        else:
          stat_rows.append(stat)
    else:
      stat_rows = slice(None)
    return stat_rows

  def _parse_core_params(self, core_params):
    core_columns = []
    if core_params:
      for core in core_params:
        core_header = StatConfig.get_core_header(core)
        core_columns.append(core_header)
    else:
      core_columns = slice(None)
    return core_columns

  def _process_equation_stat(self, stat_name, equation):
    insert_list = []
    insert_idx = []
    stat_values = self.df.reset_index(level=0).to_dict()
    for core_id in self.columns:
      exec(equation, stat_values[core_id], stat_values[core_id])
      insert_idx.append(core_id)
      insert_list.append(stat_values[core_id][stat_name])
    insert_idx.append(StatConfig.weight_header)
    insert_list.append(self.weight)
    insert_idx.append(StatConfig.stat_file_header)
    insert_list.append("Equation")
    self.df.loc[(self.label, stat_name), insert_idx] = insert_list

class StatFileParser:
  """
  Read all stats files in the results_dir into a Pandas DataFrame
  """
  def __init__(self, results_dir, weight, label):
    self.results_dir = results_dir
    self.weight = weight
    self.label = label
    self.df_wrapper = DataFrameWrapper(self.weight, self.label)

    self.stat_values = {}
    self.stat_weights = {}
    self.stat_names = {StatConfig.stat_name_header:[], StatConfig.stat_file_header:[]}

  def _add_stat(self, core_id, stat, value, weight, statsfile):
    if not core_id in self.stat_values:
      self.stat_values[core_id] = {}

    self.stat_values[core_id][stat] = value;
    self.stat_weights[stat] = self.weight;

    if core_id == 0:
      # Only add stat names and files once, always for core 0
      self.stat_names[StatConfig.stat_name_header].append(stat)
      self.stat_names[StatConfig.stat_file_header].append(statsfile)


  def _is_stat_line(self, stat_str):
    pattern = '([^\s]+)\s+([0-9]+)\s+([0-9.]+)?[%]?\s+([0-9]+)\s+([0-9.]+)?[%]?'
    #pattern = '^([^\s]+)\s+([0-9]+).*$'
    m = re.search(pattern, stat_str)
    return m

  def _parse_stat(self, stat_str):
    m = self._is_stat_line(stat_str)
    return m.group(1), int(m.group(2))

  def _parse_stats_file(self, statsfile, core_id):
    try:
      with open(statsfile) as fp:
        for line in fp:
          if self._is_stat_line(line):
            stat, value = self._parse_stat(line);
            self._add_stat(core_id, stat, value, self.weight, statsfile)
    except Exception as e:
      print(str(e))

  def _read_all_stats_for_simpoint(self):
    """
    We assume that either all stats were generated or none of them were.
    """
    stats_file_list = glob.glob(os.path.join(self.results_dir, "*.stat.*.out"))
    if len(stats_file_list) == 0:
      print("Warning: could not find stats files for {} : Skipping...".format(self.results_dir))
    else:
      self.df_wrapper.no_stat_files = False

    for stats_file in stats_file_list:
      m = re.search('[.]stat[.]([0-9]+)[.]out', stats_file)
      if m:
        stats_file_name = os.path.join(self.results_dir, stats_file)
        core_id = int(m.group(1))
        self._parse_stats_file(stats_file_name, core_id)

  def build_df(self):
    self._read_all_stats_for_simpoint()
    self.df_wrapper.build_df(self.label, self.stat_names, self.stat_values, self.stat_weights)
    return self.df_wrapper


#####################################################################
#####################################################################

def __main():
  args = parser.parse_args()
  stat = ExecutableStat(results_dir=args.results_dir, label="Stat")
  print(stat.get(core_id=args.core_id, stat_name=args.stat))

if __name__ == "__main__":
  __main()
