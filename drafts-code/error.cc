
#include "error.h"

#include <cstdlib>
#include <iostream>

ProgramError::ProgramError(const char* file, int line)
  : is_error_(false)
{
  message_ << file << ":" << line << ": error: ";
}

ProgramError::~ProgramError()
{
  if (is_error_) {
    std::cerr << message_.str() << std::endl;
    abort();
  }
}

ProgramError& ProgramError::Check(bool no_error) {
  is_error_ = is_error_ || (!no_error);
  return *this;
}

