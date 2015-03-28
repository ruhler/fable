
#include "bathylus/var_expr.h"

VarExpr::VarExpr(const Type* type, const std::string& name)
  : type_(type), name_(name)
{}

VarExpr::~VarExpr()
{}

