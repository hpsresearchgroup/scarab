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
Description: Create a snapshot of important Scarab information before a Batch run.

Snapshot directory contents: 
    scarab.snapshot
        -<date>-<time> of run
            -Copy of Scarab Binary
            -Copy of frontend pin tool binary
            -Git repo information
                -Branch
                -Commit hash
                -git status
                -Uncommitted changes
            -A dump of the ScarabParams python object
            -A log of all jobs launched with this configuration
                -PID/qsub-id    results_dir
"""

import os
import sys
import time
from pathlib import Path
import shutil
from pprint import pprint
import tempfile

sys.path.append(os.path.dirname(__file__))
import scarab_paths
from command import *

class SnapshotDefaults:
    toplevel_dir_name = "scarab.snapshot"
    scarab_binary = "scarab-binary"
    frontend_binary = "frontend-binary"
    snapshot_log = "job.log"
    git_status = "git.status"
    git_diff = "git.diff"
    scarab_params_file = "scarab.params"
    repo_root = scarab_paths.sim_dir

    @classmethod
    def make_snapshot_dir(cls, basename):
        snapshot_parent_path = os.path.join(basename, cls.toplevel_dir_name)
        Path(snapshot_parent_path).mkdir(parents=True, exist_ok=True)
        snapshot_path = tempfile.mkdtemp(prefix=cls.get_timestamp(), dir=snapshot_parent_path)
        return snapshot_path

    @staticmethod
    def get_timestamp():
        ts = time.gmtime()
        return time.strftime("%Y-%m-%d_%H-%M-%S__", ts)

    @classmethod
    def copy_binaries(cls, scarab_params, basename):
        scarab_binary_path = os.path.join(basename, cls.scarab_binary)
        frontend_binary_path = os.path.join(basename, cls.frontend_binary)

        shutil.copy2(scarab_params.scarab, scarab_binary_path)
        shutil.copy2(scarab_params.frontend_pin_tool, frontend_binary_path)

        scarab_params.scarab = scarab_binary_path
        scarab_params.frontend_pin_tool = frontend_binary_path

    @classmethod
    def record_git_info(cls, basename):
        git_status_cmd = "git status --branch --long --verbose --untracked-files=all"
        git_diff_command = "git diff --pretty"
        Command(git_status_cmd,
                 run_dir=cls.repo_root,
                 stdout=os.path.join(basename, cls.git_status)
               ).run()

        Command(git_diff_command,
                 run_dir=cls.repo_root,
                 stdout=os.path.join(basename, cls.git_diff)
               ).run()

        Command("git submodule foreach --recursive {}".format(git_status_cmd),
                 run_dir=cls.repo_root,
                 stdout=os.path.join(basename, "{}.submodule".format(cls.git_status))
               ).run()

        Command("git submodule foreach --recursive {}".format(git_diff_command),
                 run_dir=cls.repo_root,
                 stdout=os.path.join(basename, "{}.submodule".format(cls.git_diff))
               ).run()

def create_snapshot(scarab_params, results_dir):
    """
    Takes ScarabParams as input, produces snapshot directory and returns path.
    """
    snapshot_dir = SnapshotDefaults.make_snapshot_dir(results_dir)
    SnapshotDefaults.copy_binaries(scarab_params, snapshot_dir)
    SnapshotDefaults.record_git_info(snapshot_dir)

    with open(os.path.join(snapshot_dir, SnapshotDefaults.scarab_params_file), "w+") as fp:
        pprint(vars(scarab_params), stream=fp)

    scarab_params.snapshot_log = os.path.join(snapshot_dir, SnapshotDefaults.snapshot_log)
    print("Snapshot created: {}".format(snapshot_dir))
