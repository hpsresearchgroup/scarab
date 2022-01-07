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
Date: 04/27/2020
Description: Globally tracks objects declared in Scarab Batch jobfiles.

The purpose of this file is to provide an interface for globally tracking all declared objects. 
This is useful because objects are declared by users in jobfiles, and are usually not directly
operated on by the user. Typical use cases are the user directs Scarab Batch on how to operate
on the objects. Scarab Batch uses the globally tracked objects and the directives from the user
(e.g., run, progress, stat) to perform the appropriate task.
"""

import sys
import os
sys.path.append(os.path.dirname(__file__))
from scarab_batch_types import *
from batch_manager import *
from command import *
import scarab_stats

class ObjectManager:
    def __init__(self):
        self.pool = []

    def register(self, obj):
        self.pool.append(obj)

class ScarabRunManager(ObjectManager):
    def __init__(self):
      super().__init__()
      self.batch_manager = None

    def register_batch_manager(self, batch_manager):
      self.batch_manager = batch_manager

    def make(self):
      for job in self.pool:
        job.create_snapshot()
        job.make()

    def run(self):
      """
      If a BatchManager has not been specified, then create a local BatchManager
      If no phase has been specified, then create a phase and add all known ScarabRuns objects to phase
      """
      if not self.batch_manager:
        self.batch_manager = BatchManager()
      if len(self.batch_manager.phase_list) == 0:
        self.batch_manager.phase_list.append(Phase(self.pool))

      self.batch_manager.run()

    def print_progress(self):
      for job in self.pool:
        job.print_progress()

    def get_stats(self, flat=False):
      results = scarab_stats.StatRun("Scarab Stats")
      for job in self.pool:
        results.append(job.job_name, job.get_stats(flat=flat))
      return results

    def print_commands(self):
      for job in self.pool:
        job.print_commands()

# Declare global objects:
scarab_run_manager = ScarabRunManager()
program_manager = ObjectManager()
checkpoint_manager = ObjectManager()
trace_manager = ObjectManager()
mix_manager = ObjectManager()
collection_manager = ObjectManager()
