#ifndef BATHYLUS_FUNCTION_H_
#define BATHYLUS_FUNCTION_H_

#include <string>
#include <vector>

#include "bathylus/type.h"
#include "bathylus/value.h"

class Expr;

class Function {
 public:
  Function(const std::string& name, const std::vector<Field>& args,
      const Type* out_type, const Expr* body);

  Value Eval(const std::vector<Value>& args) const;

 private:
  std::string name_;
  std::vector<Field> args_;
  const Type* out_type_;
  const Expr* body_;
};

#endif//BATHYLUS_FUNCTION_H_

