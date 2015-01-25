
#include "truth_table_parser.h"

#include <cassert>

#include "parse_exception.h"
#include "token_stream.h"

// Read the list of input names from the token stream.
// Format: " a, b, c, ..., d;"
// Note: The list of inputs may be empty.
// After this returns, the token stream will point to the token just after the
// semicolon, assuming there are no parsing errors.
static std::vector<std::string> ParseInputs(SpaceEatingTokenStream& tokens)
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

// Read the list of output names from the token stream.
// Format: "a, b, c, ..., d)"
// Note, there must be at least one output name given.
// After this returns, the token stream will point to the token just after the
// open paren, assuming there are no parsing errors.
static std::vector<std::string> ParseOutputs(SpaceEatingTokenStream& tokens)
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

// Parse num_bits from the given word.
// The word may contain binary digits '0' and '1', and underscore characters
// '_'. All underscores are ignored. Binary digits are read left to right and
// packed in the least significant bits of the return value in the same order,
// left (msb) to right (lsb).
//
// Throws a ParseException if the word doesn't properly represent num_bits of
// bits.
//
// num_bits must be less than or equal to 32.
static uint32_t ParseBitSequence(int num_bits, const std::string& word,
    const Location& location)
{
  assert(num_bits <= 32 && "Too many bits to ParseBitSequence");

  uint32_t bits = 0;
  int bits_found = 0;
  for (int i = 0; i < word.size(); i++) {
    switch (word[i]) {
      case '0':
        bits <<= 1;
        bits_found++;
        break;

      case '1':
        bits = (bits << 1) | 1;
        bits_found++;
        break;

      case '_':
        break;

      default:
        throw ParseException(location)
          << "The character '" << word[i] << "' in \"" << word 
          << "\" is not allowed in a bit sequence description.";
    }
  }

  if (num_bits != bits_found) {
    throw ParseException(location)
      << "Expected " << num_bits << "bits , but \"" << word
      << "\" describes " << bits_found << "bits.";
  }

  return bits;
}

TruthTable ParseTruthTable(std::string source, std::istream& istream)
{
  Location location;
  SpaceEatingTokenStream tokens(source, istream);
  location = tokens.GetLocation();
  std::string word = tokens.GetWord();
  if (word != "TruthTable") {
    throw ParseException(location)
      << "Expected the word 'TruthTable', but found '" << word << "'.";
  }

  tokens.EatToken(kOpenParen);

  location = tokens.GetLocation();
  std::vector<std::string> inputs = ParseInputs(tokens);
  if (inputs.size() > 32) {
    throw ParseException(location)
      << "Found " << inputs.size() << " inputs"
      << ", but this implementation only supports up to 32 inputs.";
  }

  location = tokens.GetLocation();
  std::vector<std::string> outputs = ParseOutputs(tokens);
  if (outputs.size() > 32) {
    throw ParseException(location)
      << "Found " << outputs.size() << " outputs"
      << ", but this implementation only supports up to 32 outputs.";
  }

  tokens.EatToken(kOpenBrace);

  const int kNumEntries = 1 << inputs.size();
  std::vector<uint32_t> table(kNumEntries);
  std::vector<bool> filled(kNumEntries, false);

  for (int i = 0; i < kNumEntries; i++) {
    location = tokens.GetLocation();
    std::string key_word = tokens.GetWord();
    uint32_t key = ParseBitSequence(inputs.size(), key_word, location);

    if (filled[key]) {
      throw ParseException(location)
        << "Duplicate table entry: " << key_word;
    }

    tokens.EatToken(kColon);

    location = tokens.GetLocation();
    std::string value_word = tokens.GetWord();
    uint32_t value = ParseBitSequence(outputs.size(), value_word, location);
    tokens.EatToken(kSemicolon);

    table[key] = value;
    filled[key] = true;
  }

  tokens.EatToken(kCloseBrace);
  return TruthTable(inputs, outputs, table);
}

