
#include "error.h"

#include <cstdlib>
#include <iostream>

ProgramError::ProgramError(bool no_error, const char* file, int line)
  : is_error_(!no_error)
{
  if (is_error_) {
    message_ << file << ":" << line << ": error: ";
  }
}

ProgramError::ProgramError(const ProgramError& rhs)
  : is_error_(rhs.is_error_), message_(rhs.message_.str())
{}

ProgramError::~ProgramError()
{
  if (is_error_) {
    std::cerr << message_.str() << std::endl;
    abort();
  }
}

