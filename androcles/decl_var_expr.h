
#ifndef DECL_VAR_EXPR_H_
#define DECL_VAR_EXPR_H_

#include "androcles/expr.h"

class Type;
class VarDecl;

// Declared Variable Expression
class DeclVarExpr : public Expr {
 public:
  DeclVarExpr(const VarDecl* decl);
  virtual ~DeclVarExpr();
  virtual const Type* GetType() const;

 private:
  const VarDecl* decl_;
};

#endif//DECL_VAR_EXPR_H_

