
#include "char_stream.h"

CharStream::CharStream(std::string source, std::istream& istream)
  : istream_(istream) {
  location_.line = 1;
  location_.column = 1;
  location_.source = source;
}

int CharStream::GetChar() {
  char c;
  if (istream_.get(c)) {
    location_.column++;
    if (c == '\n') {
      location_.line++;
      location_.column = 1;
    }
    return c;
  }
  return EOF;
}

int CharStream::PeekChar() {
  return istream_.peek();
}

Location CharStream::GetLocation() const {
  return location_;
}

