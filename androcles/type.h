
#ifndef TYPE_H_
#define TYPE_H_

#include <string>
#include <iostream>
#include <vector>

class Verification;

class Type {
 public:
  virtual ~Type();

  // Verifies the type is well formed.
  // The results of verification are reported to the verification argument.
  //
  // This only checks the validity of this immediate type. It does not check
  // the validity of types referenced by this type.
  virtual void Verify(Verification& verification) const = 0;

  bool operator==(const Type& rhs) const;
  virtual std::ostream& operator<<(std::ostream& os) const = 0;
};

#endif//TYPE_H_

