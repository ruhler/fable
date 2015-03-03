
#include "androcles/var_decl.h"

#include "androcles/expr.h"

VarDecl::VarDecl(const Type* type, const std::string& name, std::unique_ptr<const Expr> expr)
  : type_(type), name_(name), expr_(std::move(expr))
{}

VarDecl::~VarDecl()
{}

const Type* VarDecl::GetType() const {
  return type_;
}

