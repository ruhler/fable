
#ifndef TOKEN_STREAM_H_
#define TOKEN_STREAM_H_

#include <iostream>
#include <string>

#include "parser/char_stream.h"
#include "parser/token_type.h"

class TokenStream {
 public:
  TokenStream(CharStream char_stream);
  TokenStream(std::string source, std::istream& istream);

  // Asserts the current token is of the given type and advances to the next
  // token in the stream.
  // Throws a ParseException if the current token is not of the given type.
  void EatToken(TokenType type);

  // Advance past zero or more whitespace tokens until a non-whitespace token
  // is encountered.
  void EatSpace();

  // Asserts the current token is a Word token, returns the value of the word
  // token, and advances to the next token in the stream.
  // Throws a ParseException if the current token is not of the given type.
  std::string GetWord();

  // Returns true if the next token is of the given type.
  // Throws a ParseException if the next token is not of a known type.
  bool TokenIs(TokenType type);

  Location GetLocation() const;

  static bool IsSpaceChar(char c);
  static bool IsWordChar(char c);

 private:
  // Returns the type of the next token in the stream.
  // Throws a ParseException if the next token is not of a known type.
  TokenType NextTokenType();

  CharStream char_stream_;
};

// A TokenStream which automatically removes all space tokens before reading
// or testing the next token.
class SpaceEatingTokenStream {
 public:
  SpaceEatingTokenStream(CharStream char_stream);
  SpaceEatingTokenStream(std::string source, std::istream& istream);

  void EatToken(TokenType type);
  std::string GetWord();
  bool TokenIs(TokenType type);
  Location GetLocation();

 private:
  TokenStream token_stream_;
};

#endif//TOKEN_STREAM_H_

