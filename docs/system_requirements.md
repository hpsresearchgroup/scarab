# SYSTEM REQUIREMENTS

Please be sure that your system meets these minimum software prerequisits
before running Scarab. Below are the packages that we use to run Scarab.
Other versions of the same software may work, but have not been tested.

Scarab relies on the following software packages:

## Required Packages
* [Intel PIN (see below)](#tested-intel-pin-versions)
* g++ 7.3.1
* gcc 7.3.1
* Clang 5.0.1
* Python 3.6.3
* [Intel XED 12.01](https://github.com/intelxed/xed/releases) (included as a git submodule)

## Tested Intel PIN versions:
* [PIN-Play 3.5](https://software.intel.com/en-us/articles/program-recordreplay-toolkit)
* [PIN 3.15](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html)

Note: Our checkpoint creater scripts only work with PIN-Play currently (due to a despendency with their simpoint scripts).

## Required Python Packages
See `$SCARAB_ROOT/bin/requirements.txt`

##### E.g. Install with pip:
```
cd $SCARAB_ROOT/bin
pip3 install -r requirements.txt
```

# Other Useful Packages

## If Running GTest
* GTest 1.6

## If Using Power Model
* Perl 5 v5.16.3
* [McPAT v1.0](http://www.hpl.hp.com/research/mcpat/)
* [CACTI 6.5](http://www.hpl.hp.com/research/cacti/)

Note: Lower or higher versions of the above software may work, but have not been tested.
