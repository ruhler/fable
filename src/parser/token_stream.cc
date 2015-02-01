
#include "token_stream.h"

#include <cctype>

#include "parse_exception.h"

TokenStream::TokenStream(CharStream char_stream)
  : char_stream_(char_stream)
{}

TokenStream::TokenStream(std::string source, std::istream& istream)
  : char_stream_(CharStream(source, istream))
{}

void TokenStream::EatToken(TokenType type) {
  TokenType found = NextTokenType();
  if (type != found) {
    throw ParseException(char_stream_.GetLocation())
      << "Expected token of type " << type
      << ", but found " << found << ".";
  }

  if (type == kWord) {
    GetWord();
  } else {
    char_stream_.GetChar();
  }
}

void TokenStream::EatSpace() {
  while (TokenIs(kSpace)) {
    char_stream_.GetChar();
  }
}

std::string TokenStream::GetWord() {
  TokenType found = NextTokenType();
  if (found != kWord) {
    throw ParseException(char_stream_.GetLocation()) 
      << "Expected token of type " << kWord 
      << ", but found " << found << ".";
  }

  std::string word;
  do {
    word += char_stream_.GetChar();
  } while (kWord == NextTokenType());
  return word;
}

bool TokenStream::TokenIs(TokenType type) {
  return type == NextTokenType();
}

Location TokenStream::GetLocation() const {
  return char_stream_.GetLocation();
}

TokenType TokenStream::NextTokenType() {
  int c = char_stream_.PeekChar();
  switch (c) {
    case EOF: return kEndOfStream;
    case ',': return kComma;
    case ';': return kSemicolon;
    case ':': return kColon;
    case '(': return kOpenParen;
    case ')': return kCloseParen;
    case '{': return kOpenBrace;
    case '}': return kCloseBrace;
  }

  if (IsSpaceChar(c)) {
    return kSpace;
  }

  if (IsWordChar(c)) {
    return kWord;
  }

  throw ParseException(char_stream_.GetLocation())
    << "Encountered unsupported character '" << c << "' in input.";
}

bool TokenStream::IsSpaceChar(char c) {
  return isspace(c);
}

bool TokenStream::IsWordChar(char c) {
  return isalnum(c) || c == '_';
}

SpaceEatingTokenStream::SpaceEatingTokenStream(CharStream char_stream)
  : token_stream_(TokenStream(char_stream))
{}

SpaceEatingTokenStream::SpaceEatingTokenStream(std::string source,
   std::istream& istream)
  : token_stream_(TokenStream(source, istream))
{}

void SpaceEatingTokenStream::EatToken(TokenType type)
{
  token_stream_.EatSpace();
  token_stream_.EatToken(type);
}

std::string SpaceEatingTokenStream::GetWord()
{
  token_stream_.EatSpace();
  return token_stream_.GetWord();
}

bool SpaceEatingTokenStream::TokenIs(TokenType type)
{
  token_stream_.EatSpace();
  return token_stream_.TokenIs(type);
}

Location SpaceEatingTokenStream::GetLocation() {
  token_stream_.EatSpace();
  return token_stream_.GetLocation();
}

