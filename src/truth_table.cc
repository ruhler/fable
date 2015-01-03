
#include "truth_table.h"

#include <cassert>

TruthTable::TruthTable(int num_inputs, int num_outputs,
    std::vector<uint32_t> table)
  : num_inputs_(num_inputs), num_outputs_(num_outputs), table_(table)
{
  assert(num_inputs_ <= 32
      && "Too many input bits for this implementation.");
  assert(num_outputs_ <= 32
      && "Too many output bits for this implementation.");
  assert((1 << num_inputs_) == table_.size()
      && "Wrong number of table elements.");
}

uint32_t TruthTable::Eval(uint32_t input) const
{
  assert(input < table_.size() && "Too many input bits given");
  return table_[input];
}


