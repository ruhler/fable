
#include <assert.h>   // for assert
#include <ctype.h>    // for isspace
#include <stdbool.h>  // for false
#include <stdlib.h>   // for malloc
#include <stdio.h>    // for printf
#include <string.h>   // for strcmp

#include "fbld.h"
#include "vector.h"

/**
 * @struct[Env] Environment for execution.
 *  @field[size_t][refcount] Reference count.
 *  @field[FbldText*][name] The name of the first defined function.
 *  @field[FbldTextV][args] Names of arguments to the first defined function.
 *  @field[FbldMarkup*][body] Body of the first defined function.
 *  @field[Env*][next] Rest of the defined functions or NULL.
 */
typedef struct Env {
  size_t refcount;
  FbldText* name;
  FbldTextV args;
  FbldMarkup* body;
  struct Env* next;
} Env;

/**
 * Types of evaluation commands.
 */
typedef enum {
  EVAL_CMD,
  DEFINE_CMD
} CmdTag;

/**
 * @struct[Cmd] Base type for an evaluation command to execute.
 *  @field[CmdTag][tag] The type of command.
 *  @field[FbldMarkup**][dest] Where to store the result of evaluation.
 *  @field[Cmd*][next] The next command to execute.
 */
typedef struct Cmd {
  CmdTag tag;
  FbldMarkup** dest;
  struct Cmd* next;
} Cmd;

// Evaluates 'markup' in the environment 'env', storing the result in 'dest.'.
// The EvalCmd owns env and markup.
typedef struct {
  Cmd _base;
  Env* env;
  FbldMarkup* markup;
} EvalCmd;

typedef struct {
  Cmd _base;
  FbldMarkup* name;
  FbldMarkup* args;
  FbldMarkup* def;
  FbldMarkup* body;
  Env* env;
} DefineCmd;


static Env* CopyEnv(Env* env);
static void FreeEnv(Env* env);

static Cmd* NewEval(Cmd* next, Env* env, FbldMarkup* markup, FbldMarkup** dest);

static int HeadOf(FbldMarkup* m);
static FbldMarkup* TailOf(FbldMarkup* m);
static bool Eq(FbldMarkup* a, FbldMarkup* b);
static FbldMarkup* MapPlain(const char* f, FbldMarkup* m);
static void Eval(Cmd* cmd, bool debug);

static Env* CopyEnv(Env* env)
{
  if (env != NULL) {
    env->refcount++;
  }
  return env;
}

static void FreeEnv(Env* env)
{
  while (env != NULL) {
    if (--env->refcount > 0) {
      return;
    }

    free(env->name);
    for (size_t i = 0; i < env->args.size; ++i) {
      free(env->args.xs[i]);
    }
    free(env->args.xs);
    Env* next = env->next;
    free(env);
    env = next;
  }
}

// env, markup both borrowed, not consumed.
static Cmd* NewEval(Cmd* next, Env* env, FbldMarkup* markup, FbldMarkup** dest)
{
  EvalCmd* eval = malloc(sizeof(EvalCmd));
  eval->_base.tag = EVAL_CMD;
  eval->_base.next = next;
  eval->_base.dest = dest;
  eval->env = CopyEnv(env);
  eval->markup = FbldCopyMarkup(markup);
  return &eval->_base;
}

/**
 * @func[HeadOf] Returns first character of markup.
 *  @arg[FbldMarkup*][m] The markup to get the first character of.
 *  @returns[int] The first character if any, 0 if end, -1 if command.
 *  @sideeffects None.
 */
static int HeadOf(FbldMarkup* m)
{
  switch (m->tag) {
    case FBLD_MARKUP_PLAIN: {
      return m->text->str[0];
    }

    case FBLD_MARKUP_COMMAND: {
      return -1;
    }

    case FBLD_MARKUP_SEQUENCE: {
      int c = 0;
      for (size_t i = 0; c == 0 && i < m->markups.size; ++i) {
        c = HeadOf(m->markups.xs[i]);
      }
      return c;
    }
  }

  assert(false && "unrechable");
  return -1;
}

/**
 * @func[TailOf] Removes first character of markup.
 *  @arg[FbldMarkup*][m] The markup to remove the character from.
 *  @returns[FbldMarkup*]
 *   A new markup without the first character. NULL if markup is empty.
 *  @sideeffects Allocates a new markup that should be freed when done.
 */
