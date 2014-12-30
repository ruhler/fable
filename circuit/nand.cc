
#include "nand.h"

#include <cassert>

std::vector<Value> Nand::run(const std::vector<Value>& inputs)
{
    assert(inputs.size() == 2);
    Bit a = unpack(inputs[0]);
    Bit b = unpack(inputs[1]);

    Bit z = BIT_ONE;
    if (a == BIT_ONE && b == BIT_ONE) {
        z = BIT_ZERO;
    }

    std::vector<Value> outputs;
    outputs.push_back(pack(z));
    return outputs;
}

