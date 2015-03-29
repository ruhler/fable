
#include "bathylus/function.h"

#include <string>
#include <vector>
#include <unordered_map>

#include "bathylus/expr.h"
#include "bathylus/value.h"
#include "error.h"

Function::Function(const std::string& name, const std::vector<Field>& args,
      const Type* out_type, const Expr* body)
  : name_(name), args_(args), out_type_(out_type), body_(body)
{}

Value Function::Eval(const std::vector<Value>& args) const {
  CHECK_EQ(args_.size(), args.size());

  std::unordered_map<std::string, Value> env;
  for (int i = 0; i < args_.size(); i++) {
    auto result = env.emplace(args_[i].name, args[i]);
    CHECK(result.second) << "Duplicate arg named " << args_[i].name;
  }
  return body_->Eval(env);
}

