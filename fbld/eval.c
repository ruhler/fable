
#include <assert.h>   // for assert
#include <ctype.h>    // for isspace
#include <stdbool.h>  // for false
#include <stdio.h>    // for printf
#include <string.h>   // for strcmp

#include "alloc.h"
#include "fbld.h"
#include "vector.h"

/**
 * @struct[Env] Environment for execution.
 *  The environment is the owner of its name, args (vector and elements),
 *  body, and next.
 *
 *  The args.xs field is NULL to indicate this is a variable definition
 *  instead of a function definition. In that case the body has already been
 *  evaluated.
 *
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
  DEFINE_CMD,
  LET_CMD,
  IF_CMD,
  ERROR_CMD,
  HEAD_CMD,
  TAIL_CMD,
  PLAIN_CMD,
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

/**
 * @struct[EvalCmd] EVAL_CMD arguments.
 *  Evaluates @a[markup] in the environment @a[env].
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[Env*][env] Environment to evaluate under. Owned.
 *  @field[FbldMarkup*][markup] Markup to evaluate. Owned.
 */
typedef struct {
  Cmd _base;
  Env* env;
  FbldMarkup* markup;
} EvalCmd;

/**
 * @struct[DefineCmd] DEFINE_CMD arguments.
 *  Defines a new command.
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[FbldMarkup*][name] Evaluated name of the command to define. Owned.
 *  @field[FbldMarkup*][args] Evaluated argument name list. Owned.
 *  @field[FbldMarkup*][def] Definition of the command. Owned.
 *  @field[FbldMarkup*][body] Markup to evaluate with command defined. Owned.
 *  @field[Env*][env] Environment to add the definition to. Owned.
 */
typedef struct {
  Cmd _base;
  FbldMarkup* name;
  FbldMarkup* args;
  FbldMarkup* def;
  FbldMarkup* body;
  Env* env;
} DefineCmd;

/**
 * @struct[LetCmd] LET_CMD arguments.
 *  Defines a new variable.
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[FbldMarkup*][name] Evaluated name of the variable to define. Owned.
 *  @field[FbldMarkup*][def] Evaluated value of the variable. Owned.
 *  @field[FbldMarkup*][body] Markup to evaluate with variable defined. Owned.
 *  @field[Env*][env] Environment to add the variable to. Owned.
 */
typedef struct {
  Cmd _base;
  FbldMarkup* name;
  FbldMarkup* def;
  FbldMarkup* body;
  Env* env;
} LetCmd;

/**
 * @struct[IfCmd] IF_CMD arguments.
 *  Conditional execution.
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[FbldMarkup*][a] Evaluated left hand side of comparison. Owned.
 *  @field[FbldMarkup*][b] Evaluated right hand side of comparison. Owned.
 *  @field[FbldMarkup*][if_eq] Markup to evaluate if a equals b. Owned.
 *  @field[FbldMarkup*][if_ne] Markup to evaluate if a doesn't equal b. Owned.
 *  @field[Env*][env] Environment to evaluate the body in.
 */
typedef struct {
  Cmd _base;
  FbldMarkup* a;
  FbldMarkup* b;
  FbldMarkup* if_eq;
  FbldMarkup* if_ne;
  Env* env;
} IfCmd;

/**
 * @struct[ErrorCmd] ERROR_CMD arguments.
 *  Explicitly trigger an error.
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[FbldLoc][loc] Location to report with the error message.
 *  @field[FbldMarkup*][msg] Evaluated message to report.. Owned.
 */
typedef struct {
  Cmd _base;
  FbldLoc loc;
  FbldMarkup* msg;
} ErrorCmd;

/**
 * @struct[HeadCmd] HEAD_CMD arguments.
 *  Gets the first character of the given markup.
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[FbldMarkup*][a] Evaluated markup to get first character of. Owned.
 */
typedef struct {
  Cmd _base;
  FbldMarkup* a;
} HeadCmd;

/**
 * @struct[TailCmd] TAIL_CMD arguments.
 *  Gets all but the first character of the given markup.
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[FbldMarkup*][a] Evaluated markup to get the tail of. Owned.
 */
