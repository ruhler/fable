
#include "decl_var_expr.h"

#include "var_decl.h"

DeclVarExpr::DeclVarExpr(const VarDecl* decl)
  : decl_(decl)
{}

DeclVarExpr::~DeclVarExpr()
{}

const Type* DeclVarExpr::GetType() const {
  return decl_->GetType();
}

