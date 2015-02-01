
#include <vector>
#include <gtest/gtest.h>

#include "truth_table/truth_table.h"

TEST(TruthTableTest, XOR) {
  TruthTable truth_table({"A", "B"}, {"Z"}, {0, 1, 1, 0});
  EXPECT_EQ(0, truth_table.Eval(0));
  EXPECT_EQ(1, truth_table.Eval(1));
  EXPECT_EQ(1, truth_table.Eval(2));
  EXPECT_EQ(0, truth_table.Eval(3));
}

TEST(TruthTableTest, Eq) {
  TruthTable truth_table_a({"A", "B"}, {"Z"}, {0, 1, 1, 0});
  TruthTable truth_table_b({"A", "B"}, {"Z"}, {0, 1, 1, 0});
  EXPECT_EQ(truth_table_a, truth_table_b);
}

TEST(TruthTableTest, NEq_name) {
  TruthTable truth_table_a({"A", "B"}, {"Z"}, {0, 1, 1, 0});
  TruthTable truth_table_b({"A", "C"}, {"Z"}, {0, 1, 1, 0});
  EXPECT_NE(truth_table_a, truth_table_b);
}

TEST(TruthTableTest, NEq_data) {
  TruthTable truth_table_a({"A", "B"}, {"Z"}, {0, 1, 1, 0});
  TruthTable truth_table_b({"A", "B"}, {"Z"}, {0, 1, 1, 1});
  EXPECT_NE(truth_table_a, truth_table_b);
}

