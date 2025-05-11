/**
 * @file app.c
 *  Implementation of FbleAppMain function.
 */

#include "app.h"

#include <assert.h>       // for assert
#include <string.h>       // for strcmp

#include <SDL.h>          // for SDL_*
#include <GL/gl.h>        // for gl*

#include <fble/fble-alloc.h>       // for FbleFree.
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-main.h>        // for FbleMain.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector, etc.
#include <fble/fble-version.h>     // for FblePrintVersion

#include "char.fble.h"             // for FbleCharValueAccess
#include "debug.fble.h"            // for /Core/Debug/Builtin%
#include "int.fble.h"              // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"           // for FbleStringValueAccess
#include "stdio.fble.h"            // for FbleNewStdioIO

#include "fble-app.usage.h"        // for fbldUsageHelpText

#define DRAWING_TAGWIDTH 3
#define POINT_FIELDC 2
#define COLOR_FIELDC 3
#define TRIANGLE_FIELDC 4
#define RECT_FIELDC 5
#define TRANSFORMED_FIELDC 3
#define OVER_FIELDC 2
#define KEY_TAGWIDTH 4
#define BUTTON_TAGWIDTH 1
#define EVENT_TAGWIDTH 3
#define EFFECT_TAGWIDTH 1
#define LIST_TAGWIDTH 1
#define RESULT_FIELDC 2
#define BOOL_TAGWIDTH 1

typedef struct {
  bool fps;
  const char* driver;
} Args;

static bool ParseArg(void* dest, int* argc, const char*** argv, bool* error);

/**
 * @struct[App] Application state.
 *  @field[SDL_Window*][window] The application window for drawing.
 *  @field[Uint32][time] The application time based on effect ticks.
 *  @field[int*][fpsHistogram]
 *   The value at index i is the count of samples with i frames per second.
 *   Anything above 60 FPS is counted towards i = 60.
 */
typedef struct {
  SDL_Window* window;
  Uint32 time;
  int fpsHistogram[61];
} App;

static void Draw(SDL_Surface* s, int ax, int ay, int bx, int by, FbleValue* drawing);
static FbleValue* MakeKey(FbleValueHeap* heap, SDL_Scancode scancode);
static FbleValue* MakeButton(FbleValueHeap* heap, Uint8 button);

static FbleValue* EventImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* EffectImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);

static Uint32 OnTimer(Uint32 interval, void* param);

/**
 * @func[ParseArg] Arg parser for mem-tests.
 *  See documentation of FbleArgParser in fble-arg-parse.h for more info.
 */
static bool ParseArg(void* dest, int* argc, const char*** argv, bool* error)
{
  Args* args = (Args*)dest;
  if (FbleParseBoolArg("--fps", &args->fps, argc, argv, error)) return true;
  if (FbleParseStringArg("--driver", &args->driver, argc, argv, error)) return true;

  return false;
}

/**
 * @func[Draw] Draws a drawing to the screen of type @l{/Drawing%.Drawing@}.
 *  @arg[SDL_Surface*][surface] The surface to draw to.
 *  @arg[int][ax, ay, bx, by]
 *   A transformation to apply to the drawing: a*p + b.
 *  @arg[FbleValue*][drawing] The drawing to draw.
 *
 *  @sideeffects
 *   Draws the drawing to the window. The caller must call
 *   SDL_UpdateWindowSurface for the screen to actually be updated.
 */
