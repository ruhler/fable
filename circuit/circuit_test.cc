
#include <gtest/gtest.h>

#include "circuit/circuit.h"
#include "circuit/value.h"

TEST(CircuitTest, Swap) {
  std::vector<Circuit::PortIdentifier> outputs(2);
  outputs[0].component_index = Circuit::kInputPortComponentIndex;
  outputs[0].port_index = 1;
  outputs[1].component_index = Circuit::kInputPortComponentIndex;
  outputs[1].port_index = 0;

  Circuit swap({"A", "B"}, {"X", "Y"},
      std::vector<Circuit::SubComponentEntry>(), outputs);
  std::vector<Value> inputvals(2);
  inputvals[0] = BIT_ONE;
  inputvals[1] = BIT_ZERO;

  std::vector<Value> outputvals = swap.Eval(inputvals);
  ASSERT_EQ(2, outputvals.size());
  EXPECT_EQ(BIT_ZERO, outputvals[0]);
  EXPECT_EQ(BIT_ONE, outputvals[1]);
}

