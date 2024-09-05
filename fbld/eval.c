
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
 *  @field[FbldText*][name] The name of the first defined function.
 *  @field[FbldTextV][args] Names of arguments to the first defined function.
 *  @field[FbldMarkup*][body] Body of the first defined function.
 *  @field[Env*][next] Rest of the defined functions or NULL.
 */
typedef struct Env {
  FbldText* name;
  FbldTextV args;
  FbldMarkup* body;
  struct Env* next;
} Env;

static int HeadOf(FbldMarkup* m);
static FbldMarkup* TailOf(FbldMarkup* m);
static bool Eq(FbldMarkup* a, FbldMarkup* b);
static FbldMarkup* MapPlain(const char* f, FbldMarkup* m);
static FbldMarkup* Eval(FbldMarkup* markup, Env* env, bool debug);

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

static FbldMarkup* Eval(FbldMarkup* markup, Env* env, bool debug)
{
  if (debug) {
    printf("EVAL: "); FbldDebugMarkup(markup); printf("\n");
  }

  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      return FbldCopyMarkup(markup);
    }

    case FBLD_MARKUP_COMMAND: {
      const char* command = markup->text->str;

      for (Env* e = env; e != NULL; e = e->next) {
        if (strcmp(command, e->name->str) == 0) {
          if (markup->markups.size != e->args.size) {
            FbldError(markup->text->loc, "wrong number of arguments");
            return NULL;
          }

          Env envs[e->args.size];
          Env* next = e;
          for (size_t i = 0; i < e->args.size; ++i) {
            envs[i].name = e->args.xs[i];
            envs[i].args.xs = NULL;
            envs[i].args.size = 0;
            envs[i].body = Eval(markup->markups.xs[i], env, debug);
            envs[i].next = next;
            next = envs + i;
          }

          FbldMarkup* result = Eval(e->body, next, debug);
          for (size_t i = 0; i < e->args.size; ++i) {
            FbldFreeMarkup(envs[i].body);
          }
          return result;
        }
      }

      if (strcmp(command, "error") == 0) {
        if (markup->markups.size != 1) {
          FbldError(markup->text->loc, "expected 1 argument to @error");
          return NULL;
        }

        FbldMarkup* arg = Eval(markup->markups.xs[0], env, debug);
        FbldText* arg_text = FbldTextOfMarkup(arg);
        FbldFreeMarkup(arg);

        FbldError(markup->text->loc, arg_text->str);
        free(arg_text);
        return NULL;
      }

      if (strcmp(command, "define") == 0) {
        if (markup->markups.size != 4) {
          FbldError(markup->text->loc, "expected 4 arguments to @define");
          return NULL;
        }

        FbldMarkup* name = Eval(markup->markups.xs[0], env, debug);
        FbldText* name_text = FbldTextOfMarkup(name);
        FbldFreeMarkup(name);

        FbldMarkup* args = Eval(markup->markups.xs[1], env, debug);
        FbldText* args_text = FbldTextOfMarkup(args);
        FbldFreeMarkup(args);

        FbldMarkup* def = markup->markups.xs[2];
        FbldMarkup* body = markup->markups.xs[3];

        Env nenv;
        nenv.name = name_text;
        nenv.body = def;
        nenv.next = env;

        FbldInitVector(nenv.args);
        char buf[strlen(args_text->str) + 1];
        size_t i = 0;
        for (const char* p = args_text->str; *p != '\0'; p++) {
          if (isspace(*p)) {
            if (i > 0) {
              // TODO: Pick a better location here.
              buf[i] = '\0';
              FbldText* text = FbldNewText(args_text->loc, buf);
              FbldAppendToVector(nenv.args, text);
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
          FbldText* text = FbldNewText(args_text->loc, buf);
          FbldAppendToVector(nenv.args, text);
        }

        FbldMarkup* result = Eval(body, &nenv, debug);
        free(name_text);
        free(args_text);

        for (size_t j = 0; j < nenv.args.size; ++j) {
          free(nenv.args.xs[j]);
        }
        free(nenv.args.xs);

        return result;
      }

      if (strcmp(command, "let") == 0) {
        if (markup->markups.size != 3) {
          FbldError(markup->text->loc, "expected 3 arguments to @let");
          return NULL;
        }

        FbldMarkup* name = markup->markups.xs[0];
        FbldMarkup* def = markup->markups.xs[1];
        FbldMarkup* body = markup->markups.xs[2];

        assert(name->tag == FBLD_MARKUP_PLAIN && "TODO");

        Env nenv;
        nenv.name = name->text;
        nenv.body = def;
        nenv.next = env;
        FbldInitVector(nenv.args);

        return Eval(body, &nenv, debug);
      }

      if (strcmp(command, "head") == 0) {
        if (markup->markups.size != 1) {
          FbldError(markup->text->loc, "expected 1 arguments to @head");
          return NULL;
        }

        FbldMarkup* str = Eval(markup->markups.xs[0], env, debug);

        int c = HeadOf(str);
        if (c == 0) {
          return str;
        }

        if (c == -1) {
          FbldError(FbldMarkupLoc(str), "argument to @head not evaluated");
          return NULL;
        }

        char plain[] = {(char)c, '\0'};
        FbldMarkup* result = malloc(sizeof(FbldMarkup));
        result->tag = FBLD_MARKUP_PLAIN;
        result->text = FbldNewText(FbldMarkupLoc(str), plain);
        FbldInitVector(result->markups);
        FbldFreeMarkup(str);
        return result;
      }

      if (strcmp(command, "tail") == 0) {
        if (markup->markups.size != 1) {
          FbldError(markup->text->loc, "expected 1 arguments to @tail");
          return NULL;
        }

        FbldMarkup* str = Eval(markup->markups.xs[0], env, debug);

        FbldMarkup* result = TailOf(str);
        if (result == NULL) {
          return str;
        }
        FbldFreeMarkup(str);
        return result;
      }

      if (strcmp(command, "ifeq") == 0) {
        if (markup->markups.size != 4) {
          FbldError(markup->text->loc, "expected 4 arguments to @ifeq");
          return NULL;
        }

        // TODO: Handle the case where arguments can't be compared for
        // equality.
        FbldMarkup* a = Eval(markup->markups.xs[0], env, debug);
        FbldMarkup* b = Eval(markup->markups.xs[1], env, debug);
        bool eq = Eq(a, b);
        FbldFreeMarkup(a);
        FbldFreeMarkup(b);

        if (eq) {
          return Eval(markup->markups.xs[2], env, debug);
        }
        return Eval(markup->markups.xs[3], env, debug);
      }

      if (strcmp(command, "ifneq") == 0) {
        if (markup->markups.size != 4) {
          FbldError(markup->text->loc, "expected 4 arguments to @ifneq");
          return NULL;
        }

        // TODO: Handle the case where arguments can't be compared for
        // equality.
        FbldMarkup* a = Eval(markup->markups.xs[0], env, debug);
        FbldMarkup* b = Eval(markup->markups.xs[1], env, debug);
        bool eq = Eq(a, b);
        FbldFreeMarkup(a);
        FbldFreeMarkup(b);

        if (eq) {
          return Eval(markup->markups.xs[3], env, debug);
        }
        return Eval(markup->markups.xs[2], env, debug);
      }

      if (strcmp(command, "eval") == 0) {
        if (markup->markups.size != 1) {
          FbldError(markup->text->loc, "expected 1 argument to @eval");
          return NULL;
        }

        FbldMarkup* arg = Eval(markup->markups.xs[0], env, debug);
        FbldMarkup* result = Eval(arg, env, debug);
        FbldFreeMarkup(arg);
        return result;
      }

      if (strcmp(command, "plain") == 0) {
        if (markup->markups.size != 2) {
          FbldError(markup->text->loc, "expected 2 argument to @plain");
          return NULL;
        }

        FbldMarkup* arg = Eval(markup->markups.xs[0], env, debug);
        FbldText* arg_text = FbldTextOfMarkup(arg);
        FbldFreeMarkup(arg);

        FbldMarkup* evaled = Eval(markup->markups.xs[1], env, debug);

        FbldMarkup* plained = MapPlain(arg_text->str, evaled);
        FbldFreeMarkup(evaled);
        free(arg_text);

        FbldMarkup* result = Eval(plained, env, debug);
        FbldFreeMarkup(plained);
        return result;
      }

      // Unknown command. Leave it unevaluated for now.
      return FbldCopyMarkup(markup);
    }

    case FBLD_MARKUP_SEQUENCE: {
      FbldMarkup* m = malloc(sizeof(FbldMarkup));
      m->tag = FBLD_MARKUP_SEQUENCE;
      m->text = NULL;
      FbldInitVector(m->markups);
      for (size_t i = 0; i < markup->markups.size; ++i) {
        FbldAppendToVector(m->markups, Eval(markup->markups.xs[i], env, debug));
      }
      return m;
    }
  }

  return NULL;
}

FbldMarkup* FbldEval(FbldMarkup* markup, bool debug)
{
  return Eval(markup, NULL, debug);
}
