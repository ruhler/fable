
#ifndef ANDROCLES_TYPE_H_
#define ANDROCLES_TYPE_H_

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class TypeDecl;

// There are two kinds of types in Androcles.
// Struct types group data values together, and union types carry a single
// data value drawn from a set of possible constructors.
enum Kind {
  kStruct,
  kUnion
};

std::ostream& operator<<(std::ostream& os, Kind kind);

// Objects of class Type represent references to declared types.
// Type objects are cheap to copy. They are only valid in the context of a
// TypeEnv.
class Type {
 public:
  Type(const TypeDecl* decl);

  Kind GetKind() const;
  const std::string& GetName() const;

  // Returns the number of fields the type has.
  int NumFields() const;

  // Returns the type of the field with the given name. 
  // Returns Type::Null() if the type does not contain a field with the given
  // name.
  Type TypeOfField(const std::string& field_name) const;

  // Returns the type of the field with the given index.
  // Returns Type::Null() if the index is out of bounds.
  Type TypeOfField(int index) const;

  // Returns the index of the field with the given name.
  // Returns -1 if the type does not contain a field with the given name.
  int IndexOfField(const std::string& field_name) const;

  bool operator==(const Type& rhs) const;
  bool operator!=(const Type& rhs) const;
  std::ostream& operator<<(std::ostream& os) const;

  // Type object used to indicate when something goes wrong.
  static Type Null();

 private:
  const TypeDecl* decl_;
};


// A Field represents a typed name. It is used for struct and union fields, as
// well as function parameters.
struct Field {
  Type type;
  std::string name;
};

std::ostream& operator<<(std::ostream& os, const Field& field);

struct TypeDecl {
  Kind kind;
  std::string name;
  std::vector<Field> fields;
};

std::ostream& operator<<(std::ostream& os, const TypeDecl& field);

class TypeEnv {
 public:
  // Declare a struct type with the given name and fields.
  // The returned Type object will remain valid for the duration of the
  // TypeEnv object.
  // Returns Type::Null() if there is already a type declared with that name.
  Type DeclareStruct(const std::string& name, const std::vector<Field>& fields);

  // Declare a union type with the given name and fields.
  // The returned Type object will remain valid for the duration of the
  // TypeEnv object.
  // Returns Type::Null() if there is already a type declared with that name.
  Type DeclareUnion(const std::string& name, const std::vector<Field>& fields);

  // Declare a type with given kind, name, and fields.
  // The returned Type object will remain valid for the duration of the
  // TypeEnv object.
  // Returns Type::Null() if there is already a type declared with that name.
  Type DeclareType(Kind kind, const std::string& name, const std::vector<Field>& fields);


  // Return the type declared with the given name.
  // Returns Type::Null() if there is no type declared with the given name.
  Type LookupType(const std::string& name);

 private:
  std::unordered_map<std::string, TypeDecl> decls_;
};

#endif//ANDROCLES_TYPE_H_

