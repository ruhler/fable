
#ifndef BATHYLUS_LET_EXPR_H_
#define BATHYLUS_LET_EXPR_H_

#include <string>
#include <vector>

#include "bathylus/expr.h"
#include "bathylus/value.h"

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
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  std::vector<VarDecl> decls_;
  const Expr* body_;
};

#endif//BATHYLUS_LET_EXPR_H_

