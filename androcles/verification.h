
#ifndef VERIFICATION_H_
#define VERIFICATION_H_

#include <iotream>
#include <string>
#include <sstream>

// Class for reporting errors in verification.
class Verification {
 public:
  // Returns true if the verification succeeded.
  // Returns false if the verification failed, in which case error_msg is
  // set to a description of the failure(s). The error_msg argument must not
  // be null.
  bool Succeeded(std::string* error_msg) const;

  // Marks the verification as failed.
  // Returns an output stream to be used for reporting error messages.
  std::ostream& Fail();

 private:
  bool failed_ = false;
  std::ostringstream message_;
};

#endif//VERIFICATION_H_

