// app.fble.c --
//   Implementation of FbleAppMain function.

#include "app.fble.h"

#include <assert.h>       // for assert
#include <string.h>       // for strcmp

#include <SDL.h>          // for SDL_*
#include <GL/gl.h>        // for gl*

#include "fble-alloc.h"       // for FbleFree.
#include "fble-arg-parse.h"   // for FbleParseBoolArg, etc.
#include "fble-value.h"       // for FbleValue, etc.
#include "fble-vector.h"      // for FbleVectorInit.

#include "Core/char.fble.h"    // for FbleCharValueAccess
#include "Core/int.fble.h"     // for FbleIntValueAccess
#include "Core/string.fble.h"  // for FbleStringValueAccess

#define EX_SUCCESS 0
#define EX_FAILURE 1
#define EX_USAGE 2

// sFpsHistogram[i] is the number of samples with i frames per second.
// Anything above 60 FPS is counted towards i = 60.
static int sFpsHistogram[61] = {0};

// AppIO - Implementation of FbleIO for App, with some additional parameters
// to pass to the IO function.
//
// Fields:
//   _base - The underlying FbleIO object.
//   window - The window to draw to.
//   time - The current simulation time in units of SDL_GetTicks.
//
//   event - The event input port.
//   effect - The effect output port.
typedef struct {
  FbleIO _base;
  SDL_Window* window;
  Uint32 time;

  FbleValue* event;
  FbleValue* effect;
} AppIO;

static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module);
static void Draw(SDL_Surface* s, int ax, int ay, int bx, int by, FbleValue* drawing);
static FbleValue* MakeIntP(FbleValueHeap* heap, int x);
static FbleValue* MakeInt(FbleValueHeap* heap, int x);
static FbleValue* MakeKey(FbleValueHeap* heap, SDL_Scancode scancode);
static bool IO(FbleIO* io, FbleValueHeap* heap, bool block);
static Uint32 OnTimer(Uint32 interval, void* param);
int FbleStdioMain(int argc, const char* argv[], FbleCompiledModuleFunction* module);

// PrintUsage --
//   Prints help info to the given output stream.
//
// Inputs:
//   stream - The output stream to write the usage information to.
//   module - Non-NULL if a compiled module is provided, NULL otherwise.
//
// Side effects:
//   Outputs usage information to the given stream.
static void PrintUsage(FILE* stream, FbleCompiledModuleFunction* module)
{
  fprintf(stream, "Usage: fble-app [OPTION...]%s\n",
      module == NULL ? " -m MODULE_PATH" : "");
  fprintf(stream, "%s",
      "\n"
      "Description:\n"
      "  Runs an fble app program.\n"
      "\n"
      "Options:\n"
      "  -h, --help\n"
      "     Print this help message and exit.\n");
  if (module == NULL) {
    fprintf(stream, "%s",
      "  -I DIR\n"
      "     Adds DIR to the module search path.\n"
      "  -m, --module MODULE_PATH\n"
      "     The path of the module to get dependencies for.\n");
  }
  fprintf(stream, "%s",
      "  --profile FILE\n"
      "    Writes a profile of the app to FILE\n"
      "\n"
      "Exit Status:\n"
      "  0 on success.\n"
      "  1 on failure.\n"
      "  2 on usage error.\n"
      "\n"
      "Example:\n");
  fprintf(stream, "%s%s\n",
      "  fble-app --profile foo.prof ",
      module == NULL ? "-I prgms -m /Foo% " : "");
}

