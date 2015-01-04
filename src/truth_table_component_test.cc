
#include <vector>
#include <gtest/gtest.h>

#include "truth_table_component.h"

TEST(TruthTableComponentTest, XOR) {
  std::vector<uint32_t> table;
  table.push_back(0);
  table.push_back(1);
  table.push_back(1);
  table.push_back(0);
  TruthTable truth_table(2, 1, table);

  TruthTableComponent component(truth_table);

  std::vector<Value> inputs(2);
  std::vector<Value> outputs;

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ZERO;
  outputs = component.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ZERO, outputs[0]);

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ONE;
  outputs = component.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, outputs[0]);

  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ZERO;
  outputs = component.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, outputs[0]);
  
  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ONE;
  outputs = component.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ZERO, outputs[0]);
}

// We had a bug where we were outputting the output bits in the reverse order.
// This test case catches that error.
TEST(TruthTableComponentTest, MultiOut) {
  std::vector<uint32_t> table;
  table.push_back(0);
  table.push_back(1);
  table.push_back(2);
  table.push_back(3);
  TruthTable truth_table(2, 2, table);

  TruthTableComponent component(truth_table);

  std::vector<Value> inputs(2);
  std::vector<Value> outputs;

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ZERO;
  outputs = component.Eval(inputs);
  EXPECT_EQ(inputs, outputs);

  inputs[0] = BIT_ZERO;
  inputs[1] = BIT_ONE;
  outputs = component.Eval(inputs);
  EXPECT_EQ(inputs, outputs);

  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ZERO;
  outputs = component.Eval(inputs);
  EXPECT_EQ(inputs, outputs);
  
  inputs[0] = BIT_ONE;
  inputs[1] = BIT_ONE;
  outputs = component.Eval(inputs);
  EXPECT_EQ(inputs, outputs);
}

