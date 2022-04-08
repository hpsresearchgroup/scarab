#include "gtest/gtest.h"

extern "C" {
    #include "globals/op_pool.h"
}

void scarab_init_globals() {
    init_op_pool();
}

int main(int argc, char** argv) {
  scarab_init_globals();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}