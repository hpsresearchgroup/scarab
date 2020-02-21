# Checkpoint Tool

We provide a tool for checkpointing single-threaded Linux processes for
re-executing programs from deterministic starting states. Loading checkpoint
is a faster alternative to fast-forwarding with Scarab. Checkpoints are not
portable and cannot be moved once created. Thus, we provide scripts for
creating new checkpoints for programs.

## Requirements

* Only works in Linux.
* The checkpointed program must be linked statically.
* The checkpointed program must be single-threaded.
* The checkpointed program cannot use sockets.
* The checkpoint restores the absolute and relative paths of the files in use.
Thus, all files that the checkpointed program use must not move after the
checkpoint is created.
* The checkpointed program should not overwrite input files.

## Creating a new checkpoint.

Put all program files (binary and optional input files) in a directory. We
refer to the absolute path of this directory as *run_dir*. Prepare the command to
run the program from within the run directory (*run_command*). To create a
checkpoint at a given instruction count (*icount*), use the makefile in
utils/checkpoint/creator to create a new checkpoint in the desired absolute
path *checkpoint_path*.

> ICOUNT=icount RUN_DIR=run_dir PIN_APP_COMMAND="run_command" CHECKPOINT_PATH=checkpoint_path make checkpoint
