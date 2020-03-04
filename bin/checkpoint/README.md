# Checkpoint Creation

Scripts in this directory provide the interface for creating checkpoints.

### 1. Creating checkpoints for an application binary
The ```create_checkpoints.py``` can be used to generate checkpoints for any
application binary. The simplest way to use it, is to provide a checkpoint
descriptor file and the output directory that checkpoints will be placed in.

The```<scarab_root>/utils/qsort/example_checkpoint_descriptor.def``` is
provided as an example. This file has the following line:

> qsort  = Program("qsort", "./test_qsort", path=scarab_paths.utils_dir + "/qsort", copy=True)

Here qsort is defined as a program, what command should be used to run it, and the path to the binary.

The option ```"copy=True"``` is optional and should be used in combination with the ```--output_dir (-o)``` option. This way, the directory containing the application binary will be copied under the
output directory and checkpoints will be placed there. Otherwise the provided
path to the binary will be used to place the checkpoints as well (i.e. the
    output directory to the script will be ignored). This provides a convenient way
to avoid changes to the original binary directory hierarchy.

You can try generating the checkpoints for qsort using:

> $ python3 <scarab\_root>/bin/checkpoint/create\_checkpoints.py -dp <scarab\_root>/utils/qsort/example\_checkpoint\_descriptor.def -o qsort\_checkpoints

This should create the directory ```qsort_checkpoints``` directory containing the checkpoints. Additionally, the ```descriptor.def``` will be generated for the checkpoints, which can be used with scarab\_batch infrastructure to launch and execute the checkpoints for scarab simulation.

```create_checkpoints.py``` accepts further options (e.g. number of simpoints, region lenghts, etc.). Please refer to the help ```(-h)``` for more information.

### 2. Instructions to create checkpoints for SPEC benchmark suites

SPEC benchmark suites are commonly used for computer architecture research and this section explains the steps to prepare SPEC benchmark suites for checkpoint creation.

Under the checkpoint directory there is another script, ```prepare_spec_checkpoints_directory.py```, which can be used specifically for SPEC benchmark suites to prepare them for checkpoint generation. ```prepare_spec_checkpoints_directory.py``` gets the path to SPEC path ```(--spec17_path or --spec06_path)```, the spec config ```(-c SPEC_CONFIG_NAME)```, the input-set ```(--inputs)```, and the suite or set of benchmarks ```(--suite or --benchmarks)```. The output directory ```(-o)``` defines where SPEC binaries should be copied to and where the checkpoint descriptor will be placed.

The following is an example command.

>$ python3 <scarab\_root>/bin/checkpoint/prepare\_spec\_checkpoints\_directory.py --spec17\_path <path\_to\_SPEC17\_root> -c myspec17-m64.0000 --inputs ref --suite spec17\_int -o ./spec17\_checkpoints

This command will create the directory ```spec17_checkpoints``` and copy all SPEC 2017 integer benchmarks binaries there. It will also generate the ```checkpoint_descriptor.def```, which can be used with ```create_checkpoints.py``` to run the benchmarks and create the checkpoints. To do so, you can use the following command:

> $ python3 <scarab\_root>/bin/checkpoint/create\_checkpoints.py -dp ./spec17\_checkpoints/checkpoint\_descriptor.def

For more information, you can refer to the script's help ```(-h)```.
