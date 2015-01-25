
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

