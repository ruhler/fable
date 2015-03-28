
#ifndef BATHYLUS_CASE_EXPR_H_
#define BATHYLUS_CASE_EXPR_H_

#include <vector>

#include "bathylus/expr.h"

class CaseExpr : public Expr {
 public:
  CaseExpr(const Expr* arg, const std::vector<const Expr*>& alts);
  virtual ~CaseExpr();

 private:
  const Expr* arg_;
  std::vector<const Expr*> alts_;
};

#endif//BATHYLUS_CASE_EXPR_H_

