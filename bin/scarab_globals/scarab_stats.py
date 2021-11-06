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

os.environ['OPENBLAS_NUM_THREADS'] = '1'
import pandas as pd
import numpy as np
from scipy import stats

sys.path.append(os.path.dirname(__file__))
from scarab_utils import *

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
pd.set_option('display.float_format', "{:,.2f}".format)

print_warnings = True

#####################################################################
# Stat Hierarchy
#####################################################################

class StatDF:
  def __init__(self, df):
    self.df = df

  def print(self):
    print(self.df)

  def speedup(self, base):
    if not base in self.df.index:
      error("Count not find base index {}".format(base))
    self.df.loc[:] = self.df.loc[:].div(self.df.loc[base])
    return self

  def improvement(self, base):
    if not base in self.df.index:
      error("Count not find base index {}".format(base))
    self.df.loc[:] = (self.df.loc[base] - self.df.loc[:]) / self.df.loc[base]
    return self

  def amean(self):
    """Compute amean ignoreing NaN values"""
    self.df['amean'] = self.df.dropna(axis=1).mean(numeric_only=True, axis=1)
    return self

  def gmean(self):
    """Compute gmean ignoreing NaN values"""
    self.df['gmean'] = stats.gmean(self.df.dropna(axis=1).iloc[:], axis=1)

class StatCollection:
  """A collection (1 or more) of StatFrames.
  
  Provides:
    1. An interface for averaging together StatFrames.
  """
  def __init__(self, name, frame_list=[]):
    self.name = name
    self.frame_list = frame_list[:]

  def __str__(self):
    return ", ".join([str(f) for f in self.frame_list])

  def append(self, frame):
    self.frame_list.append(frame)

  def accumulate(self):
    stat_frame = None

    for f in self.frame_list:
      if not stat_frame:
        stat_frame = f
      else:
        stat_frame += f

    stat_frame.name = self.name
    return stat_frame

  def normalize(self):
    if len(self.frame_list) > 0:
      self.frame_list = [ x.normalize() for x in self.frame_list ]
    return self

  def apply_weight(self, weight):
    if len(self.frame_list) > 0:
      self.frame_list = [ x.apply_weight(weight) for x in self.frame_list ]
    return self

  def get(self, stat_name=None, core_id=None):
    combined_df = pd.DataFrame()
    for frame in self.frame_list:
      df = frame.get(stat_name=stat_name, core_id=core_id).df

      for column in df:
        frame_name = StatRun._generate_name(
          frame.name,
          column,
          core_id
          )
        combined_df.loc[:, frame_name] = df[column].copy()

    return StatDF(combined_df.copy())

  def sort_names_by_stat(self, stat_name, core_id, cutoff=None):
    stat_name_ = stat_name.split('=')[0]
    stat_df = self.get([stat_name], [core_id])
    stat_df.df.sort_values(stat_name_, axis='columns', ascending=False, inplace=True)

    if cutoff:
      stat_df.df = stat_df.df.T[stat_df.df.T[stat_name_] > cutoff].T

    return stat_df.df.columns.tolist()

class StatRun(StatCollection):
  def append(self, frame_name, frame):
    frame.name = frame_name
    self.frame_list.append(frame)

  def get(self, stat_name=None, core_id=None):
    combined_df = pd.DataFrame()
    for frame in self.frame_list:
      df = frame.get(stat_name=stat_name, core_id=core_id).df

      for index, row in df.iterrows():
        frame_name = StatRun._generate_name(
          frame.name,
          index,
          stat_name
          )
        combined_df.loc[:, frame_name] = row.copy()

    return StatDF(combined_df.T.copy())
  
  @staticmethod
  def _generate_name(frame_name, stat, stat_list):
    if not stat_list is None:
      if len(stat_list) > 1:
        frame_name = "{frame_name}:{stat_name}".format(
          frame_name=frame_name,
          stat_name=stat
        )
    return frame_name