static void Draw(SDL_Surface* surface, int ax, int ay, int bx, int by, FbleValue* drawing)
{
  switch (FbleUnionValueTag(drawing, DRAWING_TAGWIDTH)) {
    case 0: {
      // Blank. Do nothing.
      return;
    }

    case 1: {
      // Triangle
      FbleValue* v = FbleUnionValueArg(drawing, DRAWING_TAGWIDTH);

      FbleValue* a = FbleStructValueField(v, TRIANGLE_FIELDC, 0);
      FbleValue* b = FbleStructValueField(v, TRIANGLE_FIELDC, 1);
      FbleValue* c = FbleStructValueField(v, TRIANGLE_FIELDC, 2);
      FbleValue* color = FbleStructValueField(v, TRIANGLE_FIELDC, 3);

      int x0 = ax * FbleIntValueAccess(FbleStructValueField(a, POINT_FIELDC, 0)) + bx;
      int y0 = ay * FbleIntValueAccess(FbleStructValueField(a, POINT_FIELDC, 1)) + by;
      int x1 = ax * FbleIntValueAccess(FbleStructValueField(b, POINT_FIELDC, 0)) + bx;
      int y1 = ay * FbleIntValueAccess(FbleStructValueField(b, POINT_FIELDC, 1)) + by;
      int x2 = ax * FbleIntValueAccess(FbleStructValueField(c, POINT_FIELDC, 0)) + bx;
      int y2 = ay * FbleIntValueAccess(FbleStructValueField(c, POINT_FIELDC, 1)) + by;

      int red = FbleIntValueAccess(FbleStructValueField(color, COLOR_FIELDC, 0));
      int green = FbleIntValueAccess(FbleStructValueField(color, COLOR_FIELDC, 1));
      int blue = FbleIntValueAccess(FbleStructValueField(color, COLOR_FIELDC, 2));
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
      FbleValue* rv = FbleUnionValueArg(drawing, DRAWING_TAGWIDTH);

      FbleValue* x = FbleStructValueField(rv, RECT_FIELDC, 0);
      FbleValue* y = FbleStructValueField(rv, RECT_FIELDC, 1);
      FbleValue* w = FbleStructValueField(rv, RECT_FIELDC, 2);
      FbleValue* h = FbleStructValueField(rv, RECT_FIELDC, 3);
      FbleValue* color = FbleStructValueField(rv, RECT_FIELDC, 4);

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

      int red = FbleIntValueAccess(FbleStructValueField(color, COLOR_FIELDC, 0));
      int green = FbleIntValueAccess(FbleStructValueField(color, COLOR_FIELDC, 1));
      int blue = FbleIntValueAccess(FbleStructValueField(color, COLOR_FIELDC, 2));
      glColor3f(red/256.0, green/256.0, blue/256.0);
      glRecti(r.x, r.y, r.x + r.w, r.y + r.h);
      return;
    }

    case 3: {
      // Transformed.
      FbleValue* transformed = FbleUnionValueArg(drawing, DRAWING_TAGWIDTH);
      FbleValue* a = FbleStructValueField(transformed, TRANSFORMED_FIELDC, 0);
      FbleValue* b = FbleStructValueField(transformed, TRANSFORMED_FIELDC, 1);
      FbleValue* d = FbleStructValueField(transformed, TRANSFORMED_FIELDC, 2);

      int axi = FbleIntValueAccess(FbleStructValueField(a, POINT_FIELDC, 0));
      int ayi = FbleIntValueAccess(FbleStructValueField(a, POINT_FIELDC, 1));
      int bxi = FbleIntValueAccess(FbleStructValueField(b, POINT_FIELDC, 0));
      int byi = FbleIntValueAccess(FbleStructValueField(b, POINT_FIELDC, 1));

      // a * (ai * x + bi) + b ==> (a*ai) x + (a*bi + b)
      Draw(surface, ax * axi, ay * ayi, ax * bxi + bx, ay * byi + by, d);
      return;
    }

    case 4: {
      // Over.
      FbleValue* over = FbleUnionValueArg(drawing, DRAWING_TAGWIDTH);
      Draw(surface, ax, ay, bx, by, FbleStructValueField(over, OVER_FIELDC, 0));
      Draw(surface, ax, ay, bx, by, FbleStructValueField(over, OVER_FIELDC, 1));
      return;
    }

    default: {
      assert(false && "Invalid Drawing@ tag");
      abort();
    }
  }
}

/**
 * @func[MakeKey] Makes an @l{/App%.Key@} for the given scancode.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[SDL_Scancode*][scancode] The scan code.
 *
 *  @returns[FbleValue*]
 *   An FbleValue for the scancode, or NULL if there is corresponding @l{Key@}
 *   for that scan code.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
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
    return FbleNewEnumValue(heap, KEY_TAGWIDTH, k);
  }
  return NULL;
}

/**
 * @func[MakeButton] Makes an FbleValue of type @l{/App%.Button@}.
 *  @arg[FbleValueHeap*][heap] The heap to use for allocations.
 *  @arg[FbleValueHeap*][button] The button id.
 *
 *  @returns[FbleValue*]
 *   An FbleValue for the button, or NULL if there is corresponding
 *   @l{Button@} for that button
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* MakeButton(FbleValueHeap* heap, Uint8 button)
{
  int k = -1;
  switch (button) {
    case SDL_BUTTON_LEFT: k = 0; break;
    case SDL_BUTTON_RIGHT: k = 1; break;
    default: break;
  }

  if (k >= 0) {
    return FbleNewEnumValue(heap, BUTTON_TAGWIDTH, k);
  }
  return NULL;
}

/**
 * @func[EventImpl] FbleRunFunction Implementation of @l{App@.event} function.
 *  See documentation of FbleRunFunction in fble-function.h.
 *  Has fble type @l{IO@<Event>}.
 *  Gets the next input event.
 */
