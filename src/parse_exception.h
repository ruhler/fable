
#ifndef PARSE_EXCEPTION_H_
#define PARSE_EXCEPTION_H_

#include <iostream>

#include "location.h"
#include "token_type.h"

class ParseException {
 public:
  ParseException(Location location);
  virtual ~ParseException();

  // Outputs an error message, without location information.
  virtual std::ostream& Message(std::ostream& os) const = 0;

  // Outputs an error message prefixed with location information.
  std::ostream& MessageWithLocation(std::ostream& os) const;

 private:
  Location location_;
};

std::ostream& operator<<(std::ostream& os, const ParseException& exception);

#endif//PARSE_EXCEPTION_H_

