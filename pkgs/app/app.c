// app.c --
//   Implementation of FbleAppMain function.

#include "app.h"

#include <assert.h>       // for assert
#include <string.h>       // for strcmp

#include <SDL.h>          // for SDL_*
#include <GL/gl.h>        // for gl*

#include <fble/fble-alloc.h>       // for FbleFree.
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-usage.h>       // for FblePrintUsageDoc
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-version.h>     // for FBLE_VERSION

#include "char.fble.h"             // for FbleCharValueAccess
#include "int.fble.h"              // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"           // for FbleStringValueAccess
#include "stdio.fble.h"            // for FbleNewStdioIO

#define EX_SUCCESS 0
#define EX_USAGE 2
#define EX_FAILURE 3

typedef struct {
  SDL_Window* window;
  Uint32 time;

  // fpsHistogram[i] is the number of samples with i frames per second.
  // Anything above 60 FPS is counted towards i = 60.
  int fpsHistogram[61];
} App;

static void Draw(SDL_Surface* s, int ax, int ay, int bx, int by, FbleValue* drawing);
static FbleValue* MakeKey(FbleValueHeap* heap, SDL_Scancode scancode);
static FbleValue* MakeButton(FbleValueHeap* heap, Uint8 button);

static FbleValue* EventImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleValue** tail_call_buffer, FbleFunction* function, FbleValue** args);
static FbleValue* EffectImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleValue** tail_call_buffer, FbleFunction* function, FbleValue** args);

static Uint32 OnTimer(Uint32 interval, void* param);

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
      FbleValue* v = FbleUnionValueArg(drawing);

      FbleValue* a = FbleStructValueField(v, 0);
      FbleValue* b = FbleStructValueField(v, 1);
      FbleValue* c = FbleStructValueField(v, 2);
      FbleValue* color = FbleStructValueField(v, 3);

      int x0 = ax * FbleIntValueAccess(FbleStructValueField(a, 0)) + bx;
      int y0 = ay * FbleIntValueAccess(FbleStructValueField(a, 1)) + by;
      int x1 = ax * FbleIntValueAccess(FbleStructValueField(b, 0)) + bx;
      int y1 = ay * FbleIntValueAccess(FbleStructValueField(b, 1)) + by;
      int x2 = ax * FbleIntValueAccess(FbleStructValueField(c, 0)) + bx;
      int y2 = ay * FbleIntValueAccess(FbleStructValueField(c, 1)) + by;

      int red = FbleIntValueAccess(FbleStructValueField(color, 0));
      int green = FbleIntValueAccess(FbleStructValueField(color, 1));
      int blue = FbleIntValueAccess(FbleStructValueField(color, 2));
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
      FbleValue* rv = FbleUnionValueArg(drawing);

      FbleValue* x = FbleStructValueField(rv, 0);
      FbleValue* y = FbleStructValueField(rv, 1);
      FbleValue* w = FbleStructValueField(rv, 2);
      FbleValue* h = FbleStructValueField(rv, 3);
      FbleValue* color = FbleStructValueField(rv, 4);

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

      int red = FbleIntValueAccess(FbleStructValueField(color, 0));
      int green = FbleIntValueAccess(FbleStructValueField(color, 1));
      int blue = FbleIntValueAccess(FbleStructValueField(color, 2));
      glColor3f(red/256.0, green/256.0, blue/256.0);
      glRecti(r.x, r.y, r.x + r.w, r.y + r.h);
      return;
    }

    case 3: {
      // Transformed.
      FbleValue* transformed = FbleUnionValueArg(drawing);
      FbleValue* a = FbleStructValueField(transformed, 0);
      FbleValue* b = FbleStructValueField(transformed, 1);
      FbleValue* d = FbleStructValueField(transformed, 2);

      int axi = FbleIntValueAccess(FbleStructValueField(a, 0));
      int ayi = FbleIntValueAccess(FbleStructValueField(a, 1));
      int bxi = FbleIntValueAccess(FbleStructValueField(b, 0));
      int byi = FbleIntValueAccess(FbleStructValueField(b, 1));

      // a * (ai * x + bi) + b ==> (a*ai) x + (a*bi + b)
      Draw(surface, ax * axi, ay * ayi, ax * bxi + bx, ay * byi + by, d);
      return;
    }

    case 4: {
      // Over.
      FbleValue* over = FbleUnionValueArg(drawing);
      Draw(surface, ax, ay, bx, by, FbleStructValueField(over, 0));
      Draw(surface, ax, ay, bx, by, FbleStructValueField(over, 1));
      return;
    }

    default: {
      assert(false && "Invalid Drawing@ tag");
      abort();
    }
  }
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

