
#include "circuit/circuit_parser.h"

#include "error.h"
#include "parser/parse_exception.h"
#include "parser/token_stream.h"

Circuit ParseCircuit(std::string source, std::istream& istream)
{
  Location location;
  SpaceEatingTokenStream tokens(source, istream);

  location = tokens.GetLocation();
  std::string word = tokens.GetWord();
  if (word != "Circuit") {
    throw ParseException(location)
      << "Expected the word 'Circuit', but found '" << word << "'.";
  }

  tokens.EatToken(kOpenParen);
  std::vector<std::string> inputs = ParseInputs(tokens);
  std::vector<std::string> outputs = ParseOutputs(tokens);
  tokens.EatToken(kOpenBrace);

  // Read in the component definitions.
  location = tokens.GetLocation();
  word = tokens.GetWord();
  while (word == "Component") {
    // TODO: Parse the component here.
    location = tokens.GetLocation();
    word = tokens.GetWord();
  }

  // Read in the instance definitions.
  while (word == "Instance") {
    // TODO: Parse the instance here.
    location = tokens.GetLocation();
    word = tokens.GetWord();
  }

  if (word != "Output") {
    // TODO: If no Instance has appeared yet, then we could also be expecting
    // the word 'Instance'. And if no Component or Instance has appeared yet,
    // then we could also be expecting the word 'Component'.
    // Indicate that in the error message.
    throw ParseException(location)
      << "Expected the word 'Output', but found '" << word << "'.";
  }
}

