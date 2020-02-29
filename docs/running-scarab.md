# Running A Single Program on Scarab

## Running QSORT

> Note: before running Scarab, please ensure that you have installed all
required software packed shown on the [system requirements](system_requirements.md) page.

We will start off by running the ```qsort``` example on Scarab.



### Running a complete program
> python ./bin/scarab_launch.py --program /bin/ls --param src/PARAMS.in

### Fast forwarding to a simpoint
> python ./bin/scarab_launch.py --program /bin/ls --pintool_args='-hyper_fast_forward_count 100000'

### Starting from a checkpoint of a simpoint

This feature is not yet supported.

### Running with an instruction limit
> python ./bin/scarab_launch.py --program /bin/ls --pintool_args='-hyper_fast_forward_count 100000' --scarab_args='--inst_limit 1000'

## The Params File

In order to run scarab, the user must specify a param file that configures all
of the parameters in Scarab.

Scarab parameters are specified in 1 of 3 ways. The first, by the default value
specified in the parameter definition files (src/*.param.def).  The second, by
speficing a PARAMS.in file, which must be located in the same directory Scarab
is running. The third, by any command line arguements passed to Scarab.

