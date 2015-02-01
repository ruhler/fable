
#ifndef CIRCUIT_PARSER_H_
#define CIRCUIT_PARSER_H_

#include <istream>
#include <string>

#include "circuit/circuit.h"

// Parse a Circuit from the given input stream.
// The source is the name of the input stream for error reporting purposes.
// Throws a ParseException if there is a parse error.
Circuit ParseCircuit(std::string source, std::istream& istream);

#endif//CIRCUIT_PARSER_H_