typedef struct {
  Cmd _base;
  FbldMarkup* a;
} TailCmd;

/**
 * @struct[PlainCmd] PLAIN_CMD arguments.
 *  The \@plain builtin.
 *
 *  @field[Cmd][_base] The Cmd base class.
 *  @field[FbldMarkup*][f] Evaluated command name to apply to plain text. Owned.
 *  @field[FbldMarkup*][body] Markup to apply plain function too. Owned.
 *  @field[Env*][env] Environment to evaluate the body in.
 */
typedef struct {
  Cmd _base;
  FbldMarkup* f;
  FbldMarkup* body;
  Env* env;
} PlainCmd;

static Env* CopyEnv(Env* env);
static void FreeEnv(Env* env);

static Cmd* PushEval(Cmd* next, Env* env, FbldMarkup* markup, FbldMarkup** dest);

static int HeadOf(FbldMarkup* m);
static FbldMarkup* TailOf(FbldMarkup* m);
static bool Eq(FbldMarkup* a, FbldMarkup* b);
static FbldMarkup* MapPlain(const char* f, FbldMarkup* m);
static bool Eval(Cmd* cmd);

/**
 * @func[CopyEnv] Make reference counted copy of environment.
 *  @arg[Env*][env] The environment to copy. Borrowed.
 *  @returns[Env*] The copied environment.
 *  @sideeffects
 *   The user should call FreeEnv on the returned environment when they are
 *   done with it.
 */
static Env* CopyEnv(Env* env)
{
  if (env != NULL) {
    env->refcount++;
  }
  return env;
}

/**
 * @func[FreeEnv] Frees resources associated with an environment.
 *  @arg[Env*][env] The environment to copy.
 *  @sideeffects
 *   Frees resources associated with the environment, which should not be
 *   accessed after this call.
 */
static void FreeEnv(Env* env)
{
  while (env != NULL) {
    if (--env->refcount > 0) {
      return;
    }

    FbldFree(env->name);
    for (size_t i = 0; i < env->args.size; ++i) {
      FbldFree(env->args.xs[i]);
    }
    FbldFree(env->args.xs);
    FbldFreeMarkup(env->body);
    Env* next = env->next;
    FbldFree(env);
    env = next;
  }
}

/**
 * @func[PushEval] Adds a command to evaluate a markup.
 *  @arg[Cmd*][next] The command to evaluate after this one.
 *  @arg[Env*][env] The environment to evaluate the markup in. Borrowed.
 *  @arg[FbldMarkup*][markup] The markup to evaluate. Borrowed.
 *  @arg[FbldMarkup**][dest] Where to store the evaluated markup.
 *  @returns[Cmd*] The newly allocated command.
 *  @sideeffects
 *   Allocates a new command that will be freed by Eval when the command is
 *   executed.
 */
