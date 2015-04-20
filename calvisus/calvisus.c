
void usage(FILE* fout) {
  fprintf(fout, "calvisus [--main func] FILE\n");
}

int run(FILE* fin, const char* main) {
  env_t* env = parse(fin);
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
  print(value);
  fprintf("\n");
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
  FILE* fin = fopen(file, "r");
  if (fin == NULL) {
    fprintf(stderr, "Unable to open input file %s\n", file);
    return 1;
  }

  return run(fin, main);
}

