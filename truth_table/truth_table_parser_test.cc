
#include <sstream>
#include <gtest/gtest.h>

#include "parser/parse_exception.h"
#include "truth_table/truth_table.h"
#include "truth_table/truth_table_parser.h"

TEST(TruthTableParserTest, Basic) {
  try {
    std::istringstream iss(
        "TruthTable(A, B; Z) {\n"
        "  00: 0;\n"
        "  01: 0;\n"
        "  10: 0;\n"
        "  11: 1;\n"
        "}\n");
    TruthTable expected({"A", "B"}, {"Z"}, {0, 0, 0, 1});
    TruthTable parsed = ParseTruthTable("test", iss);
    EXPECT_EQ(expected, parsed);
  } catch (ParseException& e) {
    FAIL() << e;
  }
}

