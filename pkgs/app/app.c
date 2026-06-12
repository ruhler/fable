/**
 * @file app.c
 *  Implementation of FbleAppMain function.
 */

#include "app.h"

#include <assert.h>       // for assert
#include <locale.h>       // for setlocale, LC_CTYPE
#include <string.h>       // for strcmp

#include <SDL.h>          // for SDL_*
#include <GL/gl.h>        // for gl*

#include <fble/fble-alloc.h>       // for FbleFree.
#include <fble/fble-arg-parse.h>   // for FbleParseBoolArg, etc.
#include <fble/fble-main.h>        // for FbleMain.
#include <fble/fble-value.h>       // for FbleValue, etc.
#include <fble/fble-vector.h>      // for FbleInitVector, etc.
#include <fble/fble-version.h>     // for FblePrintVersion

#include "data.fble.h"             // for FbleCharValueAccess, etc.
#include "debug.fble.h"            // for /Std/Stream/Debug% FFI
#include "env.fble.h"              // for /Std/Io/Env%.GetEnv
#include "io.fble.h"               // for FbleIoM
#include "stdio.fble.h"            // for /Std/Io/File/Internal%
#include "cli.fble.h"              // for FbleCliArgs, etc.

#include "fble-app.usage.h"        // for fbldUsageHelpText

#define DRAWING_TAGWIDTH 3
#define POINT_FIELDC 2
#define COLOR_FIELDC 3
#define TRIANGLE_FIELDC 4
#define RECT_FIELDC 5
#define TRANSFORMED_FIELDC 3
#define OVER_FIELDC 2
#define KEY_TAGWIDTH 5
#define BUTTON_TAGWIDTH 1
#define EVENT_TAGWIDTH 3
#define EFFECT_TAGWIDTH 1
#define LIST_TAGWIDTH 1
#define RESULT_FIELDC 2
#define BOOL_TAGWIDTH 1

typedef struct {
  bool jank_stats;
  bool jank_display;
  bool no_video;
} Args;

static bool ParseArg(void* dest, int* argc, const char*** argv, bool* error);

/**
 * @struct[CountV] Vector of size_t
 *  @field[size_t][size] Number of elements.
 *  @field[size_t*][xs] Elements.
 */
typedef struct {
  size_t size;
  size_t* xs;
} CountV; 

/**
 * @struct[App] Application state.
 *  We define jank as any time an app requests a tick event to happen in the
 *  past. The duration of the jank event is how far in the past the tick event
 *  was requested to happen.
 *
 *  @field[SDL_Window*][window] The application window for drawing.
 *  @field[Uint32][time] The application time based on effect ticks.
 *  @field[size_t][ticks] Stats tracking the total number of ticks done.
 *  @field[CountV][janks]
 *   The value at index i is the count of janks of duration between 
 *   2^i and 2^(i+1) milliseconds. Expanded as needed.
 *  @field[size_t][jank_count] The number of jank occurences.
 *  @field[Uint32][jank_total] The sum of all jank durations.
 *  @field[Uint32][jank_max] The max value of all jank durations.
 *  @field[Uint32][jank_running]
 *   A running decaying average tracking cost of recent jank.
 *  @field[bool][jank_display]
 *   If true, highlight jank on display while drawing.
 */
typedef struct {
  SDL_Window* window;
  Uint32 time;

  size_t ticks;
  CountV janks;
  size_t jank_count;
  Uint32 jank_total;
  Uint32 jank_max;
  Uint32 jank_running;
  bool jank_display;
} App;

static void Color(App* app, float r, float g, float b);
static void Draw(App* app, int ax, int ay, int bx, int by, FbleValue* drawing);
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
  if (FbleParseBoolArg("--jank-stats", &args->jank_stats, argc, argv, error)) return true;
  if (FbleParseBoolArg("--jank-display", &args->jank_display, argc, argv, error)) return true;
  if (FbleParseBoolArg("--no-video", &args->no_video, argc, argv, error)) return true;

  return false;
}

