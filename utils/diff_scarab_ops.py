#!/usr/bin/env python3

"""
This script compares the output of two Scarab runs and produces a histogram of differences in Ops produced by the frontend.
It expects that Scarab has been run with the following arguments:
   --fetch_off_path_ops 0 --debug_inst_start <START> --debug_inst_stop <STOP> --debug_op_fields 1 
The two file in 
"""

import argparse
from collections import defaultdict
import sys

def parse_args():
  parser = argparse.ArgumentParser(description='Create a histogram of differences in Scarab Ops.')
  parser.add_argument('file1_path', help='Path to the first Scarab standard output file')
  parser.add_argument('file2_path', help='Path to the second Scarab standard output file')
  return parser.parse_args()

class LogReader:
  def __init__(self, path):
    self.path = path

  def __iter__(self):
    with open(self.path) as f:
      for i, line in enumerate(f):
        if not line.strip().startswith('DEBUG_OP_FIELDS'): continue
        yield i, line.strip()

def main():
  args = parse_args()

  histogram = defaultdict(lambda: defaultdict(lambda: 0))
  bucket_contents = defaultdict(lambda: defaultdict(lambda: []))

  for (line1_number, line1), (line2_number, line2) in zip(LogReader(args.file1_path), LogReader(args.file2_path)):
    if line1 != line2:
      words = line1.split()
      assert words[0] == 'DEBUG_OP_FIELDS'
      assert len(words) >= 2
      hist_key = words[1]

      if hist_key == 'src':
        second_key = ' '.join(words[3:])
      elif hist_key == 'mem_type':
        second_key = ' '.join([words[4], words[6]])
      elif hist_key == 'simd:':
        second_key = ' '.join(words[2:])
      else:
        second_key = 'other'
      
      histogram[hist_key]['total_count'] += 1
      histogram[hist_key][second_key] += 1
      bucket_contents[hist_key][second_key].append(f'{line1_number}: {line1} --- {line2_number}: {line2}')

  for hist_key, second_histogram in histogram.items():
    print(f'{hist_key}: {second_histogram["total_count"]}')
    for second_key, number in second_histogram.items():
      if second_key == 'total_count': continue
      print(f'---- {second_key}: {number}')

  for hist_key, second_histogram in bucket_contents.items():
    for second_key, diffs in second_histogram.items():
      print(f'========= diffs for {hist_key} {second_key} ========')
      print("\n".join(diffs))


main()
