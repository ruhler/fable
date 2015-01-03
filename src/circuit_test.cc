
#include <gtest/gtest.h>

#include "circuit.h"
#include "one.h"
#include "value.h"
#include "zero.h"
#include "nand.h"

TEST(CircuitTest, Dummy) {
}

TEST(CircuitTest, Zero) {
  Zero zero;
  EXPECT_EQ(0, zero.kNumInputs);
  EXPECT_EQ(1, zero.kNumOutputs);

  std::vector<Value> inputs;
  std::vector<Value> outputs = zero.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ZERO, unpack(outputs[0]));
}

TEST(CircuitTest, One) {
  One one;
  EXPECT_EQ(0, one.kNumInputs);
  EXPECT_EQ(1, one.kNumOutputs);

  std::vector<Value> inputs;
  std::vector<Value> outputs = one.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, unpack(outputs[0]));
}

TEST(CircuitTest, Nand) {
  Nand nand;
  EXPECT_EQ(2, nand.kNumInputs);
  EXPECT_EQ(1, nand.kNumOutputs);

  std::vector<Value> inputs;
  std::vector<Value> outputs;

  inputs.clear();
  inputs.push_back(pack(BIT_ZERO));
  inputs.push_back(pack(BIT_ZERO));
  outputs = nand.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, unpack(outputs[0]));

  inputs.clear();
  inputs.push_back(pack(BIT_ZERO));
  inputs.push_back(pack(BIT_ONE));
  outputs = nand.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, unpack(outputs[0]));

  inputs.clear();
  inputs.push_back(pack(BIT_ONE));
  inputs.push_back(pack(BIT_ZERO));
  outputs = nand.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ONE, unpack(outputs[0]));

  inputs.clear();
  inputs.push_back(pack(BIT_ONE));
  inputs.push_back(pack(BIT_ONE));
  outputs = nand.Eval(inputs);
  ASSERT_EQ(1, outputs.size());
  EXPECT_EQ(BIT_ZERO, unpack(outputs[0]));
}

