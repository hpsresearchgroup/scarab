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

#!/usr/bin/python
#  Copyright 2020 HPS/SAFARI
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import os
import sys
import re

pinenvname = "PIN_ROOT"
pindir = os.getenv(pinenvname)

if pindir == None:
    sys.exit("Environment variable " + pinenvname + " does not exist")

testnames = ["retire", "recover", "redirect", "exception", "illop"]

prefixes = {
    "tool" : "rollback_",
    "test" : "",
    "log"  : ""
}

suffixes = {
    "tool" : "_test.so",
    "test" : "_test.exe",
    "log"  : "_log"
}

dirs = {
    "tool" : "obj-intel64",
    "test" : "obj-intel64",
    "log"  : "test-logs"
}

extracmds = {
    "recover" : " | grep 0x40 "
}

toolknobs = {
    "recover" : " -max_buffer_size 3 "
}

def form_filename(filetype, testname):
    return dirs[filetype] + "/" + prefixes[filetype] + testname + suffixes[filetype]

def run_testcase(testname,regenerate,keeplogfiles):
    pintool_name  = form_filename("tool", testname)
    testprog_name = form_filename("test", testname)
    logfile_name  = form_filename("log",  testname)
    extra_cmdtext = ""
    toolknob_text = ""

    if testname in extracmds.keys():
        extra_cmdtext = extracmds[testname]

    if testname in toolknobs.keys():
        toolknob_text = toolknobs[testname]

    test_cmdline = (pindir + 
                    "/pin -t " + 
                    pintool_name + 
                    toolknob_text +
                    " -- ./" + 
                    testprog_name + 
                    extra_cmdtext +
                    " &> test_templog")
    print test_cmdline
    os.system(test_cmdline)

    returnval = os.system("diff test_templog " + logfile_name)

    if returnval == 0:
        print "Test " + testname + " passed"
    else:
        print "Test " + testname + " failed"
        if regenerate:
            os.system("cp test_templog " + logfile_name)

    if keeplogfiles:
        os.system("cp test_templog log" + testname + ".testlog")
    os.system("rm test_templog")

for testcase in testnames:
    run_testcase(testcase,False,False)
