
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Given a line of input from the user, convert it to a tictactoe Input and
// write that to fout.
// Writes nothing to fout if the input is not valid.
static bool ConvertInput(const char* input, FILE* fout)
{
  char rows[] = "UML";
  char cols[] = "LCR";
  if (input[0] == 'R') {
    fprintf(fout, "Input:reset(Unit())\n");
  } else if (input[0] == 'P') {
    fprintf(fout, "Input:computer(Unit())\n");
  } else if (input[0] >= 'A' && input[0] <= 'C'
          && input[1] >= '1' && input[1] <= '3') {
    fprintf(fout, "Input:position(Position:%c%c(Unit()))\n",
        rows[input[0] - 'A'], cols[input[1]-'1']);
  } else {
    fprintf(stderr, "Invalid Input\n");
    return false;
  }
  fflush(fout);
  return true;
}

// Given a text representation of the tictactoe Output, render that output to
// fout. Assumes the Output text is well formed.
static void ConvertOutput(const char* output, FILE* fout)
{
  const char* ptr = output;
  fprintf(fout, "  1 2 3\n");
  for (int r = 0; r < 3; r++) {
    fprintf(fout, "%c", 'A'+r);
    for (int c = 0; c < 3; c++) {
      ptr = strstr(ptr, "Square:");
      char c = ptr == NULL ? '?' : ptr[7]; 
      switch (c) {
        case 'X': fprintf(fout, " X"); break;
        case 'O': fprintf(fout, " O"); break;
        case 'E': fprintf(fout, " _"); break;
      }
      ptr++;
    }
    fprintf(fout, "\n");
  }

  const char* move = strstr(ptr, "GameStatus:Move(Player:");
  if (move != NULL) {
    fprintf(fout, "Player %c move:\n", move[23]);
    return;
  }

  const char* win = strstr(ptr, "GameStatus:Win(Player:");
  if (win != NULL) {
    fprintf(fout, "GAME OVER: Player %c wins\n", win[22]);
    return;
  }

  if (strstr(ptr, "GameStatus:Draw") != NULL) {
    fprintf(fout, "GAME OVER: DRAW\n");
    return;
  }

  fprintf(fout, "???\n");
  fflush(fout);
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: tictactoe FBLC tictactoe.fblc NewGame.\n");
    return 1;
  }
  argv++;

  // Reserve fds 3 and 4 for the child by duping an arbitrary fd to them.
  if (dup(0) != 3 || dup(1) != 4) {
    fprintf(stderr, "Unable to reserve fds 3 and 4\n");
    return 1;
  }

  // Set up input port: parent_in -> child fd 3
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("input pipe");
    return 1;
  }
  if (dup2(pipefd[0], 3) < 0) {
    perror("dup2 3");
    return 1;
  }
  close(pipefd[0]);
  int parent_in = pipefd[1];

  // Set up output port: child fd 4 -> parent_out
  if (pipe(pipefd) < 0) {
    perror("output pipe");
    return 1;
  }
  if (dup2(pipefd[1], 4) < 0) {
    perror("dup2 4");
    return 1;
  }
  close(pipefd[1]);
  int parent_out = pipefd[0];

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    // Child. Close the parent fds and exec.
    close(parent_in);
    close(parent_out);
    execvp(argv[0], argv);
    perror("execvp");
    return 1;
  }

  // Parent. Close the child fds and run the program.
  close(3);
  close(4);

  FILE* fin = fdopen(parent_in, "w");
  FILE* fout = fdopen(parent_out, "r");
  char* line = NULL;
  size_t len = 0;
  int read = 0;
  while (true) {
    // Output the current status
    if ((read = getline(&line, &len, fout)) == -1) {
      break;
    }
    ConvertOutput(line, stdout);

    // Get the next move.
    while ((read = getline(&line, &len, stdin)) != -1
        && !ConvertInput(line, fin)) {
      ;
    }
    if (read == -1) {
      break;
    }
  }

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
