
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
  char* positions[3][3] = {
    {"0000", "0001", "0010"},
    {"0011", "0100", "0101"},
    {"0110", "0111", "1000"}
  };

  if (input[0] == 'R') {
    // reset
    fprintf(fout, "10");
  } else if (input[0] == 'P') {
    // computer
    fprintf(fout, "01");
  } else if (input[0] >= 'A' && input[0] <= 'C'
          && input[1] >= '1' && input[1] <= '3') {
    // position
    fprintf(fout, "00");
    fprintf(fout, "%s", positions[input[0]-'A'][input[1]-'1']);
  } else {
    fprintf(stderr, "Invalid Input\n");
    return false;
  }
  fflush(fout);
  return true;
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: tictactoe FBLCBI tictactoe.fblc.bin <NewGame id>.\n");
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
    // Read, convert and output the current board and game status.
    printf("  1 2 3\n");
    for (int r = 0; r < 3; r++) {
      printf("%c", 'A'+r);
      for (int c = 0; c < 3; c++) {
        int bit = fgetc(fout);
        if (bit == '0') {
          printf(fgetc(fout) == '0' ? " X" : " O");
        } else if (bit == '1') {
          printf(fgetc(fout) == '0' ? " _" : " ?");
        } else {
          fprintf(stderr, "Unexpected bit: '%c'\n", bit);
          return 1;
        }
      }
      printf("\n");
    }

    if (fgetc(fout) == '0') {
      if (fgetc(fout) == '0') {
        printf("Player %c move:\n", fgetc(fout) == '0' ? 'X' : 'O');
      } else {
        printf("GAME OVER: Player %c wins\n", fgetc(fout) == '0' ? 'X' : 'O');
      }
    } else {
      if (fgetc(fout) == '0') {
        printf("GAME OVER: DRAW\n");
      } else {
        printf("???\n");
      }
    }

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
