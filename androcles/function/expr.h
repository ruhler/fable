
#ifndef ANDROCLES_FUNCTION_EXPR_H_
#define ANDROCLES_FUNCTION_EXPR_H_

#include <string>
#include <unordered_map>

#include "androcles/type.h"
#include "androcles/value.h"

namespace androcles {
namespace function {

class Expr { 
 public:
  virtual ~Expr();
  virtual Type GetType() const = 0;
  virtual Value Eval(const std::unordered_map<std::string, Value>& env) const = 0;
};

}  // namespace function
}  // namespace androcles

#endif//ANDROCLES_FUNCTION_EXPR_H_

