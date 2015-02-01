
#include "circuit/truth_table_component.h"

#include "error.h"

TruthTableComponent::TruthTableComponent(TruthTable truth_table)
  : truth_table_(truth_table)
{}

TruthTableComponent::TruthTableComponent(std::vector<std::string> inputs,
    std::vector<std::string> outputs, std::vector<uint32_t> table)
  : truth_table_(TruthTable(inputs, outputs, table))
{}

std::vector<Value> TruthTableComponent::Eval(const std::vector<Value>& inputs) const
{
  CHECK_EQ(inputs.size(), NumInputs())
    << "Wrong number of inputs passed to truth table component";

  uint32_t input_bits = 0;
  for (int i = 0; i < NumInputs(); i++) {
    input_bits <<= 1;
    if (inputs[i] == BIT_ONE) {
      input_bits |= 1;
    }
  }

  uint32_t output_bits = truth_table_.Eval(input_bits);
  std::vector<Value> outputs;
  uint32_t mask = 1 << NumOutputs();
  for (int i = 0; i < NumOutputs(); i++) {
    mask >>= 1;
    outputs.push_back((output_bits & mask) ? BIT_ONE : BIT_ZERO);
  }
  return outputs;
}

std::vector<std::string> TruthTableComponent::Inputs() const
{
  return truth_table_.Inputs();
}

std::vector<std::string> TruthTableComponent::Outputs() const
{
  return truth_table_.Outputs();
}

