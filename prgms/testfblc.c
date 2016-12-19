
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void PrintUsage(FILE* stream)
{
  fprintf(stream,
      "Usage: testfblc PORTSPEC SCRIPT command\n"
      "Test fblc interpreter invoked using command.\n"
      "PORTSPEC should be a comma separated list of elements of the form:\n"
      "      i:NAME     for input ports\n"
      "and   o:NAME     for output ports\n"
      "SCRIPT should be a file containing a sequence of commands of the form:\n"
      "      put NAME VALUE\n"
      "or    get NAME VALUE\n"
      "The put command puts the fblc text VALUE onto the named port.\n"
      "The get command reads the fblc value from the named port and asserts\n"
      "that the value read matches the given value.\n" 
  );
}

int main(int argc, char* argv[])
{
  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  if (argc < 4) {
    PrintUsage(stderr);
    return 1;
  }

  char* portspec = argv[1];
  const char* script = argv[2];
  char** cmd_argv = argv+3;

  // Count the number of ports.
  int portc = 1;
  for (const char* ptr = portspec; *ptr != '\0'; ptr++) {
    if (*ptr == ',') {
      portc++;
    }
  }

  // Prepare the ports based on the port spec.
  bool isputs[portc];
  char* ids[portc];
  FILE* files[portc];
  char* ptr = portspec;
  for (int i = 0; ptr != NULL; i++) {
    if (*ptr != 'i' && *ptr != 'o') {
      fprintf(stderr, "Invalid polarity specifier in '%s'.\n", ptr);
      return 1;
    }
    isputs[i] = (*ptr == 'i');

    if (ptr[1] != ':') {
      fprintf(stderr, "Missing ':' separator between polarity and id in '%s'.\n", ptr);
      return 1;
    }
    ids[i] = &ptr[2];

    ptr = strchr(ids[i], ',');
    if (ptr != NULL) {
      *ptr = '\0';
      ptr++;
    }

    int pipefd[2];
    if (pipe(pipefd) < 0) {
      perror("pipe");
      return 1;
    }

    // Child fds range from 3 to (2+portc).
    // Parent fds range from (4+portc) to (5+2*portc).
    // The fd 3+portc is left unused to ensure we never have to swap two fds
    // in place.
    int child_fd = isputs[i] ? pipefd[0] : pipefd[1];
    int parent_fd = isputs[i] ? pipefd[1] : pipefd[0];
    int child_target_fd = 3 + i;
    int parent_target_fd = 4 + portc + i;

    if (dup2(parent_fd, parent_target_fd) < 0) {
      perror("dup2 parent");
      return 1;
    }
    if (parent_fd != parent_target_fd) {
      close(parent_fd);
    }

    if (dup2(child_fd, child_target_fd) < 0) {
      perror("dup2 child");
      return 1;
    }
    if (child_fd != child_target_fd) {
      close(child_fd);
    }

    files[i] = fdopen(parent_target_fd, isputs[i] ? "w" : "r");
    if (files[i] == NULL) {
      perror("fdopen");
      return 1;
    }
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    // Child. Close the parent fds and exec.
    for (int i = 0; i < portc; i++) {
      close(5 + portc + i);
    }
    execvp(cmd_argv[0], cmd_argv);
    perror("execvp");
    return 1;
  }

  // Parent. Close the child fds and run the script.
  for (int i = 0; i < portc; i++) {
    close(3 + i);
  }

  FILE* fin = fopen(script, "r");
  if (fin == NULL) {
    perror("fopen");
    return 1;
  }

  char* line = NULL;
  size_t len = 0;
  int read = 0;
  while ((read = getline(&line, &len, fin)) != -1) {
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
    if (text[count-1] == '\n') {
      text[count-1] = '\0';
      count--;
    }

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
      fflush(files[i]);
    } else {
      if (strcmp(cmd, "get") != 0) {
        fprintf(stderr, "Expected 'get' command for port '%s', but got '%s'.\n", ids[i], cmd);
        return 1;
      }
      char input[count];
      if (fread(input, sizeof(char), count, files[i]) != count) {
        fprintf(stderr, "Error reading from '%s'\n", ids[i]);
        return 1;
      }
      if (strcmp(text, input) != 0) {
        fprintf(stderr, "Unexpected get on '%s'.\n", ids[i]);
        fprintf(stderr, "  expected: '%s'\n", text);
        fprintf(stderr, "  actual  : '%s'\n", input);
        return 1;
      }
    }
  }

  for (int i = 0; i < argc; i++) {
    // fclose(files[i]);
  }
  free(line);

  int status;
  if (wait(&status) < 0) {
    perror("wait");
    return 1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  fprintf(stderr, "child terminated abnormally\n");
  return 1;
}

