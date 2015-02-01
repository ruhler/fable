
#ifndef TOKEN_TYPE_H_
#define TOKEN_TYPE_H_

#include <iostream>

enum TokenType {
  kComma,
  kSemicolon,
  kColon,
  kOpenParen,
  kCloseParen,
  kOpenBrace,
  kCloseBrace,
  kEndOfStream,
  kWord,
  kSpace
};

std::ostream& operator<<(std::ostream& os, TokenType type);

#endif//TOKEN_TYPE_

