
#ifndef TRUTH_TABLE_H_
#define TRUTH_TABLE_H_

#include <cstdint>
#include <vector>

class TruthTable {
 public:
  // Constructs a truth table with num_inputs number of input bits,
  // num_outputs number of output bits, and data supplied in the given table.
  // The table should contain 2^num_inputs elements. Each element gives the
  // bit pattern for the output in the least significant bits corresponding to
  // the bit pattern for the input.
  //
  // For example, consider the truth table for XOR:
  //   A B | XOR(A,B)
  //   --------------
  //   0 0 | 0
  //   0 1 | 1
  //   1 0 | 1
  //   1 1 | 0
  // 
  // This is constructed with:
  //  num_inputs = 2
  //  num_outputs = 1
  //  table[0] = 0;
  //  table[1] = 1;
  //  table[2] = 1;
  //  table[3] = 0;
  //
  // The maximum num_inputs supported by this implementation is 32 bits.
  // The maximum num_outputs supported by this implementation is 32 bits.
  TruthTable(int num_inputs, int num_outputs, std::vector<uint32_t> table);
  
  // Return the value in the truth table for the given input bits.
  uint32_t Eval(uint32_t input) const;

 private:
  int num_inputs_;
  int num_outputs_;
  std::vector<uint32_t> table_;
};

#endif//TRUTH_TABLE_H_

