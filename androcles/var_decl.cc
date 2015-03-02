
#include "var_decl.h"

VarDecl::VarDecl(const Type* type, const std::string& name, const Expr* expr)
  : type_(type), name_(name), expr_(expr)
{}

VarDecl::~VarDecl()
{}

const Type* VarDecl::GetType() const {
  return type_;
}

