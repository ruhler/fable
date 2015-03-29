
#ifndef BATHYLUS_LIB_ADDITION_H_
#define BATHYLUS_LIB_ADDITION_H_

#include "bathylus/case_expr.h"
#include "bathylus/let_expr.h"
#include "bathylus/function.h"
#include "bathylus/type.h"
#include "bathylus/var_expr.h"

#include "bathylus/lib/stdlib.h"

class FullAdder {
 public:
  FullAdder(const StdLib& stdlib);

  const Type* out_t;
  const Function* function;

 private:
  Type out_t_;
  VarExpr a_;
  VarExpr b_;
  VarExpr cin_;
  VarExpr z_;
  VarExpr cout_;
  CaseExpr not_cin_;
  CaseExpr z_a0_b_;
  CaseExpr z_a1_b_;
  CaseExpr z_a_;
  CaseExpr cout_a0_b_;
  CaseExpr cout_a1_b_;
  CaseExpr cout_a_;
  StructExpr result_;
  LetExpr let_;
  Function function_;
};

#endif//BATHYLUS_LIB_ADDITION_H_
