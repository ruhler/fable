
#include <sstream>
#include <gtest/gtest.h>

#include "parser/parse_exception.h"
#include "circuit/circuit_parser.h"

TEST(CircuitParserTest, Basic) {
  try {
    std::istringstream iss(
        "Circuit(A, B, C, D; Z) {\n"
        "  Component AND:\n"
        "    TruthTable(A, B; Z) {\n"
        "     00: 0;\n"
        "     01: 0;\n"
        "     10: 0;\n"
        "     11: 1;\n"
        "    };\n"
        "\n"
        "  Component OR:\n"
        "    TruthTable(A, B; Z) {\n"
        "     00: 0;\n"
        "     01: 1;\n"
        "     10: 1;\n"
        "     11: 1;\n"
        "    };\n"
        "\n"
        "  Instance or1: OR(B, C);\n"
        "  Instance and1: AND(C, D);\n"
        "  Instance and2: AND(A, or1.Z);\n"
        "  Instance or2: OR(and2.Z, and1.Z);\n"
        "\n"
        "  Output(or2.Z);\n"
        "}\n");

    Circuit circuit = ParseCircuit("circuit_test", iss);

    // TODO: Should I be asserting structural equivalence, rather than testing
    // for behavior?

    std::vector<Value> want(1);

    want[0] = BIT_ZERO;
    EXPECT_EQ(want, circuit.Eval({BIT_ZERO, BIT_ONE, BIT_ONE, BIT_ZERO}));
    EXPECT_EQ(want, circuit.Eval({BIT_ONE, BIT_ZERO, BIT_ZERO, BIT_ONE}));

    want[0] = BIT_ONE;
    EXPECT_EQ(want, circuit.Eval({BIT_ONE, BIT_ONE, BIT_ZERO, BIT_ZERO}));
    EXPECT_EQ(want, circuit.Eval({BIT_ZERO, BIT_ZERO, BIT_ONE, BIT_ONE}));
  } catch (ParseException& e) {
    FAIL() << e;
  }
}

