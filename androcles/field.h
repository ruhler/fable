
#ifndef FIELD_H_
#define FIELD_H_

#include <string>

class Type;

// A field represents a typed name.
// Uses include:
//  * struct and union fields
//  * function parameters
struct Field {
  const Type* type;
  std::string name;
};

#endif//FIELD_H_