// Draw --
//   Draw a drawing to the screen of type /Drawing%.Drawing@.
//
// Inputs:
//   surface - the surface to draw to.
//   ax, ay, bx, by - a transformation to apply to the drawing: a*p + b.
//   drawing - the drawing to draw.
//
// Results:
//   none.
//
// Side effects:
//   Draws the drawing to the window. The caller must call
//   SDL_UpdateWindowSurface for the screen to actually be updated.
static void Draw(SDL_Surface* surface, int ax, int ay, int bx, int by, FbleValue* drawing)
{
  switch (FbleUnionValueTag(drawing)) {
    case 0: {
      // Blank. Do nothing.
      return;
    }

    case 1: {
      // Triangle
      FbleValue* v = FbleUnionValueAccess(drawing);

      FbleValue* a = FbleStructValueAccess(v, 0);
      FbleValue* b = FbleStructValueAccess(v, 1);
      FbleValue* c = FbleStructValueAccess(v, 2);
      FbleValue* color = FbleStructValueAccess(v, 3);

      int x0 = ax * FbleIntValueAccess(FbleStructValueAccess(a, 0)) + bx;
      int y0 = ay * FbleIntValueAccess(FbleStructValueAccess(a, 1)) + by;
      int x1 = ax * FbleIntValueAccess(FbleStructValueAccess(b, 0)) + bx;
      int y1 = ay * FbleIntValueAccess(FbleStructValueAccess(b, 1)) + by;
      int x2 = ax * FbleIntValueAccess(FbleStructValueAccess(c, 0)) + bx;
      int y2 = ay * FbleIntValueAccess(FbleStructValueAccess(c, 1)) + by;

      int red = FbleIntValueAccess(FbleStructValueAccess(color, 0));
      int green = FbleIntValueAccess(FbleStructValueAccess(color, 1));
      int blue = FbleIntValueAccess(FbleStructValueAccess(color, 2));
      glColor3f(red/256.0, green/256.0, blue/256.0);
      glBegin(GL_TRIANGLES);
      glVertex2i(x0, y0);
      glVertex2i(x1, y1);
      glVertex2i(x2, y2);
      glEnd();
      return;
    }

    case 2: {
      // Rect
      FbleValue* rv = FbleUnionValueAccess(drawing);

      FbleValue* x = FbleStructValueAccess(rv, 0);
      FbleValue* y = FbleStructValueAccess(rv, 1);
      FbleValue* w = FbleStructValueAccess(rv, 2);
      FbleValue* h = FbleStructValueAccess(rv, 3);
      FbleValue* color = FbleStructValueAccess(rv, 4);

      SDL_Rect r = {
        ax * FbleIntValueAccess(x) + bx,
        ay * FbleIntValueAccess(y) + by,
        ax * FbleIntValueAccess(w),
        ay * FbleIntValueAccess(h)
      };

      if (r.w < 0) {
        r.x += r.w;
        r.w = -r.w;
      }

      if (r.h < 0) {
        r.y += r.h;
        r.h = -r.h;
      }

      int red = FbleIntValueAccess(FbleStructValueAccess(color, 0));
      int green = FbleIntValueAccess(FbleStructValueAccess(color, 1));
      int blue = FbleIntValueAccess(FbleStructValueAccess(color, 2));
      glColor3f(red/256.0, green/256.0, blue/256.0);
      glRecti(r.x, r.y, r.x + r.w, r.y + r.h);
      return;
    }

    case 3: {
      // Transformed.
      FbleValue* transformed = FbleUnionValueAccess(drawing);
      FbleValue* a = FbleStructValueAccess(transformed, 0);
      FbleValue* b = FbleStructValueAccess(transformed, 1);
      FbleValue* d = FbleStructValueAccess(transformed, 2);

      int axi = FbleIntValueAccess(FbleStructValueAccess(a, 0));
      int ayi = FbleIntValueAccess(FbleStructValueAccess(a, 1));
      int bxi = FbleIntValueAccess(FbleStructValueAccess(b, 0));
      int byi = FbleIntValueAccess(FbleStructValueAccess(b, 1));

      // a * (ai * x + bi) + b ==> (a*ai) x + (a*bi + b)
      Draw(surface, ax * axi, ay * ayi, ax * bxi + bx, ay * byi + by, d);
      return;
    }

    case 4: {
      // Over.
      FbleValue* over = FbleUnionValueAccess(drawing);
      Draw(surface, ax, ay, bx, by, FbleStructValueAccess(over, 0));
      Draw(surface, ax, ay, bx, by, FbleStructValueAccess(over, 1));
      return;
    }

    default: {
      assert(false && "Invalid Drawing@ tag");
      abort();
    }
  }
}