static FbleValue* EventImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  FbleValue* world = args[0];
  FbleValue* value = NULL;
  while (value == NULL) {
    SDL_Event event;
    SDL_WaitEvent(&event);
    switch (event.type) {
      case SDL_USEREVENT: {
        value = FbleNewEnumValue(heap, EVENT_TAGWIDTH, 0);
        break;
      }

      case SDL_KEYDOWN: {
        FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
        if (key != NULL) {
          value = FbleNewUnionValue(heap, EVENT_TAGWIDTH, 1, key);
        }
        break;
      }

      case SDL_KEYUP: {
        FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
        if (key != NULL) {
          value = FbleNewUnionValue(heap, EVENT_TAGWIDTH, 2, key);
        }
        break;
      }

      case SDL_MOUSEBUTTONDOWN: {
        FbleValue* button = MakeButton(heap, event.button.button);
        if (button != NULL) {
          FbleValue* x = FbleNewIntValue(heap, event.button.x);
          FbleValue* y = FbleNewIntValue(heap, event.button.y);
          FbleValue* mouse_button = FbleNewStructValue_(heap, 3, button, x, y);
          value = FbleNewUnionValue(heap, EVENT_TAGWIDTH, 3, mouse_button);
        }
        break;
      }

      case SDL_MOUSEBUTTONUP: {
        FbleValue* button = MakeButton(heap, event.button.button);
        if (button != NULL) {
          FbleValue* x = FbleNewIntValue(heap, event.button.x);
          FbleValue* y = FbleNewIntValue(heap, event.button.y);
          FbleValue* mouse_button = FbleNewStructValue_(heap, 3, button, x, y);
          value = FbleNewUnionValue(heap, EVENT_TAGWIDTH, 4, mouse_button);
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
          value = FbleNewUnionValue(heap, EVENT_TAGWIDTH, 5, resized);
        }
        break;
      }

      case SDL_MOUSEMOTION: {
        FbleValue* x = FbleNewIntValue(heap, event.motion.x);
        FbleValue* y = FbleNewIntValue(heap, event.motion.y);
        FbleValue* dx = FbleNewIntValue(heap, event.motion.xrel);
        FbleValue* dy = FbleNewIntValue(heap, event.motion.yrel);
        FbleValue* motion = FbleNewStructValue_(heap, 4, x, y, dx, dy);
        value = FbleNewUnionValue(heap, EVENT_TAGWIDTH, 6, motion);
        break;
      }
    }
  }

  return FbleNewStructValue_(heap, 2, world, value);
}

/**
 * @func[EffectImpl] FbleRunFunction Implementation of @l{App@.effect} function.
 *  See documentation of FbleRunFunction in fble-function.h.
 *  Has fble type @l{(Effect@, World@) { R@<Unit@>; }}.
 *  Applies the given effect to the world.
 */