// MakeButton -- 
//   Make an FbleValue of type /App%.Button@ for the given button.
//
// Inputs:
//   heap - the heap to use for allocations.
//   button - the button id.
//
// Results:
//   An FbleValue for the button, or NULL if there is corresponding Button@
//   for that button
//
// Side effects:
//   Allocates a value that should be freed with FbleReleaseValue when no
//   longer needed.
static FbleValue* MakeButton(FbleValueHeap* heap, Uint8 button)
{
  int k = -1;
  switch (button) {
    case SDL_BUTTON_LEFT: k = 0; break;
    case SDL_BUTTON_RIGHT: k = 1; break;
    default: break;
  }

  if (k >= 0) {
    return FbleNewEnumValue(heap, k);
  }
  return NULL;
}

// Event -- Implementation of event function.
//   IO@<Event@>
static FbleValue* EventImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleValue** tail_call_buffer, FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)tail_call_buffer;

  FbleValue* world = args[0];
  FbleValue* value = NULL;
  while (value == NULL) {
    SDL_Event event;
    SDL_WaitEvent(&event);
    switch (event.type) {
      case SDL_USEREVENT: {
        value = FbleNewEnumValue(heap, 0);
        break;
      }

      case SDL_KEYDOWN: {
        FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
        if (key != NULL) {
          value = FbleNewUnionValue(heap, 1, key);
          FbleReleaseValue(heap, key);
        }
        break;
      }

      case SDL_KEYUP: {
        FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
        if (key != NULL) {
          value = FbleNewUnionValue(heap, 2, key);
          FbleReleaseValue(heap, key);
        }
        break;
      }

      case SDL_MOUSEBUTTONDOWN: {
        FbleValue* button = MakeButton(heap, event.button.button);
        if (button != NULL) {
          FbleValue* x = FbleNewIntValue(heap, event.button.x);
          FbleValue* y = FbleNewIntValue(heap, event.button.y);
          FbleValue* mouse_button = FbleNewStructValue_(heap, 3, button, x, y);
          FbleReleaseValue(heap, button);
          FbleReleaseValue(heap, x);
          FbleReleaseValue(heap, y);
          value = FbleNewUnionValue(heap, 3, mouse_button);
          FbleReleaseValue(heap, mouse_button);
        }
        break;
      }

      case SDL_MOUSEBUTTONUP: {
        FbleValue* button = MakeButton(heap, event.button.button);
        if (button != NULL) {
          FbleValue* x = FbleNewIntValue(heap, event.button.x);
          FbleValue* y = FbleNewIntValue(heap, event.button.y);
          FbleValue* mouse_button = FbleNewStructValue_(heap, 3, button, x, y);
          FbleReleaseValue(heap, button);
          FbleReleaseValue(heap, x);
          FbleReleaseValue(heap, y);
          value = FbleNewUnionValue(heap, 4, mouse_button);
          FbleReleaseValue(heap, mouse_button);
        }
        break;
      }

      case SDL_WINDOWEVENT: {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          glViewport(0, 0, event.window.data1, event.window.data2);
          glMatrixMode( GL_PROJECTION );
          glLoadIdentity();
          glOrtho(0, event.window.data1, event.window.data2, 0, -1, 1);
          glClear(GL_COLOR_BUFFER_BIT);

          FbleValue* width = FbleNewIntValue(heap, event.window.data1);
          FbleValue* height = FbleNewIntValue(heap, event.window.data2);
          FbleValue* resized = FbleNewStructValue_(heap, 2, width, height);
          FbleReleaseValue(heap, width);
          FbleReleaseValue(heap, height);
          value = FbleNewUnionValue(heap, 5, resized);
          FbleReleaseValue(heap, resized);
        }
        break;
      }

      case SDL_MOUSEMOTION: {
        FbleValue* x = FbleNewIntValue(heap, event.motion.x);
        FbleValue* y = FbleNewIntValue(heap, event.motion.y);
        FbleValue* dx = FbleNewIntValue(heap, event.motion.xrel);
        FbleValue* dy = FbleNewIntValue(heap, event.motion.yrel);
        FbleValue* motion = FbleNewStructValue_(heap, 4, x, y, dx, dy);
        FbleReleaseValue(heap, x);
        FbleReleaseValue(heap, y);
        FbleReleaseValue(heap, dx);
        FbleReleaseValue(heap, dy);
        value = FbleNewUnionValue(heap, 6, motion);
        FbleReleaseValue(heap, motion);
        break;
      }
    }
  }

  FbleValue* result = FbleNewStructValue_(heap, 2, world, value);
  FbleReleaseValue(heap, value);
  return result;
}