static Cmd* PushEval(Cmd* next, Env* env, FbldMarkup* markup, FbldMarkup** dest)
{
  EvalCmd* eval = FbldAlloc(EvalCmd);
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
 *  @sideeffects
 *   Allocates a new markup that should be freed with FbldFreeMarkup when done.
 */
static FbldMarkup* TailOf(FbldMarkup* m)
{
  switch (m->tag) {
    case FBLD_MARKUP_PLAIN: {
      if (m->text->str[0] == '\0') {
        return NULL;
      }

      // TODO: Fix location - advance by the first character.
      FbldMarkup* n = FbldAlloc(FbldMarkup);
      n->tag = FBLD_MARKUP_PLAIN;
      n->refcount = 1;
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

      FbldMarkup* n = FbldAlloc(FbldMarkup);
      n->tag = FBLD_MARKUP_SEQUENCE;
      n->text = NULL;
      n->refcount = 1;
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

/**
 * @func[MapPlain] Applies function f to each plain text in the given markup.
 *  @arg[const char*][f] Name of the function to apply. Borrowed.
 *  @arg[FbldMarkup*][m] Markup to apply the function in.
 *  @returns[FbldMarkup*] Markup with calls to f inserted.
 *  @sideeffects
 *   Allocates markup that should be freed with FbldFreeMarkup when no longer
 *   needed.
 */
static FbldMarkup* MapPlain(const char* f, FbldMarkup* m)
{
  switch (m->tag) {
    case FBLD_MARKUP_PLAIN: {
      FbldMarkup* n = FbldAlloc(FbldMarkup);
      n->tag = FBLD_MARKUP_COMMAND;
      n->text = FbldNewText(m->text->loc, f);
      n->refcount = 1;
      FbldInitVector(n->markups);
      FbldAppendToVector(n->markups, FbldCopyMarkup(m));
      return n;
    }

    case FBLD_MARKUP_COMMAND: {
      return FbldCopyMarkup(m);
    }

    case FBLD_MARKUP_SEQUENCE: {
      FbldMarkup* n = FbldAlloc(FbldMarkup);
      n->tag = m->tag;
      n->text = NULL;
      n->refcount = 1;
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

/**
 * @func[Eval] Evaluates a command to completion.
 *  @arg[Cmd*][cmd] The command to evaluate. Consumed.
 *  @returns[bool] True of success, false in case of error.
 *  @sideeffects
 *   @i Frees resources associated with cmd.
 *   @i Executes the command, storing a value to cmd->dest.
 *   @item
 *    The caller should call FbldFreeMarkup on cmd->dest when the result is no
 *    longer needed.
 */
static bool Eval(Cmd* cmd)
{
  bool error = false;
  while (cmd != NULL) {
    switch (cmd->tag) {
      case EVAL_CMD: {
        EvalCmd* c = (EvalCmd*)cmd;
        FbldMarkup* markup = c->markup;

        if (markup == NULL) {
          // We must have hit an error earlier. Bail out here.
          assert(error && "prior error not recorded properly?");

          EvalCmd* e = (EvalCmd*)cmd;
          cmd = cmd->next;
          FreeEnv(e->env);
          break;
        }

        switch (markup->tag) {
          case FBLD_MARKUP_PLAIN: {
            *(c->_base.dest) = markup;
            FreeEnv(c->env);
            cmd = c->_base.next;
            FbldFree(c);
            break;
          }

          case FBLD_MARKUP_COMMAND: {
            const char* command = markup->text->str;

            // Check for user defined command.
            bool user = false;
            for (Env* e = c->env; e != NULL; e = e->next) {
              if (strcmp(command, e->name->str) == 0) {
                user = true;
                if (markup->markups.size != e->args.size) {
                  FbldReportError(markup->text->loc, "wrong number of arguments");
                  error = true;
                  cmd = cmd->next;
                  FreeEnv(c->env);
                  FbldFreeMarkup(c->markup);
                  FbldFree(c);
                  break;
                }

                if (e->args.xs == NULL) {
                  // This is a variable lookup rather than a function
                  // application. Handle it directly.
                  *(c->_base.dest) = FbldCopyMarkup(e->body);
                  FreeEnv(c->env);
                  FbldFreeMarkup(c->markup);
                  cmd = c->_base.next;
                  FbldFree(c);
                  break;
                }

                Env* next = CopyEnv(e);

                for (size_t i = 0; i < e->args.size; ++i) {
                  // Add arg to the envrionment for executing the body.
                  Env* ne = FbldAlloc(Env);
                  ne->refcount = 1;
                  ne->name = FbldNewText(e->args.xs[i]->loc, e->args.xs[i]->str);
                  ne->args.xs = NULL;
                  ne->args.size = 0;
                  ne->body = NULL;
                  ne->next = next;
                  next = ne;

                  // Add a command to evaluate the argument.
                  cmd = PushEval(cmd, c->env, markup->markups.xs[i], &ne->body);
                }

                // Replace the evaluation command for the application with one
                // to evaluate the body.
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                c->env = next;
                c->markup = FbldCopyMarkup(e->body);
                break;
              }
            }

            if (user) {
              break;
            }

            if (strcmp(command, "error") == 0) {
              if (markup->markups.size != 1) {
                FbldReportError(markup->text->loc, "expected 1 argument to @error");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              ErrorCmd* ec = FbldAlloc(ErrorCmd);
              ec->_base.tag = ERROR_CMD;
              ec->_base.dest = c->_base.dest;
              ec->_base.next = c->_base.next;
              ec->loc = markup->text->loc;
              ec->msg = NULL;
              cmd = &ec->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &ec->msg);
              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            if (strcmp(command, "define") == 0) {
              if (markup->markups.size != 4) {
                FbldReportError(markup->text->loc, "expected 4 arguments to @define");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              DefineCmd* dc = FbldAlloc(DefineCmd);
              dc->_base.tag = DEFINE_CMD;
              dc->_base.dest = c->_base.dest;
              dc->_base.next = c->_base.next;
              dc->name = NULL;
              dc->args = NULL;
              dc->def = FbldCopyMarkup(markup->markups.xs[2]);
              dc->body = FbldCopyMarkup(markup->markups.xs[3]);
              dc->env = CopyEnv(c->env);
              cmd = &dc->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &dc->name);
              cmd = PushEval(cmd, c->env, markup->markups.xs[1], &dc->args);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            if (strcmp(command, "let") == 0) {
              if (markup->markups.size != 3) {
                FbldReportError(markup->text->loc, "expected 3 arguments to @let");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              LetCmd* lc = FbldAlloc(LetCmd);
              lc->_base.tag = LET_CMD;
              lc->_base.dest = c->_base.dest;
              lc->_base.next = c->_base.next;
              lc->name = NULL;

              lc->def = NULL;
              lc->body = FbldCopyMarkup(markup->markups.xs[2]);
              lc->env = CopyEnv(c->env);
              cmd = &lc->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &lc->name);
              cmd = PushEval(cmd, c->env, markup->markups.xs[1], &lc->def);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            if (strcmp(command, "head") == 0) {
              if (markup->markups.size != 1) {
                FbldReportError(markup->text->loc, "expected 1 arguments to @head");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              HeadCmd* hc = FbldAlloc(HeadCmd);
              hc->_base.tag = HEAD_CMD;
              hc->_base.dest = c->_base.dest;
              hc->_base.next = c->_base.next;
              hc->a = NULL;
              cmd = &hc->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &hc->a);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            if (strcmp(command, "tail") == 0) {
              if (markup->markups.size != 1) {
                FbldReportError(markup->text->loc, "expected 1 arguments to @tail");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              TailCmd* tc = FbldAlloc(TailCmd);
              tc->_base.tag = TAIL_CMD;
              tc->_base.dest = c->_base.dest;
              tc->_base.next = c->_base.next;
              tc->a = NULL;
              cmd = &tc->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &tc->a);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            if (strcmp(command, "ifeq") == 0) {
              if (markup->markups.size != 4) {
                FbldReportError(markup->text->loc, "expected 4 arguments to @ifeq");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              IfCmd* ic = FbldAlloc(IfCmd);
              ic->_base.tag = IF_CMD;
              ic->_base.dest = c->_base.dest;
              ic->_base.next = c->_base.next;
              ic->a = NULL;
              ic->b = NULL;
              ic->if_eq = FbldCopyMarkup(markup->markups.xs[2]);
              ic->if_ne = FbldCopyMarkup(markup->markups.xs[3]);
              ic->env = CopyEnv(c->env);
              cmd = &ic->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &ic->a);
              cmd = PushEval(cmd, c->env, markup->markups.xs[1], &ic->b);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            if (strcmp(command, "ifneq") == 0) {
              if (markup->markups.size != 4) {
                FbldReportError(markup->text->loc, "expected 4 arguments to @ifneq");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              IfCmd* ic = FbldAlloc(IfCmd);
              ic->_base.tag = IF_CMD;
              ic->_base.dest = c->_base.dest;
              ic->_base.next = c->_base.next;
              ic->a = NULL;
              ic->b = NULL;
              ic->if_eq = FbldCopyMarkup(markup->markups.xs[3]);
              ic->if_ne = FbldCopyMarkup(markup->markups.xs[2]);
              ic->env = CopyEnv(c->env);
              cmd = &ic->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &ic->a);
              cmd = PushEval(cmd, c->env, markup->markups.xs[1], &ic->b);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            if (strcmp(command, "eval") == 0) {
              if (markup->markups.size != 1) {
                FbldReportError(markup->text->loc, "expected 1 argument to @eval");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }
              
              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &c->markup);
              FbldFreeMarkup(c->markup);
              c->markup = NULL;
              break;
            }

            if (strcmp(command, "plain") == 0) {
              if (markup->markups.size != 2) {
                FbldReportError(markup->text->loc, "expected 2 argument to @plain");
                error = true;
                cmd = cmd->next;
                FreeEnv(c->env);
                FbldFreeMarkup(c->markup);
                FbldFree(c);
                break;
              }

              PlainCmd* pc = FbldAlloc(PlainCmd);
              pc->_base.tag = PLAIN_CMD;
              pc->_base.dest = c->_base.dest;
              pc->_base.next = c->_base.next;
              pc->f = NULL;
              pc->body = NULL;
              pc->env = CopyEnv(c->env);
              cmd = &pc->_base;

              cmd = PushEval(cmd, c->env, markup->markups.xs[0], &pc->f);
              cmd = PushEval(cmd, c->env, markup->markups.xs[1], &pc->body);

              FreeEnv(c->env);
              FbldFreeMarkup(c->markup);
              FbldFree(c);
              break;
            }

            // Unknown command. Leave it unevaluated for now.
            *(c->_base.dest) = c->markup;
            cmd = c->_base.next;
            FreeEnv(c->env);
            FbldFree(c);
            break;
          }

          case FBLD_MARKUP_SEQUENCE: {
            FbldMarkup* m = FbldAlloc(FbldMarkup);
            m->tag = FBLD_MARKUP_SEQUENCE;
            m->text = NULL;
            m->refcount = 1;
            FbldInitVector(m->markups);
            for (size_t i = 0; i < markup->markups.size; ++i) {
              FbldAppendToVector(m->markups, NULL);
            }

            *(c->_base.dest) = m;

            cmd = c->_base.next;
            for (size_t i = 0; i < markup->markups.size; ++i) {
              cmd = PushEval(cmd, c->env, markup->markups.xs[i], m->markups.xs + i);
            }
            FbldFreeMarkup(markup);
            FreeEnv(c->env);
            FbldFree(c);
            break;
          }

          default: {
            assert(false && "unreachable");
            break;
          }
        }
        break;
      }

      case DEFINE_CMD: {
        DefineCmd* c = (DefineCmd*)cmd;

        FbldText* name = FbldTextOfMarkup(c->name);
        if (name == NULL) {
          error = true;
          cmd = cmd->next;
          FbldFreeMarkup(c->name);
          FbldFreeMarkup(c->args);
          FbldFreeMarkup(c->def);
          FbldFreeMarkup(c->body);
          FreeEnv(c->env);
          FbldFree(c);
          break;
        }
        FbldFreeMarkup(c->name);

        FbldText* args = FbldTextOfMarkup(c->args);
        if (args == NULL) {
          error = true;
          cmd = cmd->next;
          FbldFreeMarkup(c->name);
          FbldFreeMarkup(c->args);
          FbldFreeMarkup(c->def);
          FbldFreeMarkup(c->body);
          FreeEnv(c->env);
          FbldFree(c);
          break;
        }
        FbldFreeMarkup(c->args);

        Env* nenv = FbldAlloc(Env);
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

        cmd = PushEval(c->_base.next, nenv, c->body, c->_base.dest);
        FbldFree(args);
        FreeEnv(nenv);
        FbldFreeMarkup(c->body);
        FbldFree(c);
        break;
      }

      case LET_CMD: {
        LetCmd* c = (LetCmd*)cmd;

        FbldText* name = FbldTextOfMarkup(c->name);
        if (name == NULL) {
          error = true;
          cmd = cmd->next;
          FbldFreeMarkup(c->name);
          FbldFreeMarkup(c->def);
          FbldFreeMarkup(c->body);
          FreeEnv(c->env);
          FbldFree(c);
          break;
        }
        FbldFreeMarkup(c->name);

        Env* nenv = FbldAlloc(Env);
        nenv->refcount = 1;
        nenv->name = name;
        nenv->body = c->def;
        nenv->next = c->env;
        nenv->args.xs = NULL;
        nenv->args.size = 0;

        cmd = PushEval(c->_base.next, nenv, c->body, c->_base.dest);
        FreeEnv(nenv);
        FbldFreeMarkup(c->body);
        FbldFree(c);
        break;
      }

      case IF_CMD: {
        IfCmd* c = (IfCmd*)cmd;

        // TODO: Handle the case where arguments can't be compared for
        // equality.
        bool eq = Eq(c->a, c->b);

        if (eq) {
          cmd = PushEval(c->_base.next, c->env, c->if_eq, c->_base.dest);
        } else {
          cmd = PushEval(c->_base.next, c->env, c->if_ne, c->_base.dest);
        }

        FbldFreeMarkup(c->a);
        FbldFreeMarkup(c->b);
        FbldFreeMarkup(c->if_eq);
        FbldFreeMarkup(c->if_ne);
        FreeEnv(c->env);
        FbldFree(c);
        break;
      }

      case ERROR_CMD: {
        ErrorCmd* c = (ErrorCmd*)cmd;

        FbldText* msg = FbldTextOfMarkup(c->msg);
        if (msg == NULL) {
          error = true;
          cmd = cmd->next;
          FbldFreeMarkup(c->msg);
          FbldFree(c);
          break;
        }

        FbldReportError(c->loc, msg->str);
        FbldFree(msg);
        error = true;
        cmd = cmd->next;
        FbldFreeMarkup(c->msg);
        FbldFree(c);
        break;
      }

      case HEAD_CMD: {
        HeadCmd* c = (HeadCmd*)cmd;

        int ch = HeadOf(c->a);
        if (ch == 0) {
          *(c->_base.dest) = c->a;
          cmd = c->_base.next;
          FbldFree(c);
          break;
        }

        if (ch == -1) {
          FbldReportError(FbldMarkupLoc(c->a), "argument to @head not evaluated");
          error = true;
          cmd = cmd->next;
          FbldFreeMarkup(c->a);
          FbldFree(c);
          break;
        }

        char plain[] = {(char)ch, '\0'};
        FbldMarkup* result = FbldAlloc(FbldMarkup);
        result->tag = FBLD_MARKUP_PLAIN;
        result->text = FbldNewText(FbldMarkupLoc(c->a), plain);
        result->refcount = 1;
        FbldInitVector(result->markups);
        *(c->_base.dest) = result;
        FbldFreeMarkup(c->a);
        cmd = c->_base.next;
        FbldFree(c);
        break;
      }

      case TAIL_CMD: {
        TailCmd* c = (TailCmd*)cmd;

        FbldMarkup* result = TailOf(c->a);
        if (result == NULL) {
          *(c->_base.dest) = c->a;
          cmd = c->_base.next;
          FbldFree(c);
          break;
        }

        *(c->_base.dest) = result;
        FbldFreeMarkup(c->a);
        cmd = c->_base.next;
        FbldFree(c);
        break;
      }

      case PLAIN_CMD: {
        PlainCmd* c = (PlainCmd*)cmd;

        FbldText* f = FbldTextOfMarkup(c->f);
        if (f == NULL) {
          error = true;
          cmd = cmd->next;
          FbldFreeMarkup(c->f);
          FbldFreeMarkup(c->body);
          FreeEnv(c->env);
          FbldFree(c);
          break;
        }
        FbldMarkup* plained = MapPlain(f->str, c->body);
        FbldFreeMarkup(c->f);
        FbldFreeMarkup(c->body);
        FbldFree(f);

        cmd = PushEval(c->_base.next, c->env, plained, c->_base.dest);
        FbldFreeMarkup(plained);
        FreeEnv(c->env);
        FbldFree(c);
        break;
      }
    }
  }
  return !error;
}

// See documentation in fbld.h.
FbldMarkup* FbldEval(FbldMarkup* markup)
{
  FbldMarkup* result = NULL;
  if (!Eval(PushEval(NULL, NULL, markup, &result))) {
    FbldFreeMarkup(result);
    return NULL;
  }

  return result;
}
