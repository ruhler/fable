
#include <vector>
#include <gtest/gtest.h>

#include "circuit/truth_table_component.h"

TEST(TruthTableComponentTest, XOR) {
  TruthTableComponent truth_table({"A", "B"}, {"Z"}, {0, 1, 1, 0});

  std::vector<Value> inputs(2);
  std::vector<Value> outputs;

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ZERO;
  outputs = truth_table.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ZERO, outputs[0]);

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ONE;
  outputs = truth_table.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, outputs[0]);

  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ZERO;
  outputs = truth_table.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, outputs[0]);
  
  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ONE;
  outputs = truth_table.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ZERO, outputs[0]);
}

// We had a bug where we were outputting the output bits in the reverse order.
// This test case catches that error.
TEST(TruthTableComponentTest, MultiOut) {
  TruthTableComponent truth_table({"A", "B"}, {"Y", "Z"}, {0, 1, 2, 3});

  std::vector<Value> inputs(2);
  std::vector<Value> outputs;

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ZERO;
  outputs = truth_table.Eval(inputs);
  EXPECT_EQ(inputs, outputs);

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ONE;
  outputs = truth_table.Eval(inputs);
  EXPECT_EQ(inputs, outputs);

  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ZERO;
  outputs = truth_table.Eval(inputs);
  EXPECT_EQ(inputs, outputs);
  
  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ONE;
  outputs = truth_table.Eval(inputs);
  EXPECT_EQ(inputs, outputs);
}

