
#ifndef ERROR_H_
#define ERROR_H_

#include <sstream>

class ProgramError {
 public:
  ProgramError(bool no_error, const char* file, int line);
  ProgramError(const ProgramError& rhs);
  ~ProgramError();

  template <typename T>
  ProgramError& operator<<(const T& x) {
    message_ << x;
    return *this;
  }

  template <typename A, typename B>
  static ProgramError CheckEq(A a, B b, const char* file, int line) {
    return ProgramError(a == b, file, line) << a << " == " << b << ". ";
  }

  template <typename A, typename B>
  static ProgramError CheckNe(A a, B b, const char* file, int line) {
    return ProgramError(a != b, file, line) << a << " != " << b << ". ";
  }

  template <typename A, typename B>
  static ProgramError CheckGe(A a, B b, const char* file, int line) {
    return ProgramError(a >= b, file, line) << a << " >= " << b << ". ";
  }

  template <typename A, typename B>
  static ProgramError CheckGt(A a, B b, const char* file, int line) {
    return ProgramError(a > b, file, line) << a << " > " << b << ". ";
  }

  template <typename A, typename B>
  static ProgramError CheckLt(A a, B b, const char* file, int line) {
    return ProgramError(a < b, file, line) << a << " < " << b << ". ";
  }

  template <typename A, typename B>
  static ProgramError CheckLe(A a, B b, const char* file, int line) {
    return ProgramError(a <= b, file, line) << a << " <= " << b << ". ";
  }

 private:
  bool is_error_;
  std::ostringstream message_;
};

// CHECK: Assertion.
// If the given argument is false, this reports an error with the file and
// line number and aborts the program.
//
// You can stream additional information to the CHECK that will be included in
// the error message.
//
// Examples:
//  CHECK(x < 32) << "The variable x has value " << x << " which is too big.";
//  CHECK_LT(x, 32) << "The value of variable x is too large.";
#define CHECK(x) ProgramError(x, __FILE__, __LINE__)
#define CHECK_EQ(x, y) ProgramError::CheckEq(x, y, __FILE__, __LINE__)
#define CHECK_NE(x, y) ProgramError::CheckNe(x, y, __FILE__, __LINE__)
#define CHECK_GE(x, y) ProgramError::CheckGe(x, y, __FILE__, __LINE__)
#define CHECK_GT(x, y) ProgramError::CheckGt(x, y, __FILE__, __LINE__)
#define CHECK_LE(x, y) ProgramError::CheckLe(x, y, __FILE__, __LINE__)
#define CHECK_LT(x, y) ProgramError::CheckLt(x, y, __FILE__, __LINE__)

#endif//ERROR_H_
