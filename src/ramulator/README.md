# Ramulator

## 1.0 OVERVIEW OF RAMULATOR

Ramulator is a fast and cycle-accurate DRAM simulator \[1\] that supports a
wide array of commercial, as well as academic, DRAM standards:

- DDR3 (2007), DDR4 (2012)
- LPDDR3 (2012), LPDDR4 (2014)
- GDDR5 (2009)
- WIO (2011), WIO2 (2014)
- HBM (2013)
- SALP \[2\]
- TL-DRAM \[3\]
- RowClone \[4\]
- DSARP \[5\]

The initial release of Ramulator is described in the following paper:
>Y. Kim, W. Yang, O. Mutlu.
>"[**Ramulator: A Fast and Extensible DRAM
>Simulator**](https://people.inf.ethz.ch/omutlu/pub/ramulator_dram_simulator-ieee-cal15.pdf)".
>In _IEEE Computer Architecture Letters_, March 2015.

*This directory contains a version of Ramulator that has been modified to
integrate with the core microarchitecture simulator. The original source
code of Ramulator is available
[here](https://github.com/CMU-SAFARI/ramulator).*

For information on new features, along with an extensive memory
characterization using Ramulator, please read:
>S. Ghose, T. Li, N. Hajinazar, D. Senol Cali, O. Mutlu.
>"**Demystifying Complex Workload–DRAM Interactions: An Experimental
>Study**".
>In _Proceedings of the ACM International Conference on Measurement and
>Modeling of Computer Systems (SIGMETRICS)_, June 2019
>([slides](https://people.inf.ethz.ch/omutlu/pub/Workload-DRAM-Interaction-Analysis_sigmetrics19-talk.pdf)).
>To appear in _Proceedings of the ACM on Measurement and Analysis of
>Computing Systems (POMACS)_, 2019.
>[Preprint available on arXiv.](https://arxiv.org/pdf/1902.07609.pdf)

[\[1\] Kim et al. *Ramulator: A Fast and Extensible DRAM Simulator.* IEEE
CAL
2015.](https://users.ece.cmu.edu/~omutlu/pub/ramulator_dram_simulator-ieee-cal15.pdf)  
[\[2\] Kim et al. *A Case for Exploiting Subarray-Level Parallelism (SALP)
in
DRAM.* ISCA
2012.](https://users.ece.cmu.edu/~omutlu/pub/salp-dram_isca12.pdf)  
[\[3\] Lee et al. *Tiered-Latency DRAM: A Low Latency and Low Cost DRAM
Architecture.* HPCA
2013.](https://users.ece.cmu.edu/~omutlu/pub/tldram_hpca13.pdf)  
[\[4\] Seshadri et al. *RowClone: Fast and Energy-Efficient In-DRAM Bulk
Data
Copy and Initialization.* MICRO
2013.](https://users.ece.cmu.edu/~omutlu/pub/rowclone_micro13.pdf)  
[\[5\] Chang et al. *Improving DRAM Performance by Parallelizing Refreshes
with
Accesses.* HPCA
2014.](https://users.ece.cmu.edu/~omutlu/pub/dram-access-refresh-parallelization_hpca14.pdf)

## 2.0 KEY CONFIGURATION PARAMETERS

We highlight the key configuration parameters of Ramulator that users often
customize. The entire list of the Ramulator parameters is available in
`src/ramulator.param.def`.

### DRAM Standard Parameters
* `RAMULATOR_STANDARD`: Select a DRAM standard (e.g., DDR4, DDR3, LPDDR4).
* `RAMULATOR_ORG`: Each DRAM standard has preset DRAM organizations, which
  can be found in the corresponding header file (e.g., DDR4.h). The preset
organization can be modified by setting the DRAM Organization Parameters
below. 
* `RAMULATOR_SPEED`: Each DRAM standard has preset DRAM timings, which can
  be found in the corresponding header file (e.g., DDR4.h). The preset
timings can be modified by setting the DRAM Timing Parameters below.


### DRAM Organization Parameters
* `RAMULATOR_CHANNELS`: The number of DRAM channels. The current implementation
  instantiates a memory controller for each channel.
* `RAMULATOR_RANKS`: The number of DRAM ranks per channel.
* `RAMULATOR_BANKGROUPS`: The number of DRAM bank-groups per rank. Note that not
  all DRAM standards support bank-groups.
* `RAMULATOR_BANKS`: The number of DRAM banks per rank. If applicable, this is
  the number of banks per bank-group.
* `RAMULATOR_ROWS`: The number of DRAM rows per bank.
* `RAMULATOR_COLS`: The number of columns in a row.
* `RAMULATOR_CHIP_WIDTH`: Data bus width of a single DRAM chip.
* `BUS_WIDTH_IN_BYTES`: Data bus width of a DRAM channel. Ramulator
  instantiates (BUS_WIDTH_IN_BYTES*8)/RAMULATOR_CHIP_WIDTH DRAM chips to
meet the channel bus width.


### DRAM Timing Parameters

* `RAMULATOR_TCK`: The cycle time of the DRAM channel bus clock (i.e., CK) defined in femtoseconds.

All of the timing parameters below are defined as the number of CK cycles.
They may not be applicable for all DRAM standards. Please refer to the
corresponding source file (e.g., DDR4.cpp) or the corresponding standard's
datasheet for the definition of a timing parameter.
   
* `RAMULATOR_TCL`
* `RAMULATOR_TCCD`
* `RAMULATOR_TCCDS`
* `RAMULATOR_TCCDL`
* `RAMULATOR_TCWL`
* `RAMULATOR_TBL`
* `RAMULATOR_TWTR`
* `RAMULATOR_TWTRS`
* `RAMULATOR_TWTRL`
* `RAMULATOR_TRP`
* `RAMULATOR_TRPpb`
* `RAMULATOR_TRPab`
* `RAMULATOR_TRCD`
* `RAMULATOR_TRCDR`
* `RAMULATOR_TRCDW`
* `RAMULATOR_TRAS`


### Memory Controller Parameters

* `RAMULATOR_SCHEDULING_POLICY`: The memory request scheduling policy. See
  *ramulator/Scheduler.h* for the supported scheduling policies.
* `RAMULATOR_READQ_ENTRIES`: The number of read requests the memory
  controller can store in its read request queue. The scheduling policy
selects one of the read request queue entries to schedule next.
* `RAMULATOR_WRITEQ_ENTRIES`: The number of write requests the memory
  controller can store in its write request queue. The scheduling policy
selects one of the write request queue entries to schedule next.


