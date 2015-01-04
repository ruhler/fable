
#include "truth_table_component.h"

#include <cassert>

TruthTableComponent::TruthTableComponent(TruthTable truth_table)
  : truth_table_(truth_table)
{ }

std::vector<Value> TruthTableComponent::Eval(const std::vector<Value>& inputs) const
{
  assert(inputs.size() == NumInputs()
      && "Wrong number of inputs passed to truth table component");

  uint32_t input_bits = 0;
  for (int i = 0; i < NumInputs(); i++) {
    input_bits <<= 1;
    if (inputs[i] == BIT_ONE) {
      input_bits |= 1;
    }
  }

  uint32_t output_bits = truth_table_.Eval(input_bits);
  std::vector<Value> outputs;
  for (int i = 0; i < NumOutputs(); i++) {
    outputs.push_back((output_bits & 0x1) ? BIT_ONE : BIT_ZERO);
    output_bits >>= 1;
  }
  return outputs;
}

int TruthTableComponent::NumInputs() const
{
  return truth_table_.kNumInputs;
}

int TruthTableComponent::NumOutputs() const
{
  return truth_table_.kNumOutputs;
}

