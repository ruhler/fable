
#include "zero.h"

#include <cassert>

std::vector<Value> Zero::run(const std::vector<Value>& inputs)
{
    assert(inputs.empty());
    std::vector<Value> outputs;
    outputs.push_back(pack(BIT_ZERO));
    return outputs;
}