/**
 * @func[Color] Set the color to draw.
 *  Adjusts the color to show jank display if requested.
 *  @arg[App*][app] The app context.
 *  @arg[float][r] How much red, in [0, 1]
 *  @arg[float][g] How much green, in [0, 1]
 *  @arg[float][b] How much blue, in [0, 1]
 *  @sideeffects Sets the color to use in GL rendering.
 */
static void Color(App* app, float r, float g, float b)
{
  if (app->jank_display && app->jank_running != 0) {
    float jr = 1.0 - r;
    float jg = 1.0 - g;
    float jb = 1.0 - b;
    float jw = fmin(1.0, app->jank_running / 16.0);
    float nw = 1.0 - jw;
    r = nw*r + jw*jr;
    g = nw*g + jw*jg;
    b = nw*b + jw*jb;
  }
  glColor3f(r, g, b);
}

/**
 * @func[Draw] Draws a drawing to the screen of type @l{/Drawing%.Drawing@}.
 *  @arg[App*][app] The app context.
 *  @arg[int][ax, ay, bx, by]
 *   A transformation to apply to the drawing: a*p + b.
 *  @arg[FbleValue*][drawing] The drawing to draw.
 *
 *  @sideeffects
 *   Draws the drawing to the window. The caller must call
 *   SDL_GL_SwapWindow for the screen to actually be updated.
 */
static void Draw(App* app, int ax, int ay, int bx, int by, FbleValue* drawing)
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
      Color(app, red/256.0, green/256.0, blue/256.0);
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
      Color(app, red/256.0, green/256.0, blue/256.0);
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
      Draw(app, ax * axi, ay * ayi, ax * bxi + bx, ay * byi + by, d);
      return;
    }

    case 4: {
      // Over.
      FbleValue* over = FbleUnionValueArg(drawing, DRAWING_TAGWIDTH);
      Draw(app, ax, ay, bx, by, FbleStructValueField(over, OVER_FIELDC, 0));
      Draw(app, ax, ay, bx, by, FbleStructValueField(over, OVER_FIELDC, 1));
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
    case SDL_SCANCODE_R: k = 7; break;
    case SDL_SCANCODE_S: k = 8; break;
    case SDL_SCANCODE_W: k = 9; break;
    case SDL_SCANCODE_LEFT: k = 10; break;
    case SDL_SCANCODE_RIGHT: k = 11; break;
    case SDL_SCANCODE_UP: k = 12; break;
    case SDL_SCANCODE_DOWN: k = 13; break;
    case SDL_SCANCODE_SPACE: k = 14; break;
    case SDL_SCANCODE_LSHIFT: k = 15; break;
    case SDL_SCANCODE_RSHIFT: k = 16; break;
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
 *  Has fble type @l{(Unit@) { Event@; }}.
 *  Gets the next input event.
 */
static FbleValue* EventImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  while (true) {
    SDL_Event event;
    SDL_WaitEvent(&event);
    switch (event.type) {
      case SDL_USEREVENT: {
        return FbleNewEnumValue(heap, EVENT_TAGWIDTH, 0);
      }

      case SDL_KEYDOWN: {
        FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
        if (key != NULL) {
          return FbleNewUnionValue(heap, EVENT_TAGWIDTH, 1, key);
        }
        break;
      }

      case SDL_KEYUP: {
        FbleValue* key = MakeKey(heap, event.key.keysym.scancode);
        if (key != NULL) {
          return FbleNewUnionValue(heap, EVENT_TAGWIDTH, 2, key);
        }
        break;
      }

      case SDL_MOUSEBUTTONDOWN: {
        FbleValue* button = MakeButton(heap, event.button.button);
        if (button != NULL) {
          FbleValue* x = FbleNewIntValue(heap, event.button.x);
          FbleValue* y = FbleNewIntValue(heap, event.button.y);
          FbleValue* mouse_button = FbleNewStructValue_(heap, 3, button, x, y);
          return FbleNewUnionValue(heap, EVENT_TAGWIDTH, 3, mouse_button);
        }
        break;
      }

      case SDL_MOUSEBUTTONUP: {
        FbleValue* button = MakeButton(heap, event.button.button);
        if (button != NULL) {
          FbleValue* x = FbleNewIntValue(heap, event.button.x);
          FbleValue* y = FbleNewIntValue(heap, event.button.y);
          FbleValue* mouse_button = FbleNewStructValue_(heap, 3, button, x, y);
          return FbleNewUnionValue(heap, EVENT_TAGWIDTH, 4, mouse_button);
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
          return FbleNewUnionValue(heap, EVENT_TAGWIDTH, 5, resized);
        }
        break;
      }

      case SDL_MOUSEMOTION: {
        FbleValue* x = FbleNewIntValue(heap, event.motion.x);
        FbleValue* y = FbleNewIntValue(heap, event.motion.y);
        FbleValue* dx = FbleNewIntValue(heap, event.motion.xrel);
        FbleValue* dy = FbleNewIntValue(heap, event.motion.yrel);
        FbleValue* motion = FbleNewStructValue_(heap, 4, x, y, dx, dy);
        return FbleNewUnionValue(heap, EVENT_TAGWIDTH, 6, motion);
      }
    }
  }

  assert(false && "should never get here");
  return NULL;
}