// MakeIntP -- 
//   Make an FbleValue of type /Core/Int/Core/IntP%.IntP@ for the given integer.
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
//   Make an FbleValue of type /Core/Int%.Int@ for the given integer.
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
    case SDL_SCANCODE_LSHIFT: k = 14; break;
    case SDL_SCANCODE_RSHIFT: k = 15; break;
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
        int tick = FbleIntValueAccess(FbleUnionValueAccess(effect));

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
        Draw(surface, 1, 1, 0, 0, FbleUnionValueAccess(effect));
        SDL_GL_SwapWindow(app->window);

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

// FbleAppMain -- See documentation in app.fble.h
int FbleAppMain(int argc, const char* argv[], FbleCompiledModuleFunction* module)
{
  // To ease debugging of FbleAppMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  FbleSearchPath search_path;
  FbleVectorInit(search_path);
  const char* module_path = NULL;
  const char* profile_file = NULL;
  bool help = false;
  bool error = false;

  argc--;
  argv++;
  while (!error && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (!module && FbleParseSearchPathArg("-I", &search_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("-m", &module_path, &argc, &argv, &error)) continue;
    if (!module && FbleParseStringArg("--module", &module_path, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;
    if (FbleParseInvalidArg(&argc, &argv, &error)) continue;
  }

  if (help) {
    PrintUsage(stdout, module);
    FbleFree(search_path.xs);
    return EX_SUCCESS;
  }

  if (error) {
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  if (!module && module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    PrintUsage(stderr, module);
    FbleFree(search_path.xs);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFree(search_path.xs);
      return EX_FAILURE;
    }
  }

  FbleProfile* profile = fprofile == NULL ? NULL : FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* app = FbleLinkFromCompiledOrSource(heap, profile, module, search_path, module_path);
  FbleFree(search_path.xs);
  if (app == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FbleValue* func = FbleEval(heap, app, profile);
  FbleReleaseValue(heap, app);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    FbleFreeValueHeap(heap);
    return EX_FAILURE;
  }

  SDL_Window* window = SDL_CreateWindow(
      "Fble App", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480,
      SDL_WINDOW_OPENGL);
  SDL_GLContext glctx = SDL_GL_CreateContext(window);
  SDL_ShowCursor(SDL_DISABLE);

  int width = 0;
  int height = 0;
  SDL_GetWindowSize(window, &width, &height);

  glShadeModel(GL_FLAT);
  glViewport(0, 0, width, height);
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);

  AppIO io = {
    ._base = { .io = &IO, },
    .window = window,
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

  FbleValue* fble_width = MakeInt(heap, width);
  FbleValue* fble_height = MakeInt(heap, height);
  FbleValue* fble_event = FbleNewInputPortValue(heap, &io.event, block_id);
  FbleValue* fble_effect = FbleNewOutputPortValue(heap, &io.effect, block_id + 1);
  FbleValue* fble_app = FbleNewStructValue(heap, 4, fble_width, fble_height, fble_event, fble_effect);
  FbleReleaseValue(heap, fble_width);
  FbleReleaseValue(heap, fble_height);
  FbleReleaseValue(heap, fble_event);
  FbleReleaseValue(heap, fble_effect);

  FbleValue* args[1] = { fble_app };
  FbleValue* proc = FbleApply(heap, func, args, profile);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, args[0]);

  if (proc == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EX_FAILURE;
  }

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

  SDL_GL_DeleteContext(glctx);
  SDL_DestroyWindow(window);
  SDL_Quit();

  fprintf(stderr, "FPS Histogram:\n");
  for (size_t i = 0; i < 61; ++i) {
    if (sFpsHistogram[i] > 0) {
      printf("  % 3zi: % 12i\n", i, sFpsHistogram[i]);
    }
  }

  return EX_SUCCESS;
}