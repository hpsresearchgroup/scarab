
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
