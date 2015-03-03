
#include "androcles/type.h"

#include "error.h"

Type::Type(const TypeDecl* decl)
  : decl_(decl)
{}

Kind Type::GetKind() const {
  CHECK(decl_ != nullptr) << "GetKind called on Type::Null()";
  return decl_->kind;
}

const std::string& Type::GetName() const {
  CHECK(decl_ != nullptr) << "GetName called on Type::Null()";
  return decl_->name;
}

Type Type::TypeOfField(const std::string& field_name) const {
  CHECK(decl_ != nullptr) << "TypeOfField called on Type::Null()";
  for (Field field : decl_->fields) {
    if (field.name == field.name) {
      return field.type;
    }
  }
  return Type::Null();
}

Type Type::Null() {
  return Type(nullptr);
}

Type TypeEnv::DeclareStruct(const std::string& name, const std::vector<Field>& fields) {
  return DeclareType(kStruct, name, fields);
}

Type TypeEnv::DeclareUnion(const std::string& name, const std::vector<Field>& fields) {
  return DeclareType(kUnion, name, fields);
}

Type TypeEnv::DeclareType(Kind kind, const std::string& name, const std::vector<Field>& fields) {
  auto result = decls_.emplace(name, TypeDecl({kind, name, fields}));

  if (!result.second) {
    return Type::Null();
  }
  return Type(&(result.first->second));
}

Type TypeEnv::LookupType(const std::string& name) {
  auto result = decls_.find(name);
  if (result == decls_.end()) {
    return Type::Null();
  }
  return Type(&(result->second));
}

