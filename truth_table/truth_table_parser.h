
#ifndef TRUTH_TABLE_PARSER_H_
#define TRUTH_TABLE_PARSER_H_

#include <istream>
#include <string>

#include "truth_table/truth_table.h"

// Parse a TruthTable from the given input stream.
// The source is the name of the input stream for error reporting purposes.
// Throws a ParseException if there is a parse error.
TruthTable ParseTruthTable(std::string source, std::istream& istream);

#endif//TRUTH_TABLE_PARSER_H_

