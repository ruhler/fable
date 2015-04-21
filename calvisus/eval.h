
#ifndef EVAL_H_
#define EVAL_H_

#include "env.h"
#include "expr.h"
#include "scope.h"
#include "value.h"

value_t* eval(const env_t* env, scope_t* scope, const expr_t* expr);

#endif//EVAL_H_

