
#include "parser/parse_exception.h"

ParseException::ParseException(Location location)
 : location_(location)
{}

ParseException::ParseException(const ParseException& rhs)
 : location_(rhs.location_), message_(rhs.message_.str())
{}

std::string ParseException::GetMessage() const {
  return message_.str();
}

Location ParseException::GetLocation() const {
  return location_;
}

std::ostream& operator<<(std::ostream& os, const ParseException& exception) {
  return os << exception.GetLocation() << ":" << exception.GetMessage();
}

