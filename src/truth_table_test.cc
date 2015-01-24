
#include <vector>
#include <gtest/gtest.h>

#include "truth_table.h"

TEST(TruthTableTest, XOR) {
  std::vector<std::string> inputs;
  inputs.push_back("A");
  inputs.push_back("B");

  std::vector<std::string> outputs;
  outputs.push_back("Z");

  std::vector<uint32_t> table;
  table.push_back(0);
  table.push_back(1);
  table.push_back(1);
  table.push_back(0);

  TruthTable truth_table(inputs, outputs, table);

  EXPECT_EQ(0, truth_table.Eval(0));
  EXPECT_EQ(1, truth_table.Eval(1));
  EXPECT_EQ(1, truth_table.Eval(2));
  EXPECT_EQ(0, truth_table.Eval(3));
}

TEST(TruthTableTest, Eq) {
  std::vector<std::string> inputs_a;
  inputs_a.push_back("A");
  inputs_a.push_back("B");

  std::vector<std::string> outputs_a;
  outputs_a.push_back("Z");

  std::vector<uint32_t> table_a;
  table_a.push_back(0);
  table_a.push_back(1);
  table_a.push_back(1);
  table_a.push_back(0);

  TruthTable truth_table_a(inputs_a, outputs_a, table_a);

  std::vector<std::string> inputs_b;
  inputs_b.push_back("A");
  inputs_b.push_back("B");

  std::vector<std::string> outputs_b;
  outputs_b.push_back("Z");

  std::vector<uint32_t> table_b;
  table_b.push_back(0);
  table_b.push_back(1);
  table_b.push_back(1);
  table_b.push_back(0);

  TruthTable truth_table_b(inputs_b, outputs_b, table_b);

  EXPECT_EQ(truth_table_a, truth_table_b);
}

TEST(TruthTableTest, NEq_name) {
  std::vector<std::string> inputs_a;
  inputs_a.push_back("A");
  inputs_a.push_back("B");

  std::vector<std::string> outputs_a;
  outputs_a.push_back("Z");

  std::vector<uint32_t> table_a;
  table_a.push_back(0);
  table_a.push_back(1);
  table_a.push_back(1);
  table_a.push_back(0);

  TruthTable truth_table_a(inputs_a, outputs_a, table_a);

  std::vector<std::string> inputs_b;
  inputs_b.push_back("A");
  inputs_b.push_back("C");

  std::vector<std::string> outputs_b;
  outputs_b.push_back("Z");

  std::vector<uint32_t> table_b;
  table_b.push_back(0);
  table_b.push_back(1);
  table_b.push_back(1);
  table_b.push_back(0);

  TruthTable truth_table_b(inputs_b, outputs_b, table_b);

  EXPECT_NE(truth_table_a, truth_table_b);
}

TEST(TruthTableTest, NEq_data) {
  std::vector<std::string> inputs_a;
  inputs_a.push_back("A");
  inputs_a.push_back("B");

  std::vector<std::string> outputs_a;
  outputs_a.push_back("Z");

  std::vector<uint32_t> table_a;
  table_a.push_back(0);
  table_a.push_back(1);
  table_a.push_back(1);
  table_a.push_back(0);

  TruthTable truth_table_a(inputs_a, outputs_a, table_a);

  std::vector<std::string> inputs_b;
  inputs_b.push_back("A");
  inputs_b.push_back("B");

  std::vector<std::string> outputs_b;
  outputs_b.push_back("Z");

  std::vector<uint32_t> table_b;
  table_b.push_back(0);
  table_b.push_back(1);
  table_b.push_back(1);
  table_b.push_back(1);

  TruthTable truth_table_b(inputs_b, outputs_b, table_b);

  EXPECT_NE(truth_table_a, truth_table_b);
}

