
#include "androcles/type.h"

#include "error.h"

std::ostream& operator<<(std::ostream& os, Kind kind) {
  switch (kind) {
    case kStruct: return os << "struct";
    case kUnion: return os << "union";
  }
  CHECK(false);
}

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

int Type::NumFields() const {
  return decl_->fields.size();
}

Type Type::TypeOfField(const std::string& field_name) const {
  CHECK(decl_ != nullptr) << "TypeOfField called on Type::Null()";
  for (Field field : decl_->fields) {
    if (field.name == field_name) {
      return field.type;
    }
  }
  return Type::Null();
}

Type Type::TypeOfField(int index) const {
  CHECK(decl_ != nullptr) << "TypeOfField called on Type::Null()";
  if (index >= 0 && index < decl_->fields.size()) {
    return decl_->fields[index].type;
  }
  return Type::Null();
}

int Type::IndexOfField(const std::string& field_name) const {
  CHECK(decl_ != nullptr) << "TypeOfField called on Type::Null()";
  for (int i = 0; i < decl_->fields.size(); i++) {
    if (decl_->fields[i].name == field_name) {
      return i;
    }
  }
  return -1;
}

bool Type::operator==(const Type& rhs) const {
  return decl_ == rhs.decl_;
}

bool Type::operator!=(const Type& rhs) const {
  return decl_ != rhs.decl_;
}

std::ostream& operator<<(std::ostream& os, const Type& type) {
  if (type.decl_ == nullptr) {
    return os << "Type::Null()";
  }
  return os << *type.decl_;
}

Type Type::Null() {
  return Type(nullptr);
}

std::ostream& operator<<(std::ostream& os, const Field& field) {
  return os << field.type.GetName() << " " << field.name << ";";
}

std::ostream& operator<<(std::ostream& os, const TypeDecl& decl) {
  os << decl.kind << " " << decl.name << "{";
  for (Field field : decl.fields) {
    os << " " << field;
  }
  return os << " }";
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

