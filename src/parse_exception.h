
#ifndef PARSE_EXCEPTION_H_
#define PARSE_EXCEPTION_H_

#include <iostream>
#include <sstream>

#include "location.h"
#include "token_type.h"

class ParseException {
 public:
  explicit ParseException(Location location);
  ParseException(const ParseException& rhs);

  // Return the location in the input source of the parse exception.
  Location GetLocation() const;

  // Return the error message, without location information.
  std::string GetMessage() const;

  template <typename T>
  ParseException& operator<<(const T& x) {
    message_ << x;
    return *this;
  }

 private:
  Location location_;
  std::ostringstream message_;
};

std::ostream& operator<<(std::ostream& os, const ParseException& exception);

#endif//PARSE_EXCEPTION_H_

