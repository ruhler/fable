
#include <gtest/gtest.h>

#include "circuit.h"
#include "value.h"
#include "zero.h"

TEST(CircuitTest, Dummy) {
}

TEST(CircuitTest, Zero) {
    Zero zero;
    std::vector<Value> inputs;
    std::vector<Value> outputs = zero.run(inputs);
    ASSERT_EQ(1, outputs.size());
    EXPECT_EQ(BIT_ZERO, unpack(outputs[0]));
}

