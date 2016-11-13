
#define _GNU_SOURCE
#include <stdio.h>

#include "FblcInternal.h"

typedef struct {
  FblcEnv* env;
  FblcType* type;
} InputData;

static FblcValue* Input(InputData* user, FblcValue* value);
static FblcValue* Output(void* ignored, FblcValue* value);

static FblcValue* Input(InputData* user, FblcValue* value)
{
  char* line = NULL;
  size_t len = 0;
  int read = 0;
  while ((read = getline(&line, &len, stdin)) != -1) {
    const char* input_text = NULL;
    if (line[0] == 'R') {
      input_text = "Input:reset(Unit())";
    } else if (line[0] == 'P') {
      input_text = "Input:computer(Unit())";
    } else if (line[0] == 'A' && line[1] == '1') {
      input_text = "Input:position(Position:UL(Unit()))";
    } else if (line[0] == 'A' && line[1] == '2') {
      input_text = "Input:position(Position:UC(Unit()))";
    } else if (line[0] == 'A' && line[1] == '3') {
      input_text = "Input:position(Position:UR(Unit()))";
    } else if (line[0] == 'B' && line[1] == '1') {
      input_text = "Input:position(Position:ML(Unit()))";
    } else if (line[0] == 'B' && line[1] == '2') {
      input_text = "Input:position(Position:MC(Unit()))";
    } else if (line[0] == 'B' && line[1] == '3') {
      input_text = "Input:position(Position:MR(Unit()))";
    } else if (line[0] == 'C' && line[1] == '1') {
      input_text = "Input:position(Position:LL(Unit()))";
    } else if (line[0] == 'C' && line[1] == '2') {
      input_text = "Input:position(Position:LC(Unit()))";
    } else if (line[0] == 'C' && line[1] == '3') {
      input_text = "Input:position(Position:LR(Unit()))";
    }

    if (input_text != NULL) {
      FblcTokenStream toks;
      FblcOpenStringTokenStream(&toks, "input", input_text);
      FblcValue* input = FblcParseValue(user->env, user->type, &toks);
      FblcCloseTokenStream(&toks);
      return input;
    }
  }

  free(line);
  return NULL;
}

static FblcValue* Output(void* ignored, FblcValue* value)
{
  FblcStructValue* output = (FblcStructValue*)value;
  FblcStructValue* board = (FblcStructValue*)output->fieldv[0];
  printf("  1 2 3\n");
  for (int r = 0; r < 3; r++) {
    printf("%c", 'A'+r);
    for (int c = 0; c < 3; c++) {
      FblcUnionValue* square = (FblcUnionValue*)board->fieldv[r*3 + c];
      switch (square->tag) {
        case 0: printf(" X"); break;
        case 1: printf(" O"); break;
        case 2: printf(" _"); break;
      }
    }
    printf("\n");
  }

  FblcUnionValue* status = (FblcUnionValue*)output->fieldv[1];
  switch (status->tag) {
    case 0: {
      FblcUnionValue* player = (FblcUnionValue*)status->field;
      printf("Player %c move:\n", player->tag ? 'O' : 'X');
      break;
    }

    case 1: {
      FblcUnionValue* player = (FblcUnionValue*)status->field;
      printf("GAME OVER: Player %c wins\n", player->tag ? 'O' : 'X');
      break;
    }

    case 2:
      printf("GAME OVER: DRAW\n");
      break;
  }

  return NULL;
}

int main(int argc, char* argv[])
{
  MALLOC_INIT();

  if (argc != 2) {
    fprintf(stderr, "no input file.\n");
    return 1;
  }

  const char* filename = argv[1];

  FblcTokenStream toks;
  if (!FblcOpenFileTokenStream(&toks, filename)) {
    fprintf(stderr, "failed to open input FILE %s.\n", filename);
    return 1;
  }

  FblcAllocator alloc;
  FblcInitAllocator(&alloc);
  FblcEnv* env = FblcParseProgram(&alloc, &toks);
  FblcCloseTokenStream(&toks);
  if (env == NULL) {
    fprintf(stderr, "failed to parse input FILE.\n");
    FblcFreeAll(&alloc);
    return 1;
  }

  if (!FblcCheckProgram(env)) {
    fprintf(stderr, "input FILE is not a well formed Fblc program.\n");
    FblcFreeAll(&alloc);
    return 1;
  }

  const char* entry = "NewGame";
  FblcProc* proc = FblcLookupProc(env, entry);
  if (proc == NULL) {
    FblcFreeAll(&alloc);
    fprintf(stderr, "failed to find process '%s'.\n", entry);
    return 1;
  }

  InputData input_data;
  input_data.env = env; 
  input_data.type = FblcLookupType(env, "Input");

  FblcIO ios[2];
  ios[0].io = (FblcIOFunction)&Input;
  ios[0].user = &input_data;
  ios[1].io = (FblcIOFunction)&Output;
  ios[1].user = NULL;

  FblcExecute(env, proc, ios, NULL);
  FblcFreeAll(&alloc);
  return 0;
}
