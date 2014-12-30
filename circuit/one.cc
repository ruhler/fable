
#include "one.h"

#include <cassert>

std::vector<Value> One::run(const std::vector<Value>& inputs)
{
    assert(inputs.empty());
    std::vector<Value> outputs;
    outputs.push_back(pack(BIT_ONE));
    return outputs;
}

