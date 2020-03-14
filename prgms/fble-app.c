// fble-app
//   A program to run fble programs with an App interface.

#include <assert.h>     // for assert
#include <SDL.h>        // for SDL_*
#include <string.h>     // for strcmp

#include "fble.h"

#define MAX_ROW 20
#define MAX_COL 60
#define TICK_INTERVAL 200

SDL_Window* gWindow = NULL;
SDL_Surface* gScreen = NULL;

// black, red, green, yellow, blue, magenta, cyan, white
static Uint32 DRAW_COLORS[8];

static void PrintUsage(FILE* stream);
static int ReadNat(FbleValue* num);
static bool IO(FbleIO* io, FbleValueArena* arena, bool block);
static Uint32 OnTimer(Uint32 interval, void* param);
int main(int argc, char* argv[]);

// PrintUsage --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//
// Result:
//   None.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream)
{
  fprintf(stream,
      "Usage: fble-app FILE DIR \n"
      "Execute the an app process described by the fble program FILE.\n"
      "Example: fble-app prgms/fble-Snake.fble prgms\n"
  );
}

// ReadNat --
//   Read a number from an FbleValue of type Nat@Nat
//
// Inputs:
//   x - the value of the number.
//
// Results:
//   The number x represented as an integer.
//
// Side effects:
//   None
static int ReadNat(FbleValue* x)
{
  switch (FbleUnionValueTag(x)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 2 * ReadNat(FbleUnionValueAccess(x));
    case 3: return 2 * ReadNat(FbleUnionValueAccess(x)) + 1;
    default: assert(false && "Invalid Nat tag"); abort();
  }
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  bool change = false;

  if (io->ports.xs[1] != NULL) {
    FbleValue* drawS = io->ports.xs[1];
    while (FbleUnionValueTag(drawS) == 0) {
      FbleValue* drawP = FbleUnionValueAccess(drawS);
      FbleValue* draw = FbleStructValueAccess(drawP, 0);
      drawS = FbleStructValueAccess(drawP, 1);

      SDL_Rect rect;
      rect.x = 100 + ReadNat(FbleStructValueAccess(draw, 0));
      rect.y = 100 + ReadNat(FbleStructValueAccess(draw, 1));
      rect.w = ReadNat(FbleStructValueAccess(draw, 2));
      rect.h = ReadNat(FbleStructValueAccess(draw, 3));
      FbleValue* color = FbleStructValueAccess(draw, 4);

      size_t color_index = FbleUnionValueTag(color);
      SDL_FillRect(gScreen, &rect, DRAW_COLORS[color_index]);
    }
    SDL_UpdateWindowSurface(gWindow);

    FbleValueRelease(arena, io->ports.xs[1]);
    io->ports.xs[1] = NULL;
    change = true;
  }

  if (block) {
    while (io->ports.xs[0] == NULL) {
      SDL_Event event;
      SDL_WaitEvent(&event);
      switch (event.type) {
        case SDL_KEYDOWN: {
          int c = -1;
          switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_K: c = 0; break;
            case SDL_SCANCODE_J: c = 1; break;
            case SDL_SCANCODE_H: c = 2; break;
            case SDL_SCANCODE_L: c = 3; break;
            case SDL_SCANCODE_Q: {
              SDL_DestroyWindow(gWindow);
              SDL_Quit();
            }
            default: break;
          }

          if (c >= 0) {
            FbleValueV args = { .size = 0, .xs = NULL };
            io->ports.xs[0] = FbleNewUnionValue(arena, 0, FbleNewUnionValue(arena, c, FbleNewStructValue(arena, args)));
            change = true;
          }
          break;
        }

        case SDL_USEREVENT: {
          FbleValueV args = { .size = 0, .xs = NULL };
          io->ports.xs[0] = FbleNewUnionValue(arena, 1, FbleNewStructValue(arena, args));
          change = true;
          break;
        }
      }
    }
  }

  return change;
}

// OnTimer --
//   Callback called for every timer tick.
//
// Inputs:
//   interval - the timer interval
//   param - unused
//
// Results:
//   The new timer interval.
//
// Side effects:
//   Pushes a user event onto the SDL event queue.
static Uint32 OnTimer(Uint32 interval, void* param)
{
  SDL_UserEvent user;
  user.type = SDL_USEREVENT;
  user.code = 0;
  user.data1 = NULL;
  user.data2 = NULL;

  SDL_Event event;
  event.type = SDL_USEREVENT;
  event.user = user;
  SDL_PushEvent(&event);
  return interval;
}