/**
 * @func[EffectImpl] FbleRunFunction Implementation of @l{App@.effect} function.
 *  See documentation of FbleRunFunction in fble-function.h.
 *  Has fble type @l{(Effect@, Unit@@) { Unit@; }}.
 *  Applies the given effect to the world.
 */
static FbleValue* EffectImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  App* app = (App*)FbleNativeValueData(function->statics[0]);

  FbleValue* effect = args[0];

  switch (FbleUnionValueTag(effect, EFFECT_TAGWIDTH)) {
    case 0: {
      int tick = FbleIntValueAccess(FbleUnionValueArg(effect, EFFECT_TAGWIDTH));

      Uint32 now = SDL_GetTicks();
      if (app->time == 0) {
        app->time = now;
      }

      app->time += tick;
      app->ticks++;
      app->jank_running /= 2;
      if (app->time < now) {
        // Jank! Update jank stats.
        app->jank_count++;
        Uint32 duration = now - app->time;
        if (duration > app->jank_max) {
          app->jank_max = duration;
        }
        app->jank_total += duration;
        app->jank_running += duration;
        size_t bucket = 0;
        duration /= 2;
        while (duration > 0) {
          bucket++;
          duration /= 2;
        }
        while (app->janks.size <= bucket) {
          FbleAppendToVector(app->janks, 0);
        }
        app->janks.xs[bucket]++;

        // Reset app time to catch up.
        app->time = now;
      }
      SDL_AddTimer(app->time - now, OnTimer, NULL);
      break;
    }

    case 1: {
      Draw(app, 1, 1, 0, 0, FbleUnionValueArg(effect, EFFECT_TAGWIDTH));
      SDL_GL_SwapWindow(app->window);
      break;
    }
  }

  return FbleNewStructValue_(heap, 0);
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

