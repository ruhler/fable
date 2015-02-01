
#ifndef TRUTH_TABLE_PARSER_H_
#define TRUTH_TABLE_PARSER_H_

#include <istream>
#include <string>

#include "parser/token_stream.h"
#include "truth_table/truth_table.h"

// Parse a TruthTable from the given input stream.
// The source is the name of the input stream for error reporting purposes.
// Throws a ParseException if there is a parse error.
TruthTable ParseTruthTable(std::string source, std::istream& istream);

// Parse a TruthTable from the given input stream, assuming the magic
// 'TruthTable' keyword has already been read from the stream.
// Throws a ParseException if there is a parse error.
TruthTable ParseTruthTableAfterMagic(SpaceEatingTokenStream& tokens);

#endif//TRUTH_TABLE_PARSER_H_

