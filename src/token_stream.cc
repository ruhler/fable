
#include "token_stream.h"

#include <cctype>

#include "parse_exception.h"

TokenStream::TokenStream(CharStream char_stream)
  : char_stream_(char_stream)
{ }

void TokenStream::EatToken(TokenType type) {
  TokenType found = NextTokenType();
  if (type != found) {
    throw UnexpectedTokenException(type, found, char_stream_.GetLocation());
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
    throw UnexpectedTokenException(kWord, found, char_stream_.GetLocation());
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

  throw UnknownCharException(c, char_stream_.GetLocation());
}

bool TokenStream::IsSpaceChar(char c) {
  return isspace(c);
}

bool TokenStream::IsWordChar(char c) {
  return isalnum(c) || c == '_';
}

