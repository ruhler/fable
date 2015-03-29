
#ifndef BATHYLUS_ACCESS_EXPR_H_
#define BATHYLUS_ACCESS_EXPR_H_

#include "bathylus/expr.h"

class AccessExpr : public Expr {
 public:
  AccessExpr(const Expr* arg, int tag);
  ~AccessExpr();
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const;

 private:
  const Expr* arg_;
  int tag_;
};

#endif//BATHYLUS_ACCESS_EXPR_H_
