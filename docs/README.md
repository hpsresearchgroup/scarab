# Scarab

Scarab was created in collaboration with HPS, SAFARI, and Intel.

## License & Copyright

## Getting Started

## 0.0 TABLE OF CONTENTS

### Other important READMEs

* [src/pin/pin_lib/README.md](src/pin/pin_lib/README.md)
  - contains information on how to map new instructions

## 1.0 HOW TO RUN SCARAB

Note: before running Scarab, you must install [PIN 3.5](https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads).

### Running a complete program
> python ./bin/scarab_launch.py --program /bin/ls --param src/PARAMS.in

### Running in a different directory (Scarab produces multiple stats files in the running directory)
> python ./bin/scarab_launch.py --program /bin/ls --param src/PARAMS.in --sim_dir path_to_simulation_directory

### Fast forwarding to a simpoint
> python ./bin/scarab_launch.py --program /bin/ls --pintool_args='-hyper_fast_forward_count 100000'

### Starting from a checkpoint of a simpoint

This feature is not yet supported.

### Running with an instruction limit
> python ./bin/scarab_launch.py --program /bin/ls --pintool_args='-hyper_fast_forward_count 100000' --scarab_args='--inst_limit 1000'


## 2.0 OVERVIEW OF SCARAB

Scarab is an execute-at-fetch model simulator. That is to say, a functional
model executes each instruction before it is fetched. We refer to the function
simulator model as the 'frontend'. PIN serves as the frontend functional
simulator for Scarab.

Once instructions have been fetched and processed by the frontend functional
model, they are passed to Scarab to simulate timing.

Due to limitations with PIN, we chose to seperate PIN and Scarab in
seperate processes, so that they can be compiled completely independently from
one another. For each core in Scarab, a new PIN process is created in addition
to the scarab process. In other words, if a 4-core simulation is run, the 4 PIN
processes will be created, in addition to a single Scarab process, making there
5 processes total.

The easiest way to launch scarab is though the scarab_launch.py script,
provided in the bin directory.

## 3.0 HOW TO RUN SCARAB

In order to run scarab, the user must specify a param file that configures all
of the parameters in Scarab.

Scarab parameters are specified in 1 of 3 ways. The first, by the default value
specified in the parameter definition files (src/*.param.def).  The second, by
speficing a PARAMS.in file, which must be located in the same directory Scarab
is running. The third, by any command line arguements passed to Scarab.

## 4.0 SANITY CHECK USING QSORT

./utils/qsort includes a small test program for running libc's qsort() on
random data that we use for sanity checks. To run it, use:

> python ./bin/scarab_test_qsort.py path_to_results_directory

./utils/qsort/ref_stats includes sample stats that you can use for sanity
checks. The exact statistics are unlikely to match with the reference stats
because the produced binary is compiler-dependent.

## 5.0 EXECUTION-DRIVEN FRONTEND

The execution-driven frontend supports wrong-path execution, making it Scarabs
most detailed simulation mode. It is, however, much slower than the trace
frontend.

Using the script scarab_launch.py, located in the bin directory, is the
simplest way to run the execution-driven frontend. Please see the directions
above.

## 6.0 TRACE FRONTEND

The trace frontend is run in two phases.

During the first phase, PIN is used to generate a trace of the program. During
the second phase, Scarab reads and processes the trace.

The trace frontend is not currently supported by the scarab_launch.py script.
Both the trace creation and Scarab phases must be run by-hand.

## 7.0 MAPPING NEW INSTRUCTIONS

Scarab was tested on a subset of the SPEC 2017 benchmarks. It is possible (and
probably likely) that running on a new machine, using a different compiler,
running a different region of a program, or running new programs will uncover
unmapped instructions.

Please see src/pin/pin_lib/README for directions to map new instructions.

## 8.0 USING POWER MODEL
> python ./bin/scarab_launch.py --program /bin/ls --scarab_args='--power_intf_on 1'

Power and energy stats will be in the power.stat.out file.

## 9.0 LIMITATIONS OF SCARAB
* Does not simulate the timing of system level instructions.

## 10.0 SYSTEM REQUIREMENTS
* Python 3.6.3
* Perl 5 v5.16.3
* g++ 7.3.1
* gcc 7.3.1
* McPAT v1.0
* CACTI 6.5
* Intel PIN 3.5
* Clang 5.0.1

Note: Lower or higher versions of the above software may work, but was not tested.

## 11.0 KNOWN ISSUES
1. Unmapped Instructions on untested machines
2. Ramulator Power model not implemented
3. Ramulator params (ramulator.param.def) not effecting model
4. TODO comments in src/power_scarab_config.cc
5. Random crashing issue. Current solution is to rerun Scarab.
6. No checkpoints, hyper fast forward is provided as a temporary solution.
7. Trace frontend support in scarab_launch.py

## 12.0 CONTRIBUTORS

We would like to thank all past and present members of HPS for contributing to
Scarab.
