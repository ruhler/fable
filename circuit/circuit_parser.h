
#ifndef CIRCUIT_PARSER_H_
#define CIRCUIT_PARSER_H_

#include <istream>
#include <string>

#include "circuit/circuit.h"
#include "parser/token_stream.h"

// Parse a Circuit from the given input stream.
// The source is the name of the input stream for error reporting purposes.
// Throws a ParseException if there is a parse error.
Circuit ParseCircuit(std::string source, std::istream& istream);

// Parse a Circuit from the given token stream, assuming the magic 'Circuit'
// keyword has already been parsed.
Circuit ParseCircuitAfterMagic(SpaceEatingTokenStream& tokens);

#endif//CIRCUIT_PARSER_H_

