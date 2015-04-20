
#ifndef EXPR_H_
#define EXPR_H_

#include "name.h"

typedef enum {
  EXPR_VAR, EXPR_APP, EXPR_ACCESS, EXPR_UNION, EXPR_LET, EXPR_COND
} expr_tag_t;

typedef struct {
  expr_tag_t tag;
} expr_t;

typedef struct {
  expr_tag_t tag;
  vname_t name;
} var_expr_t;

typedef struct {
  expr_tag_t tag;
  dname_t function;
  expr_t* args[];
} app_expr_t;

typedef struct {
  expr_tag_t tag;
  expr_t* arg;
  fname_t field;
} access_expr_t; 

typedef struct {
  expr_tag_t tag;
  dname_t type;
  fname_t field;
  expr_t* value;
} union_expr_t;

typedef struct {
  expr_tag_t tag;
  dname_t type;
  vname_t name;
  expr_t* def;
  expr_t* body;
} let_expr_t;

typedef struct {
  expr_tag_t tag;
  expr_t* select;
  expr_t* choices[];
} cond_expr_t;

#endif//EXPR_H_

