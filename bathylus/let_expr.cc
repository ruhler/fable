
#include "bathylus/let_expr.h"

#include <unordered_map>

#include "error.h"

LetExpr::LetExpr(const std::vector<VarDecl>& decls, const Expr* body)
  : decls_(decls), body_(body)
{}

LetExpr::~LetExpr()
{}

Value LetExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  std::unordered_map<std::string, Value> letenv = env;
  for (auto& decl : decls_) {
    auto result = letenv.emplace(decl.name, decl.value->Eval(letenv));
    CHECK(result.second) << "Duplicate assignment to " << decl.name;
  }
  return body_->Eval(letenv);
}

