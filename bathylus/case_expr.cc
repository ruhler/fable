
#include "bathylus/case_expr.h"

#include "error.h"

CaseExpr::CaseExpr(const Expr* arg, const std::vector<const Expr*>& alts)
  : arg_(arg), alts_(alts)
{}

CaseExpr::~CaseExpr()
{}

Value CaseExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value arg = arg_->Eval(env);
  int tag = arg.GetTag();
  CHECK_LE(0, tag);
  CHECK_LE(tag, alts_.size());
  return alts_[tag]->Eval(env);
}

