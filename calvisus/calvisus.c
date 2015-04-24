
#include <stdio.h>
#include <string.h>

#include <gc/gc.h>

#include "env.h"
#include "eval.h"
#include "parser.h"
#include "toker.h"
#include "value.h"

void usage(FILE* fout) {
  fprintf(fout, "calvisus [--main func] FILE\n");
}

int run(const char* filename, const char* main) {
  toker_t* toker = toker_open(filename);
  if (toker == NULL) {
    fprintf(stderr, "Failed to open input stream.\n");
    return 1;
  }

  env_t* env = parse(toker);
  if (env == NULL) {
    fprintf(stderr, "Parse Error\n");
    return 1;
  }

  func_t* func = lookup_func(env, main);
  if (func == NULL) {
    fprintf(stderr, "Failed to find main function %s\n", main);
    return 1;
  }

  if (func->num_args != 0) {
    fprintf(stderr, "Main function does not have 0 arguments.\n");
    return 1;
  }

  value_t* value = eval(env, NULL, func->body);
  print(stdout, value);
  printf("\n");
  return 0;
}

int main(int argc, char* argv[]) {
  const char* main = "main";
  const char* file = "/dev/null/stdin";

  if (argc < 2) {
    usage(stderr);
    return 1;
  }

  for (int i = 1; i < argc; i++) {
    if (strcmp("--help", argv[i]) == 0) {
      usage(stdout);
      return 0;
    } else if (i+1 < argc && strcmp("--main", argv[i]) == 0) {
      main = argv[i+1];
      i++;
    } else {
      file = argv[i];
    }
  }

  GC_INIT();
  return run(file, main);
}

