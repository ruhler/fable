
#include "one.h"

#include <cassert>

One::One()
  : Circuit(0, 1) { }

std::vector<Value> One::Eval(const std::vector<Value>& inputs) const
{
  assert(inputs.empty());
  std::vector<Value> outputs;
  outputs.push_back(pack(BIT_ONE));
  return outputs;
}

