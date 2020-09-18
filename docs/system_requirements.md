# SYSTEM REQUIREMENTS

Please be sure that your system meets these minimum software prerequisits
before running Scarab. Below are the packages that we use to run Scarab.
Other versions of the same software may work, but have not been tested.

Scarab relies on the following software packages:

## Required Packages
* Intel [PIN 3.5](https://software.intel.com/en-us/articles/program-recordreplay-toolkit).
* g++ 7.3.1
* gcc 7.3.1
* Clang 5.0.1
* Python 3.6.3

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
