
#include "truth_table/truth_table.h"

#include <iostream>

#include "error.h"

TruthTable::TruthTable(std::vector<std::string> inputs,
    std::vector<std::string> outputs,
    std::vector<uint32_t> table)
  : kNumInputs(inputs.size()), kNumOutputs(outputs.size()),
    inputs_(inputs), outputs_(outputs), table_(table)
{
  CHECK_LE(kNumInputs, 32) << "Too many input bits for this implementation.";
  CHECK_LE(kNumOutputs, 32) << "Too many output bits for this implementation.";
  CHECK_EQ((1 << kNumInputs), table_.size())
    << "Wrong number of table elements.";
}

uint32_t TruthTable::Eval(uint32_t input) const
{
  CHECK_LT(input, table_.size()) << "Too many input bits given.";
  return table_[input];
}

bool TruthTable::operator==(const TruthTable& rhs) const
{
  return inputs_ == rhs.inputs_
    && outputs_ == rhs.outputs_ 
    && table_ == rhs.table_;
}

bool TruthTable::operator!=(const TruthTable& rhs) const
{
  return !(this->operator==(rhs));
}

const std::vector<std::string>& TruthTable::Inputs() const {
  return inputs_;
}

const std::vector<std::string>& TruthTable::Outputs() const {
  return outputs_;
}

