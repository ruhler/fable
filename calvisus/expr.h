
#ifndef EXPR_H_
#define EXPR_H_

#include "FblcInternal.h"

typedef enum {
  EXPR_VAR,
  EXPR_APP,
  EXPR_ACCESS,
  EXPR_UNION,
  EXPR_LET,
  EXPR_COND
} expr_tag_t;

typedef struct expr_t {
  expr_tag_t tag;
  union {
    struct { FblcName name; } var;
    struct { FblcName function; } app;
    struct { const struct expr_t* arg; FblcName field; } access; 
    struct { FblcName type; FblcName field; const struct expr_t* value; } union_;
    struct {
      FblcName type;
      FblcName name;
      const struct expr_t* def;
      const struct expr_t* body;
    } let;
    struct { const struct expr_t* select; } cond;
  } ex;
  struct expr_t* args[];    // Args for app and union_.
} expr_t;

#endif//EXPR_H_