class StatFrame:
  """A low level container, which holds all stats from a single run of Scarab.

     Implements the following functionality:
       1. Parses stats from Scarab results directory.
       2. Generates new stats based on user-defined equations of existing stats.
       3. Intersect results from other scarab batch runs (Remove results that do not exist in both runs)
       4. Apply weights to stats, accumulate with other StatFrames
  """
  def __init__(self, name, results_dir, weight=1.0):
    """Initialize the StatFrame by parsing the stats file and building the Pandas DF

    Args:
        results_dir (string): The path to the Scarab results directory
    """
    self.name = name
    self.results_dir = os.path.abspath(results_dir)
    self.weight = weight

    self.stat_metadata_df = None
    self.stat_df = None
    self._build_df()

  def __str__(self):
    return self.results_dir

  def _build_df(self):
    """Parse stat file. Generate two pandas dataframes:
        1. stat_df: each row is a stat, each column is a core. Each cell contains the float value of the stat
        2. stat_metadata_df: each row is a stat. Columns are metadata about stat (e.g., what file the stat is from)
    """
    stat_file_parser = StatFileParser(self.results_dir)
    self.stat_metadata_df = pd.DataFrame(data=stat_file_parser.stat_names)
    self.stat_df = pd.DataFrame(data=self.stat_metadata_df.loc[:, StatConfig.stat_name_header].copy())

    for core in stat_file_parser.stat_values:
      self.stat_df[StatConfig.get_core_header(core)] = self.stat_df[StatConfig.stat_name_header].map(stat_file_parser.stat_values[core])

    self.stat_df.set_index(StatConfig.stat_name_header, drop=True, inplace=True)
    self.stat_metadata_df.set_index(StatConfig.stat_name_header, drop=True, inplace=True)

  def apply_weight(self, weight=1.0):
    """Multiply all stat values by weight

    The total weight applied to the StatFrame is tracked so StatFrame can later be normalized.

    Args:
        weight (float): The value of the weight to apply
    """
    self.weight *= weight
    self.stat_df = self.stat_df.mul(self.weight)
    return self

  def normalize(self):
    """Divide the statframe by the total weight applied to the StateFrame.
    This function sets weight back to 1.0 (i.e., the normalized value)
    """
    self.stat_df = self.stat_df.div(self.weight)
    self.weight = 1.0
    return self

  def __add__(self, rhs):
    """Implements python add operator
    """
    return self.add(rhs)

  def add(self, rhs):
    """Accumulate with another statframe.

    Note, rows and columns are assumed to be in the same order.

    Args:
        rhs (StatFrame): The StatFrame on the RHS on the + sign
    """
    self.weight += rhs.weight
    self.stat_df = self.stat_df.add(rhs.stat_df)
    return self

  def get(self, stat_name=None, core_id=None):
    """Returns a copy of the selected rows and columns of a DataFrame. If no rows
    or columns are selected, then a copy of the entire DataFrame is returned.

    Args:
        stat_name (list of strings, optional): A list of strings containing either stat names or equations (<stat_name>=...). Defaults to None (i.e., all stats).
        core_id (list of integers, optional): the core id. Defaults to None (i.e., all cores).

    Returns:
        DataFrame: A Pandas DF containing the requested slice
    """
    parsed_core_id = self._parse_core_params(core_id)
    parsed_stat_name = self._parse_stat_params(stat_name)

    if self.stat_df is None:
      return StatDF(self._generate_empty_df(parsed_stat_name, parsed_core_id))

    try:
      df_copy = self.stat_df.loc[parsed_stat_name, parsed_core_id].copy()
    except Exception as e:
      if print_warnings:
        warn("Could not index dataframe {results_dir} with stat_name={stat_name} and core={core_id}".format(
          results_dir=self.results_dir, stat_name=parsed_stat_name, core_id=core_id))
      #print("Exception: " + str(e))
      df_copy = self._generate_empty_df(parsed_stat_name, parsed_core_id).copy()

    return StatDF(df_copy)

  def _generate_empty_df(self, stat_name, core_id):
    return pd.DataFrame(data=np.nan, index=stat_name, columns=core_id)

  def _parse_stat_params(self, stat_params):
    """Parses all stat names in list. If an equation is found, then the equation is processed and a new stat is added to the DF.

    Note: equations are always of the format <stat_name>=..., therefore if a stat contains an '=' then it is assumed to be an equation.

    If stat_params is set to None, then that is converted to slice(None), which will select everything in the DF

    Args:
        stat_params (List of strings): A list of strings containing either a stat name or an equation.

    Returns:
        [type]: [description]
    """
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
    """Convert integer core ids into column names

    Note: if none is supplied then it is converted to slice(None), which will select everything in the DF.

    Args:
        core_params (list of integers): a list of integer core ids

    Returns:
        [type]: a list of column headers
    """
    core_columns = []
    if core_params:
      for core in core_params:
        core_header = StatConfig.get_core_header(core)
        core_columns.append(core_header)
    else:
      core_columns = slice(None)
    return core_columns

  def _process_equation_stat(self, stat_name, equation):
    """Process the equation and update DF to contain new stat.

    Args:
        stat_name (string): The new stat to add to the DF
        equation (string containing = sign): The equation to compute the new stat
    """
    # Create a dictionary containing all stats, where the stat_name is the key
    # Dictionary format: {'col1': {'row1': 1, 'row2': 2}, 'col2': {'row1': 0.5, 'row2': 0.75}}
    stat_values = self.stat_df.to_dict()

    if len(stat_values) == 0:
      return #StatFrame is empty, nothing to compute

    new_stat_row = {}
    new_stat_metadata_row = {}

    # Iterate over columns (i.e. cores), and compute equation for each
    for core_id in stat_values:
      # Compute equation, placing result as entry in dictionary
      exec(equation, stat_values[core_id], stat_values[core_id])

      new_stat_row[core_id] = stat_values[core_id][stat_name]
    new_stat_metadata_row[StatConfig.stat_file_header] = "Equation"

    # Update stat in Pandas DF
    self.stat_df.loc[stat_name] = new_stat_row
    self.stat_metadata_df.loc[stat_name] = new_stat_metadata_row

