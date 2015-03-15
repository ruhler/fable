
#include "androcles/function/select_expr.h"

#include "error.h"

namespace androcles {
namespace function {

SelectExpr::SelectExpr(const Expr* select, const std::vector<Alt>& alts)
  : select_(select), alts_(alts)
{
  Type select_type = select->GetType();
  CHECK_EQ(kUnion, select_type.GetKind());
  CHECK_EQ(select_type.NumFields(), alts.size());
  CHECK_GT(select_type.NumFields(), 0);

  Type result_type = alts[0].value->GetType();
  for (auto& alt : alts) {
    CHECK(select_type.HasField(alt.tag));
    CHECK_EQ(result_type, alt.value->GetType());
  }
}

SelectExpr::~SelectExpr()
{}

Type SelectExpr::GetType() const {
  CHECK(!alts_.empty());
  CHECK(alts_[0].value != nullptr);
  return alts_[0].value->GetType();
}

Value SelectExpr::Eval(const std::unordered_map<std::string, Value>& env) const {
  Value select = select_->Eval(env);
  int index = select.GetType().IndexOfField(select.GetTag());
  CHECK_GE(index, 0);
  CHECK_LT(index, alts_.size());
  CHECK(alts_[index].value != nullptr);
  return alts_[index].value->Eval(env);
}

}  // namespace function
}  // namespace androcles

