
#ifndef BATHYLUS_EXPR_H_
#define BATHYLUS_EXPR_H_

#include <string>
#include <unordered_map>

#include "bathylus/value.h"

class Expr {
 public:
  virtual ~Expr();
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const = 0;

};

#endif//BATHYLUS_EXPR_H_

