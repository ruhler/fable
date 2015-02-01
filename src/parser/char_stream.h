
#ifndef CHAR_STREAM_H_
#define CHAR_STREAM_H_

#include <istream>
#include <string>

#include "location.h"

class CharStream {
 public:
  // Creates a CharStream on top of the given underlying istream.
  // The source is the name of the input stream for error reporting purposes.
  // For example, this would be the name of the file being read.
  CharStream(std::string source, std::istream& istream);

  // Reads and returns the next character in the stream.
  // Returns EOF if there are no more characters in the stream.
  int GetChar();

  // Returns the next character in the stream without advancing the stream
  // position.
  // Returns EOF if there are no more characters in the stream.
  int PeekChar();

  // Returns the current location in the stream.
  Location GetLocation() const;

 private:
  std::istream& istream_;
  Location location_;
};

#endif//CHAR_STREAM_H_
