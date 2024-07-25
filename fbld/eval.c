
#include <assert.h>   // for assert
#include <ctype.h>    // for isspace
#include <stdbool.h>  // for false
#include <stdlib.h>   // for malloc
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

static bool Eq(FbldMarkup* a, FbldMarkup* b);

/**
 * @func[Eq] Test whether two markup are equal for @ifeq
 *  @arg[FbldMarkup*][a] The first markup.
 *  @arg[FbldMarkup*][b] The second markup.
 *  @returns[bool] True if they are string equal. False otherwise.
 *  @sideeffects None
 */
static bool Eq(FbldMarkup* a, FbldMarkup* b)
{
  assert(false && "TODO");
  return false;
}

FbldMarkup* Eval(FbldMarkup* markup, Env* env)
{
  switch (markup->tag) {
    case FBLD_MARKUP_PLAIN: {
      // TODO: Use reference counting to avoid a copy here?
      FbldMarkup* m = malloc(sizeof(FbldMarkup));
      m->tag = FBLD_MARKUP_PLAIN;
      m->text = malloc(sizeof(FbldText));
      m->text->loc = markup->text->loc;
      m->text->str = markup->text->str;
      m->text->str->refcount++;
      FbldInitVector(m->markups);
      return m;
    }

    case FBLD_MARKUP_COMMAND: {
      const char* command = markup->text->str->str;

      for (Env* e = env; e != NULL; e = e->next) {
        if (strcmp(command, e->name->str->str) == 0) {
          if (markup->markups.size != e->args.size) {
            FbldError(markup->text->loc, "wrong number of arguments");
            return NULL;
          }

          // TODO: The body should be evaluated in the function's scope, not
          // the caller's scope, right?
          Env envs[e->args.size];
          for (size_t i = 0; i < e->args.size; ++i) {
            envs[i].name = e->args.xs[i];
            envs[i].args.xs = NULL;
            envs[i].args.size = 0;
            envs[i].body = Eval(markup->markups.xs[i], env);
            envs[i].next = (i == 0) ? env : (envs + i - 1);
          }

          FbldMarkup* result = Eval(e->body, envs + e->args.size - 1);
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

        FbldMarkup* arg = markup->markups.xs[0];
        if (arg->tag != FBLD_MARKUP_PLAIN) {
          // TODO: Sequence of plain ought to be allowed too, right?
          FbldError(markup->text->loc, "argument to @error is not plain");
          return NULL;
        }

        FbldError(markup->text->loc, arg->text->str->str);
        return NULL;
      }

      if (strcmp(command, "define") == 0) {
        if (markup->markups.size != 4) {
          FbldError(markup->text->loc, "expected 4 arguments to @define");
          return NULL;
        }

        FbldMarkup* name = markup->markups.xs[0];
        FbldMarkup* args = markup->markups.xs[1];
        FbldMarkup* def = markup->markups.xs[2];
        FbldMarkup* body = markup->markups.xs[3];

        assert(name->tag == FBLD_MARKUP_PLAIN && "TODO");
        assert(args->tag == FBLD_MARKUP_PLAIN && "TODO");

        Env nenv;
        nenv.name = name->text;
        nenv.body = def;
        nenv.next = env;

        FbldInitVector(nenv.args);
        char buf[strlen(args->text->str->str) + 1];
        size_t i = 0;
        for (const char* p = args->text->str->str; *p != '\0'; p++) {
          if (isspace(*p)) {
            if (i > 0) {
              // TODO: Pick a better location here.
              buf[i] = '\0';
              FbldText* text = malloc(sizeof(FbldText));
              text->loc = args->text->loc;
              text->str = malloc(sizeof(FbldString) + sizeof(char) * (1+i));
              text->str->refcount = 1;
              strcpy(text->str->str, buf);
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
          FbldText* text = malloc(sizeof(FbldText));
          text->loc = args->text->loc;
          text->str = malloc(sizeof(FbldString) + sizeof(char) * (1+i));
          text->str->refcount = 1;
          strcpy(text->str->str, buf);
          FbldAppendToVector(nenv.args, text);
        }

        FbldMarkup* result = Eval(body, &nenv);

        for (size_t j = 0; j < nenv.args.size; ++j) {
          nenv.args.xs[j]->str->refcount--;
          if (nenv.args.xs[j]->str->refcount == 0) {
            free(nenv.args.xs[j]->str);
          }
          free(nenv.args.xs[j]);
        }
        free(nenv.args.xs);

        return result;
      }

      if (strcmp(command, "ifeq") == 0) {
        if (markup->markups.size != 4) {
          FbldError(markup->text->loc, "expected 4 arguments to @ifeq");
          return NULL;
        }

        // TODO: Handle the case where arguments can't be compared for
        // equality.
        if (Eq(markup->markups.xs[0], markup->markups.xs[1])) {
          return Eval(markup->markups.xs[2], env);
        }
        return Eval(markup->markups.xs[3], env);
      }

      FbldError(markup->text->loc, "unsupported command");
      return NULL;
    }

    case FBLD_MARKUP_SEQUENCE: {
      FbldMarkup* m = malloc(sizeof(FbldMarkup));
      m->tag = FBLD_MARKUP_SEQUENCE;
      m->text = NULL;
      FbldInitVector(m->markups);
      for (size_t i = 0; i < markup->markups.size; ++i) {
        FbldAppendToVector(m->markups, Eval(markup->markups.xs[i], env));
      }
      return m;
    }
  }

  return NULL;
}

FbldMarkup* FbldEval(FbldMarkup* markup)
{
  return Eval(markup, NULL);
}
