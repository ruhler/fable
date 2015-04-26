
#ifndef EXPR_H_
#define EXPR_H_

#include "FblcInternal.h"

typedef enum {
  EXPR_VAR, EXPR_APP, EXPR_ACCESS, EXPR_UNION, EXPR_LET, EXPR_COND
} expr_tag_t;

typedef struct {
  expr_tag_t tag;
} expr_t;

typedef struct {
  expr_tag_t tag;
  FblcName name;
} var_expr_t;

typedef struct {
  expr_tag_t tag;
  FblcName function;
  expr_t* args[0];
} app_expr_t;

typedef struct {
  expr_tag_t tag;
  expr_t* arg;
  FblcName field;
} access_expr_t; 

typedef struct {
  expr_tag_t tag;
  FblcName type;
  FblcName field;
  expr_t* value;
} union_expr_t;

typedef struct {
  expr_tag_t tag;
  FblcName type;
  FblcName name;
  expr_t* def;
  expr_t* body;
} let_expr_t;

typedef struct {
  expr_tag_t tag;
  expr_t* select;
  expr_t* choices[0];
} cond_expr_t;

#endif//EXPR_H_

