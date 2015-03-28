
#include "bathylus/let_expr.h"

LetExpr::LetExpr(const std::vector<VarDecl>& decls, const Expr* body)
  : decls_(decls), body_(body)
{}

LetExpr::~LetExpr()
{}

