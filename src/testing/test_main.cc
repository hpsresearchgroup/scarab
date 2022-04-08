#include "gtest/gtest.h"

extern "C" {
    #include "globals/op_pool.h"

    #include "sim.h"
    #include "model.h"
}

static void scarab_test_init_globals() {
    SIM_MODEL = DUMB_MODEL;

    init_op_pool();
    init_global(NULL, NULL);
    //init_model(SIMULATION_MODE);

    mystdout = stdout;
    mystderr = stderr;
    mystatus = stdout;
    cycle_count  = 0;
    unique_count = 0;
}

int main(int argc, char** argv) {
  scarab_test_init_globals();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}