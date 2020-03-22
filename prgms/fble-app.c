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
static int ReadIntP(FbleValue* num);
static int ReadInt(FbleValue* num);
static void Draw(FbleValue* drawing);
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

// ReadIntP --
//   Read a number from an FbleValue of type /Int/IntP%.IntP@.
//
// Inputs:
//   x - the value of the number.
//
// Results:
//   The number x represented as an integer.
//
// Side effects:
//   None
static int ReadIntP(FbleValue* x)
{
  switch (FbleUnionValueTag(x)) {
    case 0: return 1;
    case 1: return 2 * ReadIntP(FbleUnionValueAccess(x));
    case 2: return 2 * ReadIntP(FbleUnionValueAccess(x)) + 1;
    default: assert(false && "Invalid IntP@ tag"); abort();
  }
}

// ReadInt --
//   Read a number from an FbleValue of type /Int/Int%.Int@.
//
// Inputs:
//   x - the value of the number.
//
// Results:
//   The number x represented as an integer.
//
// Side effects:
//   None
static int ReadInt(FbleValue* x)
{
  switch (FbleUnionValueTag(x)) {
    case 0: return -ReadIntP(FbleUnionValueAccess(x));
    case 1: return 0;
    case 2: return ReadIntP(FbleUnionValueAccess(x));
    default: assert(false && "Invalid Int@ tag"); abort();
  }
}

// Draw --
//   Draw a drawing to the screen of type /Drawing%.Drawing@.
//
// Inputs:
//   drawing - the drawing to draw
//
// Results:
//   none.
//
// Side effects:
//   Draws the drawing to the gScreen. The caller must call
//   SDL_UpdateWindowSurface for the screen to actually be updated.
static void Draw(FbleValue* drawing)
{
  switch (FbleUnionValueTag(drawing)) {
    case 0: {
      // Blank. Do nothing.
      return;
    }

    case 1: {
      // Rectangle.
      SDL_Rect rect;
      FbleValue* rv = FbleUnionValueAccess(drawing);

      rect.x = 100 + ReadInt(FbleStructValueAccess(rv, 0));
      rect.y = 100 + ReadInt(FbleStructValueAccess(rv, 1));
      rect.w = ReadInt(FbleStructValueAccess(rv, 2));
      rect.h = ReadInt(FbleStructValueAccess(rv, 3));

      FbleValue* color = FbleStructValueAccess(rv, 4);
      size_t color_index = FbleUnionValueTag(color);

      SDL_FillRect(gScreen, &rect, DRAW_COLORS[color_index]);
      return;
    }

    case 2: {
      // Over.
      FbleValue* over = FbleUnionValueAccess(drawing);
      Draw(FbleStructValueAccess(over, 0));
      Draw(FbleStructValueAccess(over, 1));
      return;
    }

    default: {
      assert(false && "Invalid Drawing@ tag");
      abort();
    }
  }
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  bool change = false;

  if (io->ports.xs[1] != NULL) {
    Draw(io->ports.xs[1]);
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
          int k = -1;
          switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_H: k = 0; break;
            case SDL_SCANCODE_J: k = 1; break;
            case SDL_SCANCODE_K: k = 2; break;
            case SDL_SCANCODE_L: k = 3; break;
            case SDL_SCANCODE_Q: k = 4; break;
            default: break;
          }

          if (k >= 0) {
            FbleValueV args = { .size = 0, .xs = NULL };
            io->ports.xs[0] = FbleNewUnionValue(arena, 1, FbleNewUnionValue(arena, k, FbleNewStructValue(arena, args)));
            change = true;
          }
          break;
        }

        case SDL_USEREVENT: {
          FbleValueV args = { .size = 0, .xs = NULL };
          io->ports.xs[0] = FbleNewUnionValue(arena, 0, FbleNewStructValue(arena, args));
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

  FbleValue* args[2];
  args[0] = FbleNewInputPortValue(value_arena, 0);
  args[1] = FbleNewOutputPortValue(value_arena, 1);
  FbleValueV argsv = { .xs = args, .size = 2 };
  FbleValue* proc = FbleApply(value_arena, func, argsv, profile);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, args[0]);
  FbleValueRelease(value_arena, args[1]);

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

  SDL_RemoveTimer(timer);
  SDL_DestroyWindow(gWindow);
  SDL_Quit();
  return 0;
}
