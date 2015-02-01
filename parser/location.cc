
#include "parser/location.h"

bool operator==(const Location& a, const Location& b) {
  return a.line == b.line && a.column == b.column && a.source == b.source;
}

std::ostream& operator<<(std::ostream& os, const Location& a) {
  return os << a.source << ":" << a.line << ":" << a.column;
}

