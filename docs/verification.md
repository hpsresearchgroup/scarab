# Verification Tools

Below are a set of resources intended to let you know that Scarab has been installed properly on your system. 

In addition, as you modify Scarab for your own research, you may be able to use these tests to verify that Scarab's base functionality has remained unchaged.

# Sanity Checks

### **Run QSORT to verify Scarab produces stats similar to golden stat files**

./utils/qsort includes a small test program for running libc's qsort() on
random data that we use for sanity checks. To run it, use:

> python ./bin/scarab_test_qsort.py path_to_results_directory
gg
./utils/qsort/ref_stats includes sample stats that you can use for sanity
checks. The exact statistics are unlikely to match with the reference stats
because the produced binary is compiler-dependent.

# Automatic Verification Tools

Coming Soon!