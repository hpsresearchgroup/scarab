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
import subprocess
import tempfile
import shutil

sys.path.append(os.path.dirname(__file__))
import command

def warn (warning_msg):
  print("Warning: " + warning_msg)
  sys.stdout.flush()

def error(error_msg):
  print("Error: " + error_msg)
  sys.exit(1)

def get_disable_aslr_prefix():
  uname_cmd = command.Command("uname -m", stdout_fp=subprocess.PIPE, stderr_fp=subprocess.PIPE)
  uname_cmd.run_in_background()
  uname_output, uname_err = uname_cmd.process.communicate()

  uname_output = uname_output.rstrip()
  disable_aslr_cmd = 'setarch {uname_output} -R'.format(uname_output=uname_output.decode('utf-8'))

  return disable_aslr_cmd

def recursive_copy(src_dir, dest_dir):
  """
  Recursively copy each child of src_dir to the dest_dir.
  If child exists, do not copy again. Instead silently skip
  over child.
  """

  assert_path_exists(src_dir)
  assert_path_exists(dest_dir)

  for child in os.listdir(src_dir):
    child_filename = os.fsdecode(child)
    dest_path = dest_dir + "/" + child_filename
    if not os.path.exists(dest_path):
      child_path = src_dir + "/" + child_filename
      if child_path != os.path.commonpath([dest_path, child_path]):
        if (os.path.isdir(child_path)):
          #print("RECURSIVE COPY:", child_path, dest_path)
          shutil.copytree(child_path, dest_path)
        else:
          #print("COPY:", child_path, dest_path)
          shutil.copy2(child_path, dest_path)

def assert_path_exists(assert_path):
  assert os.path.exists(assert_path), "Error: Path does not exist: {}".format(assert_path)

def get_temp_socket_path():
  temp_prefix = "Scarab_Temp_WorkDir_"
  socket_path = os.path.join(tempfile.mkdtemp(prefix=temp_prefix), 'socket')
  return socket_path

class ProgressBar:
  def __init__(self, banner, total_count):
    self.bar_char = '='
    self.edge_char = '|'
    self.max_bar_length = 10
    self.banner = banner
    self.total_count = total_count
    self.count = 0

    self._print_current_progress()

  def _print_current_progress(self):
    bar_length = int(self.count * self.max_bar_length / self.total_count)
    bar        =  ''.join(bar_length * [self.bar_char])
    not_bar    =  ''.join((self.max_bar_length - bar_length) * [' '])
    end        = "complete.\n" if self.total_count == self.count else ""
    progress   = "\r{banner}: {edge}{bar}{not_bar}{edge} {end}".format(
      banner=self.banner,
      edge=self.edge_char,
      bar=bar,
      not_bar=not_bar,
      end=end)
    print(progress, flush=True, end='')

  def add(self, x):
    self.count += x
    self._print_current_progress()
    return self
