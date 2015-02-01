
#include <gtest/gtest.h>

#include "circuit/circuit.h"
#include "circuit/truth_table_component.h"
#include "circuit/value.h"

TEST(CircuitTest, Swap) {
  Circuit swap({"A", "B"}, {"X", "Y"}, {}, {
    {Circuit::kInputPortComponentIndex, 1},
    {Circuit::kInputPortComponentIndex, 0}});

  EXPECT_EQ(2, swap.NumInputs());
  EXPECT_EQ(2, swap.NumOutputs());
  EXPECT_EQ(0, swap.OutputByName("X"));
  EXPECT_EQ(1, swap.OutputByName("Y"));
  EXPECT_EQ(-1, swap.OutputByName("Z"));

  std::vector<Value> expected = {BIT_ZERO, BIT_ONE};
  EXPECT_EQ(expected, swap.Eval({BIT_ONE, BIT_ZERO}));
}

TEST(CircuitTest, ApiExample) {
  // Circuit for: Z = A & (B + C) + (C & D)
  TruthTableComponent and_tt({"A", "B"}, {"Z"}, {0, 0, 0, 1});
  TruthTableComponent or_tt({"A", "B"}, {"Z"}, {0, 1, 1, 1});

  std::vector<Circuit::SubComponentEntry> instances = {
    {&or_tt, {
       {Circuit::kInputPortComponentIndex, 1},
       {Circuit::kInputPortComponentIndex, 2}}},
    {&and_tt, {
       {Circuit::kInputPortComponentIndex, 2},
       {Circuit::kInputPortComponentIndex, 3}}},
    {&and_tt, {{Circuit::kInputPortComponentIndex, 0}, {0, 0}}},
    {&or_tt, {{1, 0}, {2, 0}}}};
  std::vector<Circuit::PortIdentifier> outputs = {{3, 0}};

  Circuit circuit({"A", "B", "C", "D"}, {"Z"}, instances, outputs);

  std::vector<Value> want(1);

  want[0] = BIT_ZERO;
  EXPECT_EQ(want, circuit.Eval({BIT_ZERO, BIT_ONE, BIT_ONE, BIT_ZERO}));
  EXPECT_EQ(want, circuit.Eval({BIT_ONE, BIT_ZERO, BIT_ZERO, BIT_ONE}));

  want[0] = BIT_ONE;
  EXPECT_EQ(want, circuit.Eval({BIT_ONE, BIT_ONE, BIT_ZERO, BIT_ZERO}));
  EXPECT_EQ(want, circuit.Eval({BIT_ZERO, BIT_ZERO, BIT_ONE, BIT_ONE}));
}

