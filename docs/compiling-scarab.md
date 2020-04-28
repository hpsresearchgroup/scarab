# Compiling Scarab

Before compiling Scarab, the environment variable PIN_ROOT should be set to the
path to your Intel PIN 3.5 directory. 

> export PIN_ROOT=/absolute/path/to/pin-3.5

All of the Scarab source code is located in the src directory.

To compile Scarab, use the following commands:
> cd src

> make

This will make the optimized binary for Scarab.

Alternatively, if you would like to generate the debugging binary for Scarab,
use the following commands:
> make dbg

## Other relevant pages

For more information, please see our auto-generated
[Makefile documentation.](autogen-scarab-makefile-docs.md)

* Please run this command in the root directory to auto-generate documentation.
> make -C docs
