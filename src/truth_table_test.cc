
#include <vector>
#include <gtest/gtest.h>

#include "truth_table.h"

TEST(TruthTableTest, XOR) {
  std::vector<uint32_t> table;
  table.push_back(0);
  table.push_back(1);
  table.push_back(1);
  table.push_back(0);
  TruthTable truth_table(2, 1, table);

  EXPECT_EQ(0, truth_table.Eval(0));
  EXPECT_EQ(1, truth_table.Eval(1));
  EXPECT_EQ(1, truth_table.Eval(2));
  EXPECT_EQ(0, truth_table.Eval(3));
}

