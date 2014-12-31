
#include "zero.h"

#include <cassert>

std::vector<Value> Zero::Eval(const std::vector<Value>& inputs) const
{
    assert(inputs.empty());
    std::vector<Value> outputs;
    outputs.push_back(pack(BIT_ZERO));
    return outputs;
}

