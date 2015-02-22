
#include "androcles/verification.h"

#include "error.h"

bool Verification::Succeeded(std::string* error_msg) const {
  CHECK(error_msg != nullptr);

  if (failed_) {
    *err_msg = message_.str();
    return false;
  }
  return true;
}

std::ostream& Verification::Fail() {
  failed_ = true;
  return message_;
}