// main --
//   The main entry point for fble-app.
//
// Inputs:
//   argc - The number of command line arguments.
//   argv - The command line arguments.
//
// Results:
//   0 on success, non-zero on error.
//
// Side effects:
//   Performs IO based on the execution of FILE. Prints an error message to
//   standard error if an error is encountered.
int main(int argc, char* argv[])
{
  if (argc > 1 && strcmp("--help", argv[1]) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  if (argc <= 1) {
    fprintf(stderr, "no input file.\n");
    PrintUsage(stderr);
    return 1;
  }

  const char* path = argv[1];
  const char* include_path = argv[2];

  FbleArena* prgm_arena = FbleNewArena();
  FbleProgram* prgm = FbleLoad(prgm_arena, path, include_path);
  if (prgm == NULL) {
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleArena* eval_arena = FbleNewArena();
  FbleValueArena* value_arena = FbleNewValueArena(eval_arena);
  FbleNameV blocks;
  FbleProfile* profile = NULL;

  FbleValue* func = FbleEval(value_arena, prgm, &blocks, &profile);
  if (func == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeProfile(eval_arena, profile);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  FbleValue* input = FbleNewInputPortValue(value_arena, 0);
  FbleValue* output = FbleNewOutputPortValue(value_arena, 1);
  FbleValue* main1 = FbleApply(value_arena, func, input, profile);
  FbleValue* proc = FbleApply(value_arena, main1, output, profile);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, main1);
  FbleValueRelease(value_arena, input);
  FbleValueRelease(value_arena, output);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeProfile(eval_arena, profile);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    return 1;
  }

  gWindow = SDL_CreateWindow(
      "Fble App", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
      SDL_WINDOW_FULLSCREEN);
  gScreen = SDL_GetWindowSurface(gWindow);
  SDL_FillRect(gScreen, NULL, SDL_MapRGB(gScreen->format, 0, 0, 0));
  SDL_UpdateWindowSurface(gWindow);

  // Set up the colors.
  DRAW_COLORS[0] = SDL_MapRGB(gScreen->format, 0, 0, 0);
  DRAW_COLORS[1] = SDL_MapRGB(gScreen->format, 255, 0, 0);
  DRAW_COLORS[2] = SDL_MapRGB(gScreen->format, 0, 255, 0);
  DRAW_COLORS[3] = SDL_MapRGB(gScreen->format, 255, 255, 0);
  DRAW_COLORS[4] = SDL_MapRGB(gScreen->format, 0, 0, 255);
  DRAW_COLORS[5] = SDL_MapRGB(gScreen->format, 255, 0, 255);
  DRAW_COLORS[6] = SDL_MapRGB(gScreen->format, 0, 255, 255);
  DRAW_COLORS[7] = SDL_MapRGB(gScreen->format, 255, 255, 255);

  FbleValue* ports[2] = {NULL, NULL};
  FbleIO io = { .io = &IO, .ports = { .size = 2, .xs = ports} };

  SDL_TimerID timer = SDL_AddTimer(TICK_INTERVAL, OnTimer, NULL);

  FbleValue* value = FbleExec(value_arena, &io, proc, profile);

  FbleValueRelease(value_arena, proc);
  FbleValueRelease(value_arena, ports[0]);
  FbleValueRelease(value_arena, ports[1]);

  FbleValueRelease(value_arena, value);
  FbleDeleteValueArena(value_arena);
  FbleFreeBlockNames(eval_arena, &blocks);
  FbleFreeProfile(eval_arena, profile);
  FbleAssertEmptyArena(eval_arena);
  FbleDeleteArena(eval_arena);
  FbleDeleteArena(prgm_arena);

  // Wait for key down before quiting.
  while (true) {
    SDL_Event event;
    SDL_WaitEvent(&event);
    if (event.type == SDL_KEYDOWN) {
      break;
    }
  }

  SDL_RemoveTimer(timer);
  SDL_DestroyWindow(gWindow);
  SDL_Quit();
  return 0;
}
