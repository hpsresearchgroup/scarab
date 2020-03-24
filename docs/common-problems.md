# Common Problems
## MAPPING NEW INSTRUCTIONS

Scarab was tested on a subset of the SPEC 2017 benchmarks. It is possible (and
probably likely) that running on a new machine, using a different compiler,
running a different region of a program, or running new programs will uncover
unmapped instructions.

Please see the [directions](../src/pin/pin_lib/README.md) to map new instructions.

## USING POWER MODEL
> python ./bin/scarab_launch.py --program /bin/ls --scarab_args='--power_intf_on 1'

Power and energy stats will be in the power.stat.out file.
