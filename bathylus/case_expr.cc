
#include "bathylus/case_expr.h"

CaseExpr::CaseExpr(const Expr* arg, const std::vector<const Expr*>& alts)
  : arg_(arg), alts_(alts)
{}

CaseExpr::~CaseExpr()
{}

