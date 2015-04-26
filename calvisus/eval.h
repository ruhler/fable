
#ifndef EVAL_H_
#define EVAL_H_

#include "env.h"
#include "scope.h"
#include "value.h"
#include "FblcInternal.h"

value_t* eval(const env_t* env, scope_t* scope, const FblcExpr* expr);

#endif//EVAL_H_