// Effect -- Implementation of effect function.
//   (Effect@, World@) { R@<Unit@>; }
static FbleValue* EffectImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleValue** tail_call_buffer, FbleFunction* function, FbleValue** args)
{
  (void)profile;
  (void)tail_call_buffer;

  App* app = *(App**)FbleNativeValueData(function->statics[0]);

  FbleValue* effect = args[0];
  FbleValue* world = args[1];

  switch (FbleUnionValueTag(effect)) {
    case 0: {
      int tick = FbleIntValueAccess(FbleUnionValueArg(effect));

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
      Draw(surface, 1, 1, 0, 0, FbleUnionValueArg(effect));
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
        app->fpsHistogram[fps]++;
        last = now;
      }
      break;
    }
  }

  FbleValue* unit = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleNewStructValue_(heap, 2, world, unit);
  FbleReleaseValue(heap, unit);
  return result;
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
  (void)interval;
  (void)param;

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
  const char* arg0 = argv[0];

  // To ease debugging of FbleAppMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  FbleModuleArg module_arg = FbleNewModuleArg();
  const char* profile_file = NULL;
  bool end_of_options = false;
  bool help = false;
  bool error = false;
  bool version = false;
  bool fps = false;

  // If the module is compiled and '--' isn't present, skip to end of options
  // right away. That way precompiled programs can go straight to application
  // args if they want.
  if (module != NULL) {
    end_of_options = true;
    for (int i = 0; i < argc; ++i) {
      if (strcmp(argv[i], "--") == 0) {
        end_of_options = false;
        break;
      }
    }
  }

  argc--;
  argv++;
  while (!(help || error || version) && !end_of_options && argc > 0) {
    if (FbleParseBoolArg("-h", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--help", &help, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("-v", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--version", &version, &argc, &argv, &error)) continue;
    if (FbleParseBoolArg("--fps", &fps, &argc, &argv, &error)) continue;
    if (!module && FbleParseModuleArg(&module_arg, &argc, &argv, &error)) continue;
    if (FbleParseStringArg("--profile", &profile_file, &argc, &argv, &error)) continue;

    end_of_options = true;
    if (strcmp(argv[0], "--") == 0) {
      argc--;
      argv++;
    }
  }

  if (version) {
    FblePrintCompiledHeaderLine(stdout, "fble-app", arg0, module);
    FblePrintVersion(stdout, "fble-app");
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (help) {
    FblePrintCompiledHeaderLine(stdout, "fble-app", arg0, module);
    FblePrintUsageDoc(arg0, "fble-app.usage.txt");
    FbleFreeModuleArg(module_arg);
    return EX_SUCCESS;
  }

  if (error) {
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  if (!module && module_arg.module_path == NULL) {
    fprintf(stderr, "missing required --module option.\n");
    fprintf(stderr, "Try --help for usage info.\n");
    FbleFreeModuleArg(module_arg);
    return EX_USAGE;
  }

  FILE* fprofile = NULL;
  if (profile_file != NULL) {
    fprofile = fopen(profile_file, "w");
    if (fprofile == NULL) {
      fprintf(stderr, "unable to open %s for writing.\n", profile_file);
      FbleFreeModuleArg(module_arg);
      return EX_FAILURE;
    }
  }

  FbleProfile* profile = FbleNewProfile(fprofile != NULL);
  FbleValueHeap* heap = FbleNewValueHeap();

  FbleValue* app_main = FbleLink(heap, profile, module, module_arg.search_path, module_arg.module_path);
  FbleFreeModuleArg(module_arg);
  if (app_main == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  FbleValue* func = FbleEval(heap, app_main, profile);
  FbleReleaseValue(heap, app_main);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    FbleReleaseValue(heap, func);
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return EX_FAILURE;
  }

  SDL_Window* window = SDL_CreateWindow(
      "Fble App", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480,
      SDL_WINDOW_OPENGL);
  SDL_SetWindowResizable(window, true);
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

  FbleName block_names[2];
  block_names[0].name = FbleNewString("event!");
  block_names[0].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  block_names[1].name = FbleNewString("effect!");
  block_names[1].loc = FbleNewLoc(__FILE__, __LINE__-1, 3);
  FbleNameV names = { .size = 2, .xs = block_names };
  FbleBlockId block_id = FbleAddBlocksToProfile(profile, names);
  FbleFreeName(block_names[0]);
  FbleFreeName(block_names[1]);

  FbleExecutable event_exe = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .num_args = 1,
    .num_statics = 0,
    .tail_call_buffer_size = 0,
    .profile_block_id = block_id,
    .run = &EventImpl,
    .on_free = &FbleExecutableNothingOnFree,
  };
  FbleValue* fble_event = FbleNewFuncValue(heap, &event_exe, 0, NULL);

  FbleExecutable effect_exe = {
    .refcount = 1,
    .magic = FBLE_EXECUTABLE_MAGIC,
    .num_args = 2,
    .num_statics = 1,
    .tail_call_buffer_size = 0,
    .profile_block_id = block_id + 1,
    .run = &EffectImpl,
    .on_free = &FbleExecutableNothingOnFree,
  };

  App app;
  app.window = window;
  app.time = SDL_GetTicks();
  for (size_t i = 0; i < 61; ++i) {
    app.fpsHistogram[i] = 0;
  }

  App* app_ptr = &app;
  FbleValue* app_value = FbleNewNativeValue(heap, sizeof(App*), NULL, &app_ptr);
  FbleValue* fble_effect = FbleNewFuncValue(heap, &effect_exe, 0, &app_value);
  FbleReleaseValue(heap, app_value);

  FbleValue* fble_stdio = FbleNewStdioIO(heap, profile);

  FbleValue* argS = FbleNewEnumValue(heap, 1);
  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argValue = FbleNewStringValue(heap, argv[i]);
    FbleValue* argP = FbleNewStructValue_(heap, 2, argValue, argS);
    FbleReleaseValue(heap, argValue);
    FbleReleaseValue(heap, argS);
    argS = FbleNewUnionValue(heap, 0, argP);
    FbleReleaseValue(heap, argP);
  }

  FbleValue* fble_width = FbleNewIntValue(heap, width);
  FbleValue* fble_height = FbleNewIntValue(heap, height);

  FbleValue* args[6] = {
    fble_stdio, fble_event, fble_effect, fble_width, fble_height, argS
  };
  FbleValue* computation = FbleApply(heap, func, 6, args, profile);
  FbleReleaseValue(heap, func);
  FbleReleaseValue(heap, args[0]);
  FbleReleaseValue(heap, args[1]);
  FbleReleaseValue(heap, args[2]);
  FbleReleaseValue(heap, args[3]);
  FbleReleaseValue(heap, args[4]);
  FbleReleaseValue(heap, args[5]);

  if (computation == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EX_FAILURE;
  }

  // computation has type IO@<Bool@>, which is (World@) { R@<Bool@>; }
  FbleValue* world = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleApply(heap, computation, 1, &world, profile);

  if (fps) {
    fprintf(stderr, "FPS Histogram:\n");
    for (size_t i = 0; i < 61; ++i) {
      if (app.fpsHistogram[i] > 0) {
        printf("  % 3zi: % 12i\n", i, app.fpsHistogram[i]);
      }
    }
  }

  int exit_status = EX_FAILURE;
  if (result != NULL) {
    exit_status = FbleUnionValueTag(FbleStructValueField(result, 1));
  }

  FbleReleaseValue(heap, computation);
  FbleReleaseValue(heap, result);
  FbleFreeValueHeap(heap);

  FbleGenerateProfileReport(fprofile, profile);
  FbleFreeProfile(profile);

  SDL_GL_DeleteContext(glctx);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return exit_status;
}