class StatFileParser:
  """A low level container, which holds all stats from a single run of Scarab.

     Implements the following functionality:
       1. Parses stats from Scarab results directory.
       2. Generates new stats based on user-defined equations of existing stats.
       3. Intersect results from other scarab batch runs (Remove results that do not exist in both runs)
       4. Apply weights to stats, accumulate with other StatFrames
  """
  def __init__(self, results_dir):
    """Set up StatFrame Object:
    
    Internal Variables:
        no_stat_files (Boolean): True if 1) have not parsed stats files yet or 2) No stats files exist. 
        stat_valuese (Dictionary[core_id][stat_name]): A dictionary holding all the parsed stats.
        Will be converted to Pandas DB once stats have been parsed, but faster to use dictionary while parsing.
        stat_names (Dictionary): A dictionary of stat names and the files the stats were found in

    Args:
        results_dir (string): The path to a Scarab results directory.
    """
    self.results_dir = os.path.abspath(results_dir)

    self.no_stat_files = True
    self.stat_values = {}
    self.stat_names = {StatConfig.stat_name_header:[], StatConfig.stat_file_header:[]}

    self._read_all_stats_for_simpoint()

  def _read_all_stats_for_simpoint(self):
    """Parse all stats in Scarab results directory into the temoprary dictionary object.
    
    Note: We assume that either all stats were generated or none were.
    """
    stats_file_list = glob.glob(os.path.join(self.results_dir, "*.stat.*.out"))

    # Check to see if any stats were generated
    if len(stats_file_list) == 0:
      if print_warnings:
        warn("No stat files for {} : Skipping...".format(self.results_dir))
    else:
      self.no_stat_files = False

    # Get core id from stat filename and parse all stats
    for stats_file in stats_file_list:
      m = re.search('[.]stat[.]([0-9]+)[.]out', stats_file)
      if m:
        stats_file_name = os.path.join(self.results_dir, stats_file)
        core_id = int(m.group(1))
        self._parse_stats_file(stats_file_name, core_id)

  def _parse_stats_file(self, statsfile, core_id):
    """Parse stats out of a single Scarab statsfile

    If there is a problem either opening the file or converting the stat to
    a float, simply print the error and continue.

    Args:
        statsfile (string): Absolute path to Scarab statfile
        core_id (int): Core ID of the statfile
    """
    try:
      with open(statsfile) as fp:
        for line in fp:
          if self._is_stat_line(line):
            stat, value = self._parse_stat(line)
            self._add_stat(core_id, stat, value, statsfile)
    except Exception as e:
      if print_warnings:
        warn("Unable to read stats file {} : ".format(statsfile) + str(e))

  def _parse_stat(self, stat_str):
    """Convert stat string to stat name and float

    Args:
        stat_str (string): A string from the Scarab statfile that contains a stat. 
        otherwise return the column not affected by reset stats.

    Returns:
        string, float: The string name of the stat, the float value of the stat
    """
    m = self._is_stat_line(stat_str)
    return m.group(1), float(m.group(4))

  def _is_stat_line(self, stat_str):
    """The regex pattern that 1) tells you if a line from the statsfile contains a stat and
       2) parses the stat value from the file.

    Note: there are 2 options of value to parse from the stat value: the column affected by
    reset stats and the column that is not affected by reset stats. By default we return the
    column that is affected by reset stats.

    Args:
        stat_str (string): The string that may or may not contain a stat
        otherwise return the column not affected by reset stats.

    Returns:
        RegEx Object: The Regex Object containing the parsed results
    """
    # A slow regex that grabs all stat values from file:
    pattern_all_values = '^([^\s]+)\s+([0-9.]+)\s+([0-9.nan-]+)?[%]?\s+([0-9.]+)\s+([0-9.nan-]+)?[%]?'

    # If we only need the reset stats, then use the faster regex...
    m = re.search(pattern_all_values, stat_str)
    return m

  def _add_stat(self, core_id, stat, value, statsfile):
    if not core_id in self.stat_values:
      self.stat_values[core_id] = {}

    self.stat_values[core_id][stat] = value

    if core_id == 0:
      # Only add stat names and files once, always for core 0
      self.stat_names[StatConfig.stat_name_header].append(stat)
      self.stat_names[StatConfig.stat_file_header].append(statsfile)


#####################################################################
#####################################################################

def __main():
  args = parser.parse_args()
  stat = StatFrame('single_frame', args.results_dir)
  print(stat.get(core_id=args.core_id, stat_name=args.stat).df)

if __name__ == "__main__":
  __main()
