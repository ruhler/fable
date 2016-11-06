
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
  argc--;
  argv++;
  bool isputs[argc];
  char* ids[argc];
  FILE* files[argc];
  for (int i = 0; i < argc; i++) {
    if (argv[i][0] != 'i' && argv[i][0] != 'o') {
      fprintf(stderr, "Invalid polarity specifier in '%s'.\n", argv[i]);
      return 1;
    }
    isputs[i] = (argv[i][0] == 'i');

    if (argv[i][1] != ':') {
      fprintf(stderr, "Missing ':' separator between polarity and id in '%s'.\n", argv[i]);
      return 1;
    }
    ids[i] = &argv[i][2];

    char* idend = strchr(ids[i], ':');
    if (idend == NULL) {
      fprintf(stderr, "Missing ':' separate between id and file in '%s'.\n", argv[i]);
      return 1;
    }

    *idend = '\0';
    char* filename = idend + 1;
    files[i] = fopen(filename, isputs[i] ? "w" : "r");
    if (files[i] == NULL) {
      perror("fopen");
      return 1;
    }
  }

  char* line = NULL;
  size_t len = 0;
  int read = 0;
  while ((read = getline(&line, &len, stdin)) != -1) {
    printf("READ LINE: '%s'\n", line);
    char* cmd = line;
    char* ptr = strchr(cmd, ' ');
    if (ptr == NULL) {
      fprintf(stderr, "malformed command line: '%s'\n", line);
      return 1;
    }
    *ptr = '\0';
    char* id = ptr+1;

    ptr = strchr(id, ' ');
    if (ptr == NULL) {
      fprintf(stderr, "malformed command line: '%s'\n", line);
      return 1;
    }
    *ptr = '\0';
    char* text = ptr+1;
    size_t count = strlen(text);

    int i;
    for (i = 0; i < argc; i++) {
      if (strcmp(id, ids[i]) == 0) {
        break;
      }
    }

    if (i == argc) {
      fprintf(stderr, "No such port: '%s'\n", id);
      return 1;
    }

    if (isputs[i]) {
      if (strcmp(cmd, "put") != 0) {
        fprintf(stderr, "Expected 'put' command for port '%s', but got '%s'.\n", ids[i], cmd);
        return 1;
      }
      if (fwrite(text, sizeof(char), count, files[i]) < count) {
        fprintf(stderr, "Failed to write text to '%s'\n", ids[i]);
        return 1;
      }
    } else {
      if (strcmp(cmd, "get") != 0) {
        fprintf(stderr, "Expected 'get' command for port '%s', but got '%s'.\n", ids[i], cmd);
        return 1;
      }
      char input[count+1];
      if (fgets(input, count+1, files[i]) == NULL) {
        fprintf(stderr, "Error reading from '%s'\n", ids[i]);
        return 1;
      }
      if (strcmp(text, input) != 0) {
        fprintf(stderr, "Unexpected get on '%s'.\n", ids[i]);
        fprintf(stderr, "  expected: '%s'\n", text);
        fprintf(stderr, "  actual  : '%s'\n", input);
        return 1;
      } else {
        printf("GOT EXPECTED VALUE \n");
      }
    }
  }

  for (int i = 0; i < argc; i++) {
    fclose(files[i]);
  }
  free(line);
  return 0;
}

