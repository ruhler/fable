#ifndef COMMON_PARSER_H_
#define COMMON_PARSER_H_

#include <string>
#include <vector>

#include "token_stream.h"

// Common parsing utilities used by multiple parsers.

// Read the list of input names from the token stream.
// Format: " a, b, c, ..., d;"
// Note: The list of inputs may be empty.
// After this returns, the token stream will point to the token just after the
// semicolon, assuming there are no parsing errors.
// Throws a ParseException on parse error.
std::vector<std::string> ParseInputs(SpaceEatingTokenStream& tokens);

// Read the list of output names from the token stream.
// Format: "a, b, c, ..., d)"
// Note, there must be at least one output name given.
// After this returns, the token stream will point to the token just after the
// open paren, assuming there are no parsing errors.
// Throws a ParseException on parse error.
std::vector<std::string> ParseOutputs(SpaceEatingTokenStream& tokens);

#endif//COMMON_PARSER_H_
