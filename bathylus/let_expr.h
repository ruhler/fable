
#ifndef BATHYLUS_LET_EXPR_H_
#define BATHYLUS_LET_EXPR_H_

#include <string>
#include <vector>

#include "bathylus/expr.h"

class Type;

struct VarDecl {
  const Type* type;
  std::string name;
  const Expr* value;
};

class LetExpr : public Expr {
 public:
  LetExpr(const std::vector<VarDecl>& decls, const Expr* body);
  virtual ~LetExpr();

 private:
  std::vector<VarDecl> decls_;
  const Expr* body_;
};

#endif//BATHYLUS_LET_EXPR_H_

