
#include <vector>
#include <gtest/gtest.h>

#include "adder.h"
#include "circuit.h"

TEST(AdderTest, FourBit) {
  std::unique_ptr<Component> adder = CreateAdder(4);

  // 3 + 5 + carry 0 is input as: 0011_0101_0
  std::vector<Value> input(9);
  input[0] = BIT_ZERO;
  input[1] = BIT_ZERO;
  input[2] = BIT_ONE;
  input[3] = BIT_ONE;

  input[4] = BIT_ZERO;
  input[5] = BIT_ONE;
  input[6] = BIT_ZERO;
  input[7] = BIT_ONE;

  input[8] = BIT_ZERO;

  // The result should be 8 + carry 0: 1000_0
  std::vector<Value> desired(5);
  desired[0] = BIT_ONE;
  desired[1] = BIT_ZERO;
  desired[2] = BIT_ZERO;
  desired[3] = BIT_ZERO;
  
  desired[4] = BIT_ZERO;

  std::vector<Value> actual = adder->Eval(input);

  EXPECT_EQ(desired, actual);
}

