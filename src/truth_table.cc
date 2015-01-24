
#include "truth_table.h"

#include <cassert>
#include <iostream>

TruthTable::TruthTable(std::vector<std::string> inputs,
    std::vector<std::string> outputs,
    std::vector<uint32_t> table)
  : kNumInputs(inputs.size()), kNumOutputs(outputs.size()),
    inputs_(inputs), outputs_(outputs), table_(table)
{
  assert(kNumInputs <= 32
      && "Too many input bits for this implementation.");
  assert(kNumOutputs <= 32
      && "Too many output bits for this implementation.");
  assert((1 << kNumInputs) == table_.size()
      && "Wrong number of table elements.");
}

uint32_t TruthTable::Eval(uint32_t input) const
{
  assert(input < table_.size() && "Too many input bits given");
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

