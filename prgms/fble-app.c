// fble-app
//   A program to run fble programs with an App interface.

#include <assert.h>     // for assert
#include <string.h>     // for strcmp

#include <SDL.h>        // for SDL_*

#include "fble-main.h"    // for FbleMain.
#include "fble-value.h"   // for FbleValue, etc.

// sFpsHistogram[i] is the number of samples with i frames per second.
// Anything above 60 FPS is counted towards i = 60.
static int sFpsHistogram[61] = {0};

// AppIO - Implementation of FbleIO for App, with some additional parameters
// to pass to the IO function.
//
// Fields:
//   _base - The underlying FbleIO object.
//   window - The window to draw to.
//   format - The pixel format of the screen, for getting color values.
//   time - The current simulation time in units of SDL_GetTicks.
//
//   event - The event input port.
//   effect - The effect output port.
typedef struct {
  FbleIO _base;
  SDL_Window* window;
  SDL_PixelFormat* format;
  Uint32 time;

  FbleValue* event;
  FbleValue* effect;
} AppIO;

static void PrintUsage(FILE* stream);
static int ReadIntP(FbleValue* num);
static int ReadInt(FbleValue* num);
static Uint32 ReadColor(SDL_PixelFormat* format, FbleValue* color);
static void Draw(SDL_Surface* s, int ax, int ay, int bx, int by, FbleValue* drawing, SDL_PixelFormat* format);
static FbleValue* MakeIntP(FbleValueHeap* heap, int x);
static FbleValue* MakeInt(FbleValueHeap* heap, int x);
static FbleValue* MakeKey(FbleValueHeap* heap, SDL_Scancode scancode);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
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
  fprintf(stream, "%s",
      "Usage: fble-app [--profile FILE] " FBLE_MAIN_USAGE_SUMMARY "\n"
      "Execute an fble app process.\n"
      FBLE_MAIN_USAGE_DETAIL
      "Options:\n"
      "  --profile FILE\n"
      "    Writes a profile of the app to FILE\n"
      "Example: fble-app --profile app.prof " FBLE_MAIN_USAGE_EXAMPLE "\n"
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

// ReadColor --
//  Read the color value from a /Drawing%.Color@ tag.
//
// Inputs:
//   format - the pixel format of the screen to encode color with.
//   color - the /Drawing%.Color@ value.
//
// Returns:
//   The pixel value to use for the color.
//
// Side effects:
//   None.
static Uint32 ReadColor(SDL_PixelFormat* format, FbleValue* color)
{
  int r = ReadInt(FbleStructValueAccess(color, 0));
  int g = ReadInt(FbleStructValueAccess(color, 1));
  int b = ReadInt(FbleStructValueAccess(color, 2));
  return SDL_MapRGB(format, r, g, b);
}

// Draw --
//   Draw a drawing to the screen of type /Drawing%.Drawing@.
//
// Inputs:
//   surface - the surface to draw to.
//   ax, ay, bx, by - a transformation to apply to the drawing: a*p + b.
//   drawing - the drawing to draw.
//   format - the screen format, for determinig color values.
//
// Results:
//   none.
//
// Side effects:
//   Draws the drawing to the window. The caller must call
//   SDL_UpdateWindowSurface for the screen to actually be updated.
static void Draw(SDL_Surface* surface, int ax, int ay, int bx, int by, FbleValue* drawing, SDL_PixelFormat* format)
{
  switch (FbleUnionValueTag(drawing)) {
    case 0: {
      // Blank. Do nothing.
      return;
    }

    case 1: {
      // Rect
      FbleValue* rv = FbleUnionValueAccess(drawing);

      FbleValue* x = FbleStructValueAccess(rv, 0);
      FbleValue* y = FbleStructValueAccess(rv, 1);
      FbleValue* w = FbleStructValueAccess(rv, 2);
      FbleValue* h = FbleStructValueAccess(rv, 3);
      FbleValue* color = FbleStructValueAccess(rv, 4);

      SDL_Rect r = {
        ax * ReadInt(x) + bx,
        ay * ReadInt(y) + by,
        ax * ReadInt(w),
        ay * ReadInt(h)
      };

      if (r.w < 0) {
        r.x += r.w;
        r.w = -r.w;
      }

      if (r.h < 0) {
        r.y += r.h;
        r.h = -r.h;
      }

      SDL_FillRect(surface, &r, ReadColor(format, color));
      return;
    }

    case 2: {
      // Transformed.
      FbleValue* transformed = FbleUnionValueAccess(drawing);
      FbleValue* a = FbleStructValueAccess(transformed, 0);
      FbleValue* b = FbleStructValueAccess(transformed, 1);
      FbleValue* d = FbleStructValueAccess(transformed, 2);

      int axi = ReadInt(FbleStructValueAccess(a, 0));
      int ayi = ReadInt(FbleStructValueAccess(a, 1));
      int bxi = ReadInt(FbleStructValueAccess(b, 0));
      int byi = ReadInt(FbleStructValueAccess(b, 1));

      // a * (ai * x + bi) + b ==> (a*ai) x + (a*bi + b)
      Draw(surface, ax * axi, ay * ayi, ax * bxi + bx, ay * byi + by, d, format);
      return;
    }

    case 3: {
      // Over.
      FbleValue* over = FbleUnionValueAccess(drawing);
      Draw(surface, ax, ay, bx, by, FbleStructValueAccess(over, 0), format);
      Draw(surface, ax, ay, bx, by, FbleStructValueAccess(over, 1), format);
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
//   heap - the heap to use for allocations.
//   x - the integer value. Must be greater than 0.
//
// Results:
//   An FbleValue for the integer.
//
// Side effects:
//   Allocates a value that should be freed with FbleReleaseValue when no
//   longer needed. Behavior is undefined if x is not positive.
static FbleValue* MakeIntP(FbleValueHeap* heap, int x)
{
  assert(x > 0);
  if (x == 1) {
    return FbleNewEnumValue(heap, 0);
  }

  FbleValue* p = MakeIntP(heap, x / 2);
  FbleValue* result = FbleNewUnionValue(heap, 1 + (x % 2), p);
  FbleReleaseValue(heap, p);
  return result;
}

// MakeInt -- 
//   Make an FbleValue of type /Int/Int%.Int@ for the given integer.
//
// Inputs:
//   heap - the heap to use for allocations.
//   x - the integer value.
//
// Results:
//   An FbleValue for the integer.
//
// Side effects:
//   Allocates a value that should be freed with FbleReleaseValue when no
//   longer needed.
static FbleValue* MakeInt(FbleValueHeap* heap, int x)
{
  if (x < 0) {
    FbleValue* p = MakeIntP(heap, -x);
    FbleValue* result = FbleNewUnionValue(heap, 0, p);
    FbleReleaseValue(heap, p);
    return result;
  }

  if (x == 0) {
    return FbleNewEnumValue(heap, 1);
  }

  FbleValue* p = MakeIntP(heap, x);
  FbleValue* result = FbleNewUnionValue(heap, 2, p);
  FbleReleaseValue(heap, p);
  return result;
}

// MakeKey -- 
//   Make an FbleValue of type /App%.Key@ for the given scancode.
//
// Inputs:
//   heap - the heap to use for allocations.
//   scancode - the scan code.
//
// Results:
//   An FbleValue for the scancode, or NULL if there is corresponding Key@ for
//   that scan code.
//
// Side effects:
//   Allocates a value that should be freed with FbleReleaseValue when no
//   longer needed.
static FbleValue* MakeKey(FbleValueHeap* heap, SDL_Scancode scancode)
{
  int k = -1;
  switch (scancode) {
    case SDL_SCANCODE_A: k = 0; break;
    case SDL_SCANCODE_D: k = 1; break;
    case SDL_SCANCODE_H: k = 2; break;
    case SDL_SCANCODE_J: k = 3; break;
    case SDL_SCANCODE_K: k = 4; break;
    case SDL_SCANCODE_L: k = 5; break;
    case SDL_SCANCODE_Q: k = 6; break;
    case SDL_SCANCODE_S: k = 7; break;
    case SDL_SCANCODE_W: k = 8; break;
    case SDL_SCANCODE_LEFT: k = 9; break;
    case SDL_SCANCODE_RIGHT: k = 10; break;
    case SDL_SCANCODE_UP: k = 11; break;
    case SDL_SCANCODE_DOWN: k = 12; break;
    case SDL_SCANCODE_SPACE: k = 13; break;
    default: break;
  }

  if (k >= 0) {
    return FbleNewEnumValue(heap, k);
  }
  return NULL;
}

// IO --
//   FbleIO.io function for external ports.
//   See the corresponding documentation in fble-value.h.
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  AppIO* app = (AppIO*)io;

  bool change = false;

  if (app->effect != NULL) {
    FbleValue* effect = app->effect;
    switch (FbleUnionValueTag(effect)) {
      case 0: {
        int tick = ReadInt(FbleUnionValueAccess(effect));

        // TODO: This assumes we don't already have a tick in progress. We
        // should add proper support for multiple backed up tick requests.
        Uint32 now = SDL_GetTicks();
        app->time += tick;
        if (app->time < now) {
          app->time = now;
        }
        SDL_AddTimer(app->time - now, OnTimer, NULL);
        break;
      }

      case 1: {
        SDL_Surface* surface = SDL_GetWindowSurface(app->window);
        Draw(surface, 1, 1, 0, 0, FbleUnionValueAccess(effect), app->format);
        SDL_UpdateWindowSurface(app->window);

        // Collect status on frame rate.
        static Uint32 last = 0;

        Uint32 now = SDL_GetTicks();
        if (last == 0) {
          last = now;
        } else if (now > last) {
          Uint32 fps = 1000/(now - last);
          if (fps > 60) {
            fps = 60;
          }
          sFpsHistogram[fps]++;
          last = now;
        }
        break;
      }
    }

    FbleReleaseValue(heap, app->effect);
    app->effect = NULL;
    change = true;
  }

  if (block) {
    while (app->event == NULL) {
      SDL_Event event;
      SDL_WaitEvent(&event);
      switch (event.type) {
        case SDL_KEYDOWN: {
          FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
          if (key != NULL) {
            app->event = FbleNewUnionValue(heap, 1, key);
            FbleReleaseValue(heap, key);
            change = true;
          }
          break;
        }

        case SDL_KEYUP: {
          FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
          if (key != NULL) {
            app->event = FbleNewUnionValue(heap, 2, key);
            FbleReleaseValue(heap, key);
            change = true;
          }
          break;
        }

        case SDL_USEREVENT: {
          app->event = FbleNewEnumValue(heap, 0);
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
  argc--;
  argv++;
  if (argc > 0 && strcmp("--help", *argv) == 0) {
    PrintUsage(stdout);
    return 0;
  }

  FILE* fprofile = NULL;
  if (argc > 1 && strcmp("--profile", *argv) == 0) {
    fprofile = fopen(argv[1], "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", argv[1]);
      return 1;
    }

    argc -= 2;
    argv += 2;
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* linked = FbleMain(heap, profile, FbleCompiledMain, argc, argv);
  if (linked == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return 1;
  }

  FbleValue* func = FbleEval(heap, linked, profile);
  FbleReleaseValue(heap, linked);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return 1;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    FbleFreeValueHeap(heap);
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
      "Fble App", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480,
      0);
  SDL_ShowCursor(SDL_DISABLE);

  SDL_Surface* screen = SDL_GetWindowSurface(window);

  int width = 0;
  int height = 0;
  SDL_GetWindowSize(window, &width, &height);

  AppIO io = {
    ._base = { .io = &IO, },
    .window = window,
    .format = screen->format,
    .time = SDL_GetTicks(),

    .event = NULL,
    .effect = NULL,
  };

  FbleName block_names[3];
  block_names[0].name = FbleNewString("in!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("out!");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[2].name = FbleNewString("out!!");
  block_names[2].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleBlockId block_id = 0;
  if (profile != NULL) {
    FbleNameV names = { .size = 3, .xs = block_names };
    block_id = FbleProfileAddBlocks(profile, names);
  }
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);
  FbleFreeName(block_names[2]);

  FbleValue* args[] = {
    MakeInt(heap, width),
    MakeInt(heap, height),
    FbleNewInputPortValue(heap, &io.event, block_id),
    FbleNewOutputPortValue(heap, &io.effect, block_id + 1)
  };
  FbleValue* proc = FbleApply(heap, func, args, profile);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, args[0]);
  FbleReleaseValue(heap, args[1]);
  FbleReleaseValue(heap, args[2]);
  FbleReleaseValue(heap, args[3]);

  if (proc == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

  FbleValue* value = FbleExec(heap, &io._base, proc, profile);
  FbleReleaseValue(heap, proc);
  FbleReleaseValue(heap, io.event);
  FbleReleaseValue(heap, io.effect);

  FbleReleaseValue(heap, value);
  FbleFreeValueHeap(heap);

  if (fprofile != NULL) {
    FbleProfileReport(fprofile, profile);
  }
  FbleFreeProfile(profile);

  SDL_DestroyWindow(window);
  SDL_Quit();

  fprintf(stderr, "FPS Histogram:\n");
  for (size_t i = 0; i < 61; ++i) {
    if (sFpsHistogram[i] > 0) {
      printf("  % 2zi: % 12i\n", i, sFpsHistogram[i]);
    }
  }

  return 0;
}
