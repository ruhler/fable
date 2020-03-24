// fble-app
//   A program to run fble programs with an App interface.

#include <assert.h>     // for assert
#include <string.h>     // for strcmp

#include <GL/gl.h>      // for gl*
#include <SDL.h>        // for SDL_*

#include "fble.h"

SDL_Window* gWindow = NULL;
SDL_Surface* gScreen = NULL;

typedef enum {
  BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
} Color;

static void PrintUsage(FILE* stream);
static int ReadIntP(FbleValue* num);
static int ReadInt(FbleValue* num);
static void Draw(FbleValue* drawing);
static FbleValue* MakeIntP(FbleValueArena* arena, int x);
static FbleValue* MakeInt(FbleValueArena* arena, int x);
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
//   SDL_GL_SwapWindow for the screen to actually be updated.
static void Draw(FbleValue* drawing)
{
  switch (FbleUnionValueTag(drawing)) {
    case 0: {
      // Blank. Do nothing.
      return;
    }

    case 1: {
      // Rectangle.
      FbleValue* rv = FbleUnionValueAccess(drawing);

      int x = ReadInt(FbleStructValueAccess(rv, 0));
      int y = ReadInt(FbleStructValueAccess(rv, 1));
      int w = ReadInt(FbleStructValueAccess(rv, 2));
      int h = ReadInt(FbleStructValueAccess(rv, 3));

      FbleValue* color = FbleStructValueAccess(rv, 4);
      size_t color_index = FbleUnionValueTag(color);
      switch (color_index) {
        case BLACK: glColor3d(0, 0, 0); break;
        case RED: glColor3d(1, 0, 0); break;
        case GREEN: glColor3d(0, 1, 0); break;
        case YELLOW: glColor3d(1, 1, 0); break;
        case BLUE: glColor3d(0, 0, 1); break;
        case MAGENTA: glColor3d(1, 0, 1); break;
        case CYAN: glColor3d(0, 1, 1); break;
        case WHITE: glColor3d(1, 1, 1); break;
      }

      glBegin(GL_QUADS);
        glVertex3i(x, y, 0);
        glVertex3i(x + w, y, 0);
        glVertex3i(x + w, y + h, 0);
        glVertex3i(x, y + h, 0);
      glEnd();
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

// MakeIntP -- 
//   Make an FbleValue of type /Int/IntP%.IntP@ for the given integer.
//
// Inputs:
//   arena - the arena to use for allocations.
//   x - the integer value. Must be greater than 0.
//
// Results:
//   An FbleValue for the integer.
//
// Side effects:
//   Allocates a value that should be freed with FbleValueRelease when no
//   longer needed. Behavior is undefined if x is not positive.
static FbleValue* MakeIntP(FbleValueArena* arena, int x)
{
  assert(x > 0);
  if (x == 1) {
    FbleValueV args = { .size = 0, .xs = NULL };
    return FbleNewUnionValue(arena, 0, FbleNewStructValue(arena, args));
  }

  return FbleNewUnionValue(arena, 1 + (x % 2), MakeIntP(arena, x / 2));
}

// MakeInt -- 
//   Make an FbleValue of type /Int/Int%.Int@ for the given integer.
//
// Inputs:
//   arena - the arena to use for allocations.
//   x - the integer value.
//
// Results:
//   An FbleValue for the integer.
//
// Side effects:
//   Allocates a value that should be freed with FbleValueRelease when no
//   longer needed.
static FbleValue* MakeInt(FbleValueArena* arena, int x)
{
  if (x < 0) {
    return FbleNewUnionValue(arena, 0, MakeIntP(arena, -x));
  }

  if (x == 0) {
    FbleValueV args = { .size = 0, .xs = NULL };
    return FbleNewUnionValue(arena, 1, FbleNewStructValue(arena, args));
  }

  return FbleNewUnionValue(arena, 2, MakeIntP(arena, x));
}

// IO --
//   io function for external ports.
//   See the corresponding documentation in fble.h.
static bool IO(FbleIO* io, FbleValueArena* arena, bool block)
{
  bool change = false;

  if (io->ports.xs[1] != NULL) {
    FbleValue* effect = io->ports.xs[1];
    switch (FbleUnionValueTag(effect)) {
      case 0: {
        int tick = ReadInt(FbleUnionValueAccess(effect));

        // TODO: Time should be relative to when the last tick was delivered,
        // not to the current time.
        SDL_AddTimer(tick, OnTimer, NULL);
        break;
      }

      case 1: {
        Draw(FbleUnionValueAccess(effect));
        SDL_GL_SwapWindow(gWindow);
        break;
      }
    }

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
//   0 to cancel to the timer.
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
  return 0;
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

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeProfile(eval_arena, profile);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    return 1;
  }

  gWindow = SDL_CreateWindow(
      "Fble App", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
      SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);

  int width = 0;
  int height = 0;
  SDL_GetWindowSize(gWindow, &width, &height);

  FbleValue* args[4];
  args[0] = MakeInt(value_arena, width);
  args[1] = MakeInt(value_arena, height);
  args[2] = FbleNewInputPortValue(value_arena, 0);
  args[3] = FbleNewOutputPortValue(value_arena, 1);
  FbleValueV argsv = { .xs = args, .size = 4 };
  FbleValue* proc = FbleApply(value_arena, func, argsv, profile);
  FbleValueRelease(value_arena, func);
  FbleValueRelease(value_arena, args[0]);
  FbleValueRelease(value_arena, args[1]);
  FbleValueRelease(value_arena, args[2]);
  FbleValueRelease(value_arena, args[3]);

  if (proc == NULL) {
    FbleDeleteValueArena(value_arena);
    FbleFreeBlockNames(eval_arena, &blocks);
    FbleFreeProfile(eval_arena, profile);
    FbleDeleteArena(eval_arena);
    FbleDeleteArena(prgm_arena);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
    return 1;
  }

  SDL_GLContext gl = SDL_GL_CreateContext(gWindow);
  SDL_ShowCursor(SDL_DISABLE);
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  SDL_GL_SwapWindow(gWindow);

  FbleValue* ports[2] = {NULL, NULL};
  FbleIO io = { .io = &IO, .ports = { .size = 2, .xs = ports} };

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

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(gWindow);
  SDL_Quit();
  return 0;
}
