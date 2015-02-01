
#ifndef ERROR_H_
#define ERROR_H_

#include <sstream>

class ProgramError {
 public:
  ProgramError(const char* file, int line);
  ~ProgramError();

  template <typename T>
  ProgramError& operator<<(const T& x) {
    message_ << x;
    return *this;
  }

  ProgramError& Check(bool no_error);

  template <typename A, typename B>
  ProgramError& CheckEq(A a, B b) {
    return Check(a == b) << a << " == " << b << ". ";
  }

  template <typename A, typename B>
  ProgramError& CheckNe(A a, B b) {
    return Check(a != b) << a << " != " << b << ". ";
  }

  template <typename A, typename B>
  ProgramError& CheckGe(A a, B b) {
    return Check(a >= b) << a << " >= " << b << ". ";
  }

  template <typename A, typename B>
  ProgramError& CheckGt(A a, B b) {
    return Check(a > b) << a << " > " << b << ". ";
  }

  template <typename A, typename B>
  ProgramError& CheckLt(A a, B b) {
    return Check(a < b) << a << " < " << b << ". ";
  }

  template <typename A, typename B>
  ProgramError& CheckLe(A a, B b) {
    return Check(a <= b) << a << " <= " << b << ". ";
  }

 private:
  // Dissallow copy and assignment.
  ProgramError(const ProgramError& rhs);
  ProgramError& operator=(const ProgramError& rhs);

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
#define CHECK(x) ProgramError(__FILE__, __LINE__).Check(x)
#define CHECK_EQ(x, y) ProgramError(__FILE__, __LINE__).CheckEq(x, y)
#define CHECK_NE(x, y) ProgramError(__FILE__, __LINE__).CheckNe(x, y)
#define CHECK_GE(x, y) ProgramError(__FILE__, __LINE__).CheckGe(x, y)
#define CHECK_GT(x, y) ProgramError(__FILE__, __LINE__).CheckGt(x, y)
#define CHECK_LE(x, y) ProgramError(__FILE__, __LINE__).CheckLe(x, y)
#define CHECK_LT(x, y) ProgramError(__FILE__, __LINE__).CheckLt(x, y)
#define TODO (ProgramError(__FILE__, __LINE__).Check(false) << "TODO: ")

#endif//ERROR_H_