// FbleAppMain -- See documentation in app.h
int FbleAppMain(int argc, const char* argv[], FblePreloadedModule* preloaded)
{
  // To ease debugging of FbleAppMain programs, cause the following useful
  // functions to be linked in:
  (void)(FbleCharValueAccess);
  (void)(FbleIntValueAccess);
  (void)(FbleStringValueAccess);

  // Set locale properly before converting command line args into fble land.
  setlocale(LC_CTYPE, "");

  Args app_args = {
    .jank_stats = false,
    .jank_display = false,
    .no_video = false
  };

  FbleProfile* profile = FbleNewProfile();
  FbleValueHeap* heap = FbleNewValueHeap();
  const char* profile_output_file = NULL;
  uint64_t profile_sample_period = 0;
  FbleValue* func = NULL;

  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Stream_2f_Debug_25__2e_PutChar);
  FbleRegisterForeignValue(heap, &_Fble_2f_Std_2f_Io_2f_Env_25__2e_GetVar);
  FbleRegisterStdioForeignValues(heap);

  FbleMainStatus status = FbleMain(&ParseArg, &app_args, "fble-app", fbldUsageHelpText,
      &argc, &argv, preloaded, heap, profile, &profile_output_file, &profile_sample_period, &func);

  bool video = !app_args.no_video;

  if (func == NULL) {
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return FbleCliMainOtherStatus(status);
  }

  // Note: video will be initialized later only if requested.
  if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
    FbleFreeValueHeap(heap);
    FbleFreeProfile(profile);
    return FbleCliMainOtherStatus(FBLE_MAIN_OTHER_ERROR);
  }

  int width = 640;
  int height = 480;

  SDL_Window* window = NULL;
  if (video) {
    window = SDL_CreateWindow(
        "Fble App", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height,
        SDL_WINDOW_OPENGL);
    if (window == NULL) {
      fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
      FbleFreeValueHeap(heap);
      FbleFreeProfile(profile);
      return FbleCliMainOtherStatus(FBLE_MAIN_OTHER_ERROR);
    }
  }

  SDL_SetWindowResizable(window, true);
  SDL_GLContext glctx = SDL_GL_CreateContext(window);
  SDL_GL_SetSwapInterval(1);
  SDL_ShowCursor(SDL_DISABLE);

  SDL_GetWindowSize(window, &width, &height);

  glShadeModel(GL_FLAT);
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
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
  app.time = 0;
  app.ticks = 0;
  FbleInitVector(app.janks);
  app.jank_count = 0;
  app.jank_total = 0;
  app.jank_max = 0;
  app.jank_running = 0;
  app.jank_display = app_args.jank_display;

  FbleValue* app_value = FbleNewNativeValue(heap, &app, NULL);
  FbleValue* fble_effect = FbleNewFuncValue(heap, &effect_exe, block_id + 1, &app_value);

  FbleValue* m = FbleIoMonad(heap, profile);            // Monad@<M@>
  FbleValue* io = FbleIo(heap, profile);                // Io@<M@>
  FbleValue* fble_app = FbleNewStructValue_(heap, 2, fble_event, fble_effect);
  FbleValue* fble_width = FbleNewIntValue(heap, width);
  FbleValue* fble_height = FbleNewIntValue(heap, height);
  FbleValue* args = FbleCliArgs(heap, argc, argv);      // List@<String@>
  FbleValue* unit = FbleNewStructValue_(heap, 0);       // Unit@

  FbleValue* func_args[7] = {
    m, io, fble_app, fble_width, fble_height, args, unit
  };

  FbleValue* result = FbleApply(heap, func, 7, func_args, profile);

  if (app_args.jank_stats) {
    fprintf(stderr, "Jank Stats:\n");
    fprintf(stderr, "  jank count: %zi of %zi\n", app.jank_count, app.ticks); 
    fprintf(stderr, "  jank total: %i\n", app.jank_total);
    fprintf(stderr, "  jank max: %i\n", app.jank_max);
    fprintf(stderr, "  jank count by duration:\n");
    for (size_t i = 0; i < app.janks.size; ++i) {
      fprintf(stderr, "    2^%zi ms: % 12zi\n", i, app.janks.xs[i]);
    }
  }

  FbleFreeVector(app.janks);

  int exit_status = FbleCliMainAppStatus(result);

  FbleFreeValueHeap(heap);

  FbleOutputProfile(profile_output_file, profile, profile_sample_period);
  FbleFreeProfile(profile);

  SDL_GL_DeleteContext(glctx);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return exit_status;
}
