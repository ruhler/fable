
#include "parser/token_type.h"

#include <stdexcept>

std::ostream& operator<<(std::ostream& os, TokenType type) {
  switch (type) {
    case kComma: return os << ",";
    case kPeriod: return os << ".";
    case kSemicolon: return os << ";";
    case kColon: return os << ":";
    case kOpenParen: return os << "(";
    case kCloseParen: return os << ")";
    case kOpenBrace: return os << "{";
    case kCloseBrace: return os << "}";
    case kEndOfStream: return os << "EOF";
    case kWord: return os << "WORD";
    case kSpace: return os << "SPACE";
    default: throw std::logic_error("Invalid TokenType Encountered");
  }
}