static FbldMarkup* TailOf(FbldMarkup* m)
{
  switch (m->tag) {
    case FBLD_MARKUP_PLAIN: {
      if (m->text->str[0] == '\0') {
        return NULL;
      }

      // TODO: Fix location - advance by the first character.
      FbldMarkup* n = malloc(sizeof(FbldMarkup));
      n->tag = FBLD_MARKUP_PLAIN;
      n->text = FbldNewText(m->text->loc, m->text->str + 1);
      FbldInitVector(n->markups);
      return n;
    }

    case FBLD_MARKUP_COMMAND: {
      assert(false && "Please don't call TailOf in this case.");
      return NULL;
    }

    case FBLD_MARKUP_SEQUENCE: {
      size_t i = 0;
      FbldMarkup* child = NULL;
      for (i = 0; child == NULL && i < m->markups.size; ++i) {
        child = TailOf(m->markups.xs[i]);
      }

      if (child == NULL) {
        return NULL;
      }

      if (i == m->markups.size) {
        return child;
      }

      FbldMarkup* n = malloc(sizeof(FbldMarkup));
      n->tag = FBLD_MARKUP_SEQUENCE;
      n->text = NULL;
      FbldInitVector(n->markups);
      FbldAppendToVector(n->markups, child);
      for (size_t j = i; j < m->markups.size; ++j) {
        FbldAppendToVector(n->markups, FbldCopyMarkup(m->markups.xs[j]));
      }
      return n;
    }
  }

  assert(false && "unreachable");
  return NULL;
}

/**
 * @func[Eq] Test whether two markup are equal for @ifeq
 *  @arg[FbldMarkup*][a] The first markup.
 *  @arg[FbldMarkup*][b] The second markup.
 *  @returns[bool] True if they are string equal. False otherwise.
 *  @sideeffects None
 */
static bool Eq(FbldMarkup* a, FbldMarkup* b)
{
  int ha = HeadOf(a);
  int hb = HeadOf(b);
  if (ha != hb) {
    return false;
  }

  if (ha == 0) {
    return true;
  }

  if (ha == -1) {
    assert(false && "TODO: Eq on non-string");
    return false;
  }

  FbldMarkup* na = TailOf(a);
  FbldMarkup* nb = TailOf(b);
  bool teq = Eq(na, nb);
  FbldFreeMarkup(na);
  FbldFreeMarkup(nb);
  return teq;
}

static FbldMarkup* MapPlain(const char* f, FbldMarkup* m)
{
  switch (m->tag) {
    case FBLD_MARKUP_PLAIN: {
      FbldMarkup* n = malloc(sizeof(FbldMarkup));
      n->tag = FBLD_MARKUP_COMMAND;
      n->text = FbldNewText(m->text->loc, f);
      FbldInitVector(n->markups);
      FbldAppendToVector(n->markups, FbldCopyMarkup(m));
      return n;
    }

    case FBLD_MARKUP_COMMAND: {
      return FbldCopyMarkup(m);
    }

    case FBLD_MARKUP_SEQUENCE: {
      FbldMarkup* n = malloc(sizeof(FbldMarkup));
      n->tag = m->tag;
      n->text = NULL;
      FbldInitVector(n->markups);
      for (size_t i = 0; i < m->markups.size; ++i) {
        FbldAppendToVector(n->markups, MapPlain(f, m->markups.xs[i]));
      }
      return n;
    }
  }

  assert(false && "unreachable");
  return NULL;
}

