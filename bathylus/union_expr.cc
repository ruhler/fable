
#include "bathylus/union_expr.h"

UnionExpr::UnionExpr(const Type* type, int tag, const Expr* expr)
  : type_(type), tag_(tag), expr_(expr)
{}

UnionExpr::~UnionExpr()
{}

