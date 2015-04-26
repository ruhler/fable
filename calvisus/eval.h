
#ifndef EVAL_H_
#define EVAL_H_

#include "scope.h"
#include "value.h"
#include "FblcInternal.h"

value_t* eval(const FblcEnv* env, scope_t* scope, const FblcExpr* expr);

#endif//EVAL_H_

