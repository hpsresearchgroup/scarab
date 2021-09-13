# Scarab

Scarab is a cycle accurate simulator for state-of-the-art, high performance,
multicore chips. Scarab's goal is to be highly accurate, while also being
fast and easy to work with.

##### Simulator Features:
* Accurate: Scarab is detailed cycle accurate uArchitecture model
* Fast: 600 KIPS trace-driven, 100 KIPS exec-driven
* SimPoint Support: Checkpoints, Fast-Forward, Marker Instructions
* Execute-at-Fetch: Easier support for oracle features, faster development of new features

##### What Code Can Scarab Run?
* Single-threaded x86\_64 programs that can be run on Intel's [PIN](https://software.intel.com/en-us/articles/pin-a-dynamic-binary-instrumentation-tool)

##### Scarab uArchitecture:
* All typical pipeline stages and out-of-order structures (Fetch, Decode, Rename, Retire, ROB, R/S, and more...)
* Multicore 
* Wrong path simulation
* Cache Hierarchy (Private L1, Private MLC, Private/Shared LLC)
* Ramulator Memory Simulator (DDR3/4, LPDDR3/4, GDDR5, HBM, WideIO/2, and more...)  
* Interface to McPat and CACTI for system level power/energy modeling
* Support for DVFS
* Latest Branch Predictors and Data Prefetchers (TAGE-SC-L, Stride, Stream, 2dc, GHB, Markov, and more...)

##### Code Limitations
* 32-bit binaries not supported (work in progress)
* Performance of System Code not modeled
* No cooperative multithreaded code

##### uArch Limitations
* No SMT
* No real OS virtual to physical address translation
* Shared bus interconnect only (ring, mesh, and others are in progress.)

Scarab was created in collaboration with HPS and SAFARI. This project was sponsored by Intel Labs.

## License & Copyright
Please see the [LICENSE](LICENSE) for more information.

## Getting Started

1. [System requirements and software prerequisites.](docs/system_requirements.md)
2. [Compiling Scarab.](docs/compiling-scarab.md)
3. [Setting up and running auto-verification on Scarab.](docs/verification.md)
4. Running a single program on Scarab.
5. Running multiple jobs locally or on a batch system. (coming soon!)
6. Viewing batch job status and results. (coming soon!)
7. [Simulating dynamorio memtraces](docs/memtrace.md)
8. Solutions to common Scarab problems.

## Contributing to Scarab

Found a bug? [File a bug report.](https://github.com/hpsresearchgroup/scarab/issues/new/choose)

Request a new feature? [File a feature request.](https://github.com/hpsresearchgroup/scarab/issues/new/choose)

Have code you would like to commit? [Create a pull request.](https://github.com/hpsresearchgroup/scarab/pulls)

## Other Resources


1) Auto-generated software documentation can be found [here](docs/doxygen/index.html).

* Please run this command in this directory to auto-generate documentation files.
> make -C docs
