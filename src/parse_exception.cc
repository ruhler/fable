
#include "parse_exception.h"

ParseException::ParseException(Location location)
 : location_(location)
{}

ParseException::~ParseException()
{}

std::ostream& ParseException::MessageWithLocation(std::ostream& os) const {
  return Message(os << location_ << ":");
}

std::ostream& operator<<(std::ostream& os, const ParseException& exception) {
  return exception.MessageWithLocation(os);
}

UnexpectedTokenException::UnexpectedTokenException(
    TokenType expected, TokenType found, Location location)
 : ParseException(location), expected_(expected), found_(found)
{}

std::ostream& UnexpectedTokenException::Message(std::ostream& os) const {
  return os << "Expected token of type " << expected_
    << ", but found " << found_ << ".";
}

UnknownCharException::UnknownCharException(char c, Location location)
 : ParseException(location), char_(c)
{}

std::ostream& UnknownCharException::Message(std::ostream& os) const {
  return os << "Encountered unknown character '" << char_ << "' in input.";
}
