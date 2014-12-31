
#include "zero.h"

#include <cassert>

Zero::Zero()
  : Circuit(0, 1) {}

std::vector<Value> Zero::Eval(const std::vector<Value>& inputs) const
{
  assert(inputs.empty());
  std::vector<Value> outputs;
  outputs.push_back(pack(BIT_ZERO));
  return outputs;
}

