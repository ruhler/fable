
#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include <vector>

#include "value.h"

class Circuit {
 public:
  Circuit(int num_inputs, int num_outputs);
  virtual std::vector<Value> Eval(const std::vector<Value>& inputs) const = 0;

  const int kNumInputs;
  const int kNumOutputs;
};

#endif//CIRCUIT_H_
