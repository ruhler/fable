// fble-app
//   A program to run fble programs with an App interface.

#include <assert.h>     // for assert
#include <string.h>     // for strcmp

#include <SDL.h>        // for SDL_*

#include "fble.h"

SDL_Window* gWindow = NULL;
SDL_Surface* gScreen = NULL;

typedef enum {
  BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
} Color;

// Indexed by Color
static Uint32 SDL_COLORS[8];

static void PrintUsage(FILE* stream);
static int ReadIntP(FbleValue* num);
static int ReadInt(FbleValue* num);
static void FillPath(size_t n, SDL_Point* points, Uint32 color);
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

// FillPath --
//   Fill a closed path with a solid color to the window.
//
// Inputs:
//   n - the number of points in the path.
//   points - the points, in order around the path.
//   color - the color to fill the path with.
//
// Results:
//   None.
//
// Side effects:
//   Draws a filled path as described by the given points to the global
//   window. The user is responsible for calling SDL_UpdateWindowSurface when
//   they are ready to display the filled path.
static void FillPath(size_t n, SDL_Point* points, Uint32 color)
{
  SDL_Surface* s = SDL_GetWindowSurface(gWindow);

  assert(n > 0);

  SDL_Rect bounds;
  SDL_EnclosePoints(points, n, NULL, &bounds);

  for (int y = bounds.y; y < bounds.y + bounds.h; ++y) {
    // xs is a sorted list of the x coordinates of line segments in the path
    // that intersect the horizontal line defined by y.
    // xn is the number of valid points in the xs array.
    int xs[n];
    size_t xn = 0;
    for (size_t i = 0; i < n; ++i) {
      SDL_Point* s = points + i;
      SDL_Point* e = points + ((i+1) % n);
      if ((s->y <= y && e->y > y) || (s->y > y && e->y <= y)) {
        int x = s->x + (y - s->y)*(e->x - s->x)/(e->y - s->y);

        // Insert x sorted into the array of xs gathered so far.
        for (size_t j = 0; j < xn; ++j) {
          if (x < xs[j]) {
            int tmp = xs[j];
            xs[j] = x;
            x = tmp;
          }
        }
        xs[xn++] = x;
      }
    }

    // For a closed path, by mean value theorem, there should be an even
    // number of line segments intersecting with this horizontal y.
    assert(xn % 2 == 0);

    // Fill with the even odd rule: draw a ray from the point to infinity in
    // any direction, the point is considered inside the path if the number of
    // line segments the ray crosses is odd. We pick a ray going in the
    // horizontal direction. All points between x[0] and x[1] should be
    // filled, between x[2] and x[3], and so on.
    for (size_t i = 0; i + 1 < xn; i += 2) {
      SDL_Rect r = { xs[i], y, xs[i+1]-xs[i], 1 };
      SDL_FillRect(s, &r, color);
    }
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
      // Quad.
      FbleValue* rv = FbleUnionValueAccess(drawing);

      FbleValue* a = FbleStructValueAccess(rv, 0);
      FbleValue* b = FbleStructValueAccess(rv, 1);
      FbleValue* c = FbleStructValueAccess(rv, 2);
      FbleValue* d = FbleStructValueAccess(rv, 3);

      SDL_Point points[4];
      points[0].x = ReadInt(FbleStructValueAccess(a, 0));
      points[0].y = ReadInt(FbleStructValueAccess(a, 1));
      points[1].x = ReadInt(FbleStructValueAccess(b, 0));
      points[1].y = ReadInt(FbleStructValueAccess(b, 1));
      points[2].x = ReadInt(FbleStructValueAccess(c, 0));
      points[2].y = ReadInt(FbleStructValueAccess(c, 1));
      points[3].x = ReadInt(FbleStructValueAccess(d, 0));
      points[3].y = ReadInt(FbleStructValueAccess(d, 1));

      FbleValue* color = FbleStructValueAccess(rv, 4);
      size_t color_index = FbleUnionValueTag(color);
      FillPath(4, points, SDL_COLORS[color_index]);
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
        SDL_UpdateWindowSurface(gWindow);
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
      SDL_WINDOW_FULLSCREEN);
  SDL_ShowCursor(SDL_DISABLE);

  gScreen = SDL_GetWindowSurface(gWindow);
  SDL_COLORS[BLACK] = SDL_MapRGB(gScreen->format, 0, 0, 0);
  SDL_COLORS[RED] = SDL_MapRGB(gScreen->format, 255, 0, 0);
  SDL_COLORS[GREEN] = SDL_MapRGB(gScreen->format, 0, 255, 0);
  SDL_COLORS[YELLOW] = SDL_MapRGB(gScreen->format, 255, 255, 0);
  SDL_COLORS[BLUE] = SDL_MapRGB(gScreen->format, 0, 0, 255);
  SDL_COLORS[MAGENTA] = SDL_MapRGB(gScreen->format, 255, 0, 255);
  SDL_COLORS[CYAN] = SDL_MapRGB(gScreen->format, 0, 255, 255);
  SDL_COLORS[WHITE] = SDL_MapRGB(gScreen->format, 255, 255, 255);

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

  SDL_FillRect(gScreen, NULL, SDL_MapRGB(gScreen->format, 0, 0, 0));

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

  SDL_DestroyWindow(gWindow);
  SDL_Quit();
  return 0;
}