static void Eval(Cmd* cmd, bool debug)
{
  while (cmd != NULL) {
    switch (cmd->tag) {
      case EVAL_CMD: {
        EvalCmd* c = (EvalCmd*)cmd;
        FbldMarkup* markup = c->markup;

        if (debug) {
          printf("EVAL: "); FbldDebugMarkup(markup); printf("\n");
        }

        switch (markup->tag) {
          case FBLD_MARKUP_PLAIN: {
            *(c->_base.dest) = markup;
            FreeEnv(c->env);
            cmd = c->_base.next;
            free(c);
            break;
          }

          case FBLD_MARKUP_COMMAND: {
            const char* command = markup->text->str;


            // Check for user defined command.
            bool user = false;
            for (Env* e = c->env; e != NULL; e = e->next) {
              if (strcmp(command, e->name->str) == 0) {
                if (markup->markups.size != e->args.size) {
                  FbldError(markup->text->loc, "wrong number of arguments");
                }

                Env* next = CopyEnv(e);

                for (size_t i = 0; i < e->args.size; ++i) {
                  // Add arg to the envrionment for executing the body.
                  Env* ne = malloc(sizeof(Env));
                  ne->refcount = 1;
                  ne->name = FbldNewText(e->args.xs[i]->loc, e->args.xs[i]->str);
                  ne->args.xs = NULL;
                  ne->args.size = 0;
                  ne->body = NULL;
                  ne->next = next;
                  next = ne;

                  // Add a command to evaluate the argument.
                  cmd = NewEval(cmd, c->env, markup->markups.xs[i], &ne->body);
                }

                // Replace the evaluation command for the application with one
                // to evaluate the body.
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                c->env = next;
                c->markup = FbldCopyMarkup(e->body);
                user = true;
                break;
              }
            }

            if (user) {
              break;
            }

            if (strcmp(command, "error") == 0) {
              if (markup->markups.size != 1) {
                FbldError(markup->text->loc, "expected 1 argument to @error");
              }

              assert(false && "TODO: @error");
              // FbldMarkup* arg = Eval(markup->markups.xs[0], env, debug);
              // FbldText* arg_text = FbldTextOfMarkup(arg);
              // FbldFreeMarkup(arg);

              // FbldError(markup->text->loc, arg_text->str);
              // free(arg_text);
              // return NULL;
            }

            if (strcmp(command, "define") == 0) {
              if (markup->markups.size != 4) {
                FbldError(markup->text->loc, "expected 4 arguments to @define");
              }

              DefineCmd* dc = malloc(sizeof(DefineCmd));
              dc->_base.tag = DEFINE_CMD;
              dc->_base.dest = c->_base.dest;
              dc->_base.next = c->_base.next;
              dc->name = NULL;
              dc->args = NULL;
              dc->def = FbldCopyMarkup(markup->markups.xs[2]);
              dc->body = FbldCopyMarkup(markup->markups.xs[3]);
              dc->env = CopyEnv(c->env);
              cmd = &dc->_base;

              cmd = NewEval(cmd, c->env, markup->markups.xs[0], &dc->name);
              cmd = NewEval(cmd, c->env, markup->markups.xs[1], &dc->args);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              free(c);
              break;
            }

            if (strcmp(command, "let") == 0) {
              if (markup->markups.size != 3) {
                FbldError(markup->text->loc, "expected 3 arguments to @let");
              }

              assert(false && "TODO: @let");
              // FbldMarkup* name = markup->markups.xs[0];
              // FbldMarkup* def = markup->markups.xs[1];
              // FbldMarkup* body = markup->markups.xs[2];

              // assert(name->tag == FBLD_MARKUP_PLAIN && "TODO");

              // Env nenv;
              // nenv.name = name->text;
              // nenv.body = def;
              // nenv.next = env;
              // FbldInitVector(nenv.args);

              // return Eval(body, &nenv, debug);
            }

            if (strcmp(command, "head") == 0) {
              if (markup->markups.size != 1) {
                FbldError(markup->text->loc, "expected 1 arguments to @head");
              }

              assert(false && "TODO: @head");
              // FbldMarkup* str = Eval(markup->markups.xs[0], env, debug);

              // int c = HeadOf(str);
              // if (c == 0) {
              //   return str;
              // }

              // if (c == -1) {
              //   FbldError(FbldMarkupLoc(str), "argument to @head not evaluated");
              //   return NULL;
              // }

              // char plain[] = {(char)c, '\0'};
              // FbldMarkup* result = malloc(sizeof(FbldMarkup));
              // result->tag = FBLD_MARKUP_PLAIN;
              // result->text = FbldNewText(FbldMarkupLoc(str), plain);
              // FbldInitVector(result->markups);
              // FbldFreeMarkup(str);
              // return result;
            }

            if (strcmp(command, "tail") == 0) {
              if (markup->markups.size != 1) {
                FbldError(markup->text->loc, "expected 1 arguments to @tail");
              }

              assert(false && "TODO: @tail");
              // FbldMarkup* str = Eval(markup->markups.xs[0], env, debug);

              // FbldMarkup* result = TailOf(str);
              // if (result == NULL) {
              //   return str;
              // }
              // FbldFreeMarkup(str);
              // return result;
            }

            if (strcmp(command, "ifeq") == 0) {
              if (markup->markups.size != 4) {
                FbldError(markup->text->loc, "expected 4 arguments to @ifeq");
              }

              assert(false && "TODO: @ifeq");
              (void) &Eq;

              // // TODO: Handle the case where arguments can't be compared for
              // // equality.
              // FbldMarkup* a = Eval(markup->markups.xs[0], env, debug);
              // FbldMarkup* b = Eval(markup->markups.xs[1], env, debug);
              // bool eq = Eq(a, b);
              // FbldFreeMarkup(a);
              // FbldFreeMarkup(b);

              // if (eq) {
              //   return Eval(markup->markups.xs[2], env, debug);
              // }
              // return Eval(markup->markups.xs[3], env, debug);
            }

            if (strcmp(command, "ifneq") == 0) {
              if (markup->markups.size != 4) {
                FbldError(markup->text->loc, "expected 4 arguments to @ifneq");
              }

              assert(false && "TODO: @ifneq");

              // // TODO: Handle the case where arguments can't be compared for
              // // equality.
              // FbldMarkup* a = Eval(markup->markups.xs[0], env, debug);
              // FbldMarkup* b = Eval(markup->markups.xs[1], env, debug);
              // bool eq = Eq(a, b);
              // FbldFreeMarkup(a);
              // FbldFreeMarkup(b);

              // if (eq) {
              //   return Eval(markup->markups.xs[3], env, debug);
              // }
              // return Eval(markup->markups.xs[2], env, debug);
            }

            if (strcmp(command, "eval") == 0) {
              if (markup->markups.size != 1) {
                FbldError(markup->text->loc, "expected 1 argument to @eval");
              }

              assert(false && "TODO: @eval");
              // FbldMarkup* arg = Eval(markup->markups.xs[0], env, debug);
              // FbldMarkup* result = Eval(arg, env, debug);
              // FbldFreeMarkup(arg);
              // return result;
            }

            if (strcmp(command, "plain") == 0) {
              if (markup->markups.size != 2) {
                FbldError(markup->text->loc, "expected 2 argument to @plain");
              }

              assert(false && "TODO: @plain");
              (void) &MapPlain;
              // FbldMarkup* arg = Eval(markup->markups.xs[0], env, debug);
              // FbldText* arg_text = FbldTextOfMarkup(arg);
              // FbldFreeMarkup(arg);

              // FbldMarkup* evaled = Eval(markup->markups.xs[1], env, debug);

              // FbldMarkup* plained = MapPlain(arg_text->str, evaled);
              // FbldFreeMarkup(evaled);
              // free(arg_text);

              // FbldMarkup* result = Eval(plained, env, debug);
              // FbldFreeMarkup(plained);
              // return result;
            }

            // Unknown command. Leave it unevaluated for now.
            assert(false && "TODO: unknown cmd");
            // return FbldCopyMarkup(markup);
          }

          case FBLD_MARKUP_SEQUENCE: {
            FbldMarkup* m = malloc(sizeof(FbldMarkup));
            m->tag = FBLD_MARKUP_SEQUENCE;
            m->text = NULL;
            m->markups.xs = malloc(markup->markups.size * sizeof(FbldMarkup*));
            m->markups.size = markup->markups.size;
            *(c->_base.dest) = m;

            cmd = c->_base.next;
            for (size_t i = 0; i < markup->markups.size; ++i) {
              cmd = NewEval(cmd, c->env, markup->markups.xs[i], m->markups.xs + i);
            }
            FbldFreeMarkup(markup);
            FreeEnv(c->env);
            free(c);
            break;
          }
        }
        break;
      }

      case DEFINE_CMD: {
        DefineCmd* c = (DefineCmd*)cmd;

        FbldText* name = FbldTextOfMarkup(c->name);
        FbldFreeMarkup(c->name);

        FbldText* args = FbldTextOfMarkup(c->args);
        FbldFreeMarkup(c->args);

        if (debug) {
          printf("DEFINE %s(%s)\n", name->str, args->str);
        }

        Env* nenv = malloc(sizeof(Env));
        nenv->refcount = 1;
        nenv->name = name;
        nenv->body = c->def;
        nenv->next = c->env;
        FbldInitVector(nenv->args);

        char buf[strlen(args->str) + 1];
        size_t i = 0;
        for (const char* p = args->str; *p != '\0'; p++) {
          if (isspace(*p)) {
            if (i > 0) {
              // TODO: Pick a better location here.
              buf[i] = '\0';
              FbldText* text = FbldNewText(args->loc, buf);
              FbldAppendToVector(nenv->args, text);
              i = 0;
            }
            continue;
          }

          buf[i] = *p;
          i++;
        }

        if (i > 0) {
          // TODO: Pick a better location here.
          buf[i] = '\0';
          FbldText* text = FbldNewText(args->loc, buf);
          FbldAppendToVector(nenv->args, text);
        }

        cmd = NewEval(c->_base.next, nenv, c->body, c->_base.dest);
        free(args);
        FreeEnv(nenv);
        FbldFreeMarkup(c->body);
        break;
      }
    }
  }
}

FbldMarkup* FbldEval(FbldMarkup* markup, bool debug)
{
  FbldMarkup* result = NULL;

  Eval(NewEval(NULL, NULL, markup, &result), debug);
  return result;
}
