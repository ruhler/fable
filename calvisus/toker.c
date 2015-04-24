
#include "toker.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <gc/gc.h>

static bool isname(int c) {
  return isalnum(c) || c == '_';
}

static int toker_getc(toker_t* toker) {
  int c = fgetc(toker->fin);
  if (c == '\n') {
    toker->line++;
    toker->ncol = 0;
  } else if (c != EOF) {
    toker->ncol++;
  }
  return c;
}

static void toker_ungetc(toker_t* toker, int c) {
  toker->ncol--;
  ungetc(c == '\n' ? ' ' : c, toker->fin);
}


static void read_next(toker_t* toker) {
  int c;
  do {
    c = toker_getc(toker);
  } while (isspace(c));
  toker->col = toker->ncol;

  if (c == EOF) {
    toker->type = TOK_EOF;
    toker->name = NULL;
    return;
  }

  if (isname(c)) {
    char buf[BUFSIZ];
    size_t n;
    buf[0] = c;
    c = toker_getc(toker);
    for (n = 1; isname(c); n++) {
      assert(n < BUFSIZ && "TODO: Support longer names.");
      buf[n] = c;
      c = toker_getc(toker);
    }
    toker_ungetc(toker, c);
    toker->type = TOK_NAME;
    char* name = GC_MALLOC((n+1) * sizeof(char));
    for (int i = 0; i < n; i++) {
      name[i] = buf[i];
    }
    name[n] = '\0';
    toker->name = name;
    return;
  }

  toker->type = c;
  toker->name = NULL;
}

toker_t* toker_open(const char* filename) {
  toker_t* toker = GC_MALLOC(sizeof(toker_t));
  toker->fin = fopen(filename, "r");
  if (toker->fin == NULL) {
    return NULL;
  }
  toker->filename = filename;
  toker->line = 1;
  toker->col = 0;
  toker->ncol = 0;
  read_next(toker);
  return toker;
}

void toker_close(toker_t* toker) {
  fclose(toker->fin);
}

const char* toker_get_name(toker_t* toker, const char* expected) {
  if (toker->type == TOK_NAME) {
    const char* name = toker->name;
    read_next(toker);
    return name;
  }
  fprintf(stderr, "%s:%d:%d: error: Expected %s, but got token of type '%c'\n",
      toker->filename, toker->line, toker->col, expected, toker->type);
  return NULL;
}

bool toker_get(toker_t* toker, int type) {
  if (toker->type == type) {
    read_next(toker);
    return true;
  }
  fprintf(stderr, "%s:%d:%d: error: Expected %c, but got token of type '%c'\n",
      toker->filename, toker->line, toker->col, type, toker->type);
  return false;
}

bool toker_is(toker_t* toker, int type) {
  return toker->type == type;
}

void toker_unexpected(toker_t* toker, const char* expected) {
  fprintf(stderr, "%s:%d:%d: error: Expected %s, but got token of type '%c'\n",
      toker->filename, toker->line, toker->col, expected, toker->type);
}

