
#ifndef FIELD_H_
#define FIELD_H_

#include <vector>

class Verification;
class Type;

// A field represents a typed name.
// Uses include:
//  * struct and union fields
//  * function parameters
struct Field {
  const Type& type;
  std::string name;
};

// Verify all the field names for the given fields are distinct.
void VerifyFieldNamesAreDistinct(const std::vector<Field> fields, Verification& verification);

#endif//FIELD_H_