static FbleValue* EffectImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  App* app = (App*)FbleNativeValueData(function->statics[0]);

  FbleValue* effect = args[0];
  FbleValue* world = args[1];

  switch (FbleUnionValueTag(effect, EFFECT_TAGWIDTH)) {
    case 0: {
      int tick = FbleIntValueAccess(FbleUnionValueArg(effect, EFFECT_TAGWIDTH));

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
      Draw(surface, 1, 1, 0, 0, FbleUnionValueArg(effect, EFFECT_TAGWIDTH));
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
  return FbleNewStructValue_(heap, 2, world, unit);
}

/**
 * @func[OnTimer] Callback called for every timer tick.
 *  @arg[Uint32][interval] The timer interval
 *  @arg[void*][param] unused.
 *
 *  @returns[Uint32]
 *   0 to cancel to the timer.
 *
 *  @sideeffects
 *   Pushes a user event onto the SDL event queue.
 */
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
int FbleAppMain(int argc, const char* argv[], FblePreloadedModule* preloaded)
{
  // To ease debugging of FbleAppMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  // If the module is compiled and '--' isn't present, skip to end of options
  // right away. That way precompiled programs can go straight to application
  // args if they want.
  bool end_of_options = true;
  if (preloaded != NULL) {
    for (int i = 0; i < argc; ++i) {
      if (strcmp(argv[i], "--") == 0) {
        end_of_options = false;
        break;
      }
    }

    if (end_of_options) {
      argc = 1;
    }
  }

  Args app_args = { .fps = false, .driver = NULL };

  FbleProfile* profile = FbleNewProfile(false);
  FbleValueHeap* heap = FbleNewValueHeap();
  FILE* profile_output_file = NULL;
  FbleValue* func = NULL;

  FblePreloadedModuleV builtins;
  FbleInitVector(builtins);
  FbleAppendToVector(builtins, &_Fble_2f_Core_2f_Debug_2f_Builtin_25_);
  FbleAppendToVector(builtins, &_Fble_2f_Core_2f_Stdio_2f_IO_2f_Builtin_25_);

  FbleMainStatus status = FbleMain(&ParseArg, &app_args, "fble-app", fbldUsageHelpText,
      &argc, &argv, preloaded, builtins, heap, profile, &profile_output_file, &func);

  FbleFreeVector(builtins);

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return status;
  }

  if (app_args.driver != NULL) {
    SDL_setenv("SDL_VIDEODRIVER", app_args.driver, 1);
    SDL_setenv("SDL_VIDEO_DRIVER", app_args.driver, 1);
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());

    fprintf(stderr, "Driver options:\n");
    int num_drivers = SDL_GetNumVideoDrivers();
    for (int i = 0; i < num_drivers; ++i) {
      fprintf(stderr, "%i: %s\n", i, SDL_GetVideoDriver(i));
    }

    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return FBLE_MAIN_OTHER_ERROR;
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
    .num_args = 1,
    .num_statics = 0,
    .max_call_args = 0,
    .run = &EventImpl,
  };
  FbleValue* fble_event = FbleNewFuncValue(heap, &event_exe, block_id, NULL);

  FbleExecutable effect_exe = {
    .num_args = 2,
    .num_statics = 1,
    .max_call_args = 0,
    .run = &EffectImpl,
  };

  App app;
  app.window = window;
  app.time = SDL_GetTicks();
  for (size_t i = 0; i < 61; ++i) {
    app.fpsHistogram[i] = 0;
  }

  FbleValue* app_value = FbleNewNativeValue(heap, &app, NULL);
  FbleValue* fble_effect = FbleNewFuncValue(heap, &effect_exe, block_id + 1, &app_value);

  FbleValue* argS = FbleNewEnumValue(heap, LIST_TAGWIDTH, 1);
  argc = 0;
  while (argv[argc] != NULL) {
    argc++;
  }

  for (size_t i = 0; i < argc; ++i) {
    FbleValue* argValue = FbleNewStringValue(heap, argv[argc - i - 1]);
    FbleValue* argP = FbleNewStructValue_(heap, 2, argValue, argS);
    argS = FbleNewUnionValue(heap, LIST_TAGWIDTH, 0, argP);
  }

  FbleValue* fble_width = FbleNewIntValue(heap, width);
  FbleValue* fble_height = FbleNewIntValue(heap, height);

  FbleValue* args[5] = {
    fble_event, fble_effect, fble_width, fble_height, argS
  };
  FbleValue* computation = FbleApply(heap, func, 5, args, profile);

  if (computation == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return FBLE_MAIN_RUNTIME_ERROR;
  }

  // computation has type IO@<Bool@>, which is (World@) { R@<Bool@>; }
  FbleValue* world = FbleNewStructValue_(heap, 0);
  FbleValue* result = FbleApply(heap, computation, 1, &world, profile);

  if (app_args.fps) {
    fprintf(stderr, "FPS Histogram:\n");
    for (size_t i = 0; i < 61; ++i) {
      if (app.fpsHistogram[i] > 0) {
        printf("  % 3zi: % 12i\n", i, app.fpsHistogram[i]);
      }
    }
  }

  FbleMainStatus exit_status = FBLE_MAIN_OTHER_ERROR;
  if (result != NULL) {
    exit_status = (FbleUnionValueTag(FbleStructValueField(result, RESULT_FIELDC, 1), BOOL_TAGWIDTH) == 0) ? FBLE_MAIN_SUCCESS : FBLE_MAIN_FAILURE;
  }

  FbleFreeValueHeap(heap);

  FbleOutputProfile(profile_output_file, profile);
  FbleFreeProfile(profile);

  SDL_GL_DeleteContext(glctx);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return exit_status;
}
