
#include "parser/common_parser.h"

std::vector<std::string> ParseInputs(SpaceEatingTokenStream& tokens)
{
  std::vector<std::string> inputs;
  if (tokens.TokenIs(kWord)) {
    inputs.push_back(tokens.GetWord());
  }

  while (!tokens.TokenIs(kSemicolon)) {
    tokens.EatToken(kComma);
    inputs.push_back(tokens.GetWord());
  }
  tokens.EatToken(kSemicolon);
  return inputs;
}

std::vector<std::string> ParseOutputs(SpaceEatingTokenStream& tokens)
{
  std::vector<std::string> outputs;
  outputs.push_back(tokens.GetWord());
  while (!tokens.TokenIs(kCloseParen)) {
    tokens.EatToken(kComma);
    outputs.push_back(tokens.GetWord());
  }
  tokens.EatToken(kCloseParen);
  return outputs;
}

