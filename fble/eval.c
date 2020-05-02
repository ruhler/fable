// eval.c --
//   This file implements the fble evaluation routines.

#include <assert.h>   // for assert
#include <stdio.h>    // for fprintf, stderr
#include <stdlib.h>   // for NULL, abort, rand
#include <string.h>   // for memset

#include "fble.h"
#include "instr.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// TIME_SLICE --
//   The number of instructions a thread is allowed to run before switching to
//   another thread. Also used as the profiling sample period in number of
//   instructions executed.
#define TIME_SLICE 1024

static FbleValue* FrameGet(FbleValue** statics, FbleValue** locals, FbleFrameIndex index);
static FbleValue* FrameTaggedGet(FbleValueTag tag, FbleValue** statics, FbleValue** locals, FbleFrameIndex index);

static void Cleanup(FbleValueHeap* heap, FbleThunkValue* thunk, FbleValue** locals, size_t localc);
static FbleValue* Run(FbleValueHeap* heap, FbleIO* io, FbleThunkValue* thunk, FbleProfileThread* profile);
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleThunkValue* thunk, FbleProfile* profile);

// FrameGet --
//   Look up a value by frame index.
//
// Inputs:
//   statics - static variables in the frame.
//   locals - local variables in the frame.
//   index - the index of the value to access.
//
// Results:
//   The value at the given index.
//
// Side effects:
//   None.
static FbleValue* FrameGet(FbleValue** statics, FbleValue** locals, FbleFrameIndex index)
{
  switch (index.section) {
    case FBLE_STATICS_FRAME_SECTION: return statics[index.index];
    case FBLE_LOCALS_FRAME_SECTION: return locals[index.index];
  }
  UNREACHABLE("should never get here");
}

// FrameTaggedGet --
//   Get and dereference a value by frame index.
//
//   Dereferences the data value, removing all layers of reference values
//   until a non-reference value is encountered and returns the non-reference
//   value.
//
//   A tag for the type of dereferenced value should be provided. This
//   function will assert that the correct kind of value is encountered.
//
// Inputs:
//   tag - the expected tag of the value.
//   statics - static variables in the frame.
//   locals - local variables in the frame.
//   index - the location of the value in the frame.
//
// Results:
//   The dereferenced value. Returns null in case of abstract value
//   dereference.
//
// Side effects:
//   The returned value will only stay alive as long as the original value on
//   the stack frame.
static FbleValue* FrameTaggedGet(FbleValueTag tag, FbleValue** statics, FbleValue** locals, FbleFrameIndex index)
{
  FbleValue* original = FrameGet(statics, locals, index);
  FbleValue* value = original;
  while (value->tag == FBLE_REF_VALUE) {
    FbleRefValue* rv = (FbleRefValue*)value;

    if (rv->value == NULL) {
      // We are trying to dereference an abstract value. This is undefined
      // behavior according to the spec. Return NULL to indicate to the
      // caller.
      return NULL;
    }

    value = rv->value;
  }
  assert(value->tag == tag);
  return value;
}

// Cleanup --
//   Helper function for cleaning up Run.
//
// Inputs:
//   heap - the heap to use for allocations.
//   thunk - the thunk to free.
//   locals - the locals to release.
//   localc - the size of locals
//
// Side effects:
//   Releases the thunk and all locals.
static void Cleanup(FbleValueHeap* heap, FbleThunkValue* thunk, FbleValue** locals, size_t localc)
{
  FbleValueRelease(heap, &thunk->_base);
  for (size_t i = 0; i < localc; ++i) {
    FbleValueRelease(heap, locals[i]);
  }
}

// Run --
//   Evaluate the given thunk.
//
// Inputs:
//   heap - the value heap.
//   io - io to use for external ports.
//   thunk - the thunk to evaluate.
//   profile - the current profiling thread.
//
// Results:
//   The result of evaluating the thunk, or NULL in case of error.
//
// Side effects:
//   Evaluates the thunk, which may result in i/o or port activity.
//   Takes ownership of thunk.
//   Updates profiling information based on evaluation of the thunk.
//   Prints a message to stderr in case of error.
static FbleValue* Run(FbleValueHeap* heap, FbleIO* io, FbleThunkValue* thunk, FbleProfileThread* profile)
{
  FbleArena* arena = heap->arena;

  while (true) {
    assert(thunk->args_needed == 0);

    size_t argc = 0;
    FbleThunkValue* t = thunk;
    while (t->tag == FBLE_APP_THUNK_VALUE) {
      argc++;
      t = ((FbleAppThunkValue*)t)->func;
    }

    assert(t->tag == FBLE_CODE_THUNK_VALUE);
    FbleCodeThunkValue* code_thunk = (FbleCodeThunkValue*)t;

    size_t localc = code_thunk->code->locals;
    FbleValue** statics = code_thunk->scope;
    FbleInstr** instrs = code_thunk->code->instrs.xs;
    FbleValue* locals[localc];
    memset(locals, 0, localc * sizeof(FbleValue*));

    t = thunk;
    while (t->tag == FBLE_APP_THUNK_VALUE) {
      FbleAppThunkValue* app = (FbleAppThunkValue*)t;
      locals[--argc] = FbleValueRetain(heap, app->arg);
      t = app->func;
    }

    bool tail_call = false;
    size_t pc = 0;
    do {
      FbleInstr* instr = instrs[pc++];
      if (rand() % TIME_SLICE == 0) {
        // Don't count profiling instructions against the profile time. The user
        // doesn't care about those.
        if (instr->tag != FBLE_PROFILE_INSTR) {
          FbleProfileSample(arena, profile, 1);
        }

        // TODO: Yield here and check for abort.
      }

      switch (instr->tag) {
        case FBLE_STRUCT_VALUE_INSTR: {
          FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
          size_t argc = struct_value_instr->args.size;
          FbleValue* argv[argc];
          for (size_t i = 0; i < argc; ++i) {
            FbleValue* arg = FrameGet(statics, locals, struct_value_instr->args.xs[i]);
            argv[i] = FbleValueRetain(heap, arg);
          }

          FbleValueV args = { .size = argc, .xs = argv, };
          locals[struct_value_instr->dest] = FbleNewStructValue(heap, args);
          break;
        }

        case FBLE_UNION_VALUE_INSTR: {
          FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
          FbleValue* arg = FrameGet(statics, locals, union_value_instr->arg);
          locals[union_value_instr->dest] = FbleNewUnionValue(heap, union_value_instr->tag, FbleValueRetain(heap, arg));
          break;
        }

        case FBLE_STRUCT_ACCESS_INSTR: {
          FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

          FbleStructValue* sv = (FbleStructValue*)FrameTaggedGet(FBLE_STRUCT_VALUE, statics, locals, access_instr->obj);
          if (sv == NULL) {
            FbleReportError("undefined struct value access\n", &access_instr->loc);
            Cleanup(heap, thunk, locals, localc);
            return NULL;
          }
          assert(access_instr->tag < sv->fieldc);
          locals[access_instr->dest] = FbleValueRetain(heap, sv->fields[access_instr->tag]);
          break;
        }

        case FBLE_UNION_ACCESS_INSTR: {
          FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;

          FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, statics, locals, access_instr->obj);
          if (uv == NULL) {
            FbleReportError("undefined union value access\n", &access_instr->loc);
            Cleanup(heap, thunk, locals, localc);
            return NULL;
          }

          if (uv->tag != access_instr->tag) {
            FbleReportError("union field access undefined: wrong tag\n", &access_instr->loc);
            Cleanup(heap, thunk, locals, localc);
            return NULL;
          }

          locals[access_instr->dest] = FbleValueRetain(heap, uv->arg);
          break;
        }

        case FBLE_UNION_SELECT_INSTR: {
          FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
          FbleUnionValue* uv = (FbleUnionValue*)FrameTaggedGet(FBLE_UNION_VALUE, statics, locals, select_instr->condition);
          if (uv == NULL) {
            FbleReportError("undefined union value select\n", &select_instr->loc);
            Cleanup(heap, thunk, locals, localc);
            return NULL;
          }
          pc += uv->tag;
          break;
        }

        case FBLE_GOTO_INSTR: {
          FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
          pc = goto_instr->pc;
          break;
        }

        case FBLE_FUNC_VALUE_INSTR: {
          FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
          size_t scopec = func_value_instr->code->statics;

          FbleCodeThunkValue* value = FbleNewValueExtra(
              heap, FbleCodeThunkValue,
              sizeof(FbleValue*) * scopec);
          value->_base._base.tag = FBLE_THUNK_VALUE;
          value->_base.tag = FBLE_CODE_THUNK_VALUE;
          value->_base.args_needed = func_value_instr->argc;
          value->code = func_value_instr->code;
          value->code->refcount++;
          for (size_t i = 0; i < scopec; ++i) {
            FbleValue* arg = FrameGet(statics, locals, func_value_instr->scope.xs[i]);
            value->scope[i] = arg;
            FbleValueAddRef(heap, &value->_base._base, arg);
          }
          locals[func_value_instr->dest] = &value->_base._base;
          break;
        }

        case FBLE_RELEASE_INSTR: {
          FbleReleaseInstr* release = (FbleReleaseInstr*)instr;
          assert(locals[release->value] != NULL);
          FbleValueRelease(heap, locals[release->value]);
          locals[release->value] = NULL;
          break;
        }

        case FBLE_FUNC_APPLY_INSTR: {
          FbleFuncApplyInstr* func_apply_instr = (FbleFuncApplyInstr*)instr;
          FbleThunkValue* func = (FbleThunkValue*)FrameTaggedGet(FBLE_THUNK_VALUE, statics, locals, func_apply_instr->func);
          if (func == NULL) {
            FbleReportError("undefined function value apply\n", &func_apply_instr->loc);
            Cleanup(heap, thunk, locals, localc);
            return NULL;
          };

          FbleAppThunkValue* value = FbleNewValue(heap, FbleAppThunkValue);
          value->_base._base.tag = FBLE_THUNK_VALUE;
          value->_base.tag = FBLE_APP_THUNK_VALUE;
          value->_base.args_needed = func->args_needed - 1;
          value->func = func;
          FbleValueAddRef(heap, &value->_base._base, &value->func->_base);
          value->arg = FrameGet(statics, locals, func_apply_instr->arg);
          FbleValueAddRef(heap, &value->_base._base, value->arg);

          if (value->_base.args_needed > 0) {
            if (func_apply_instr->exit) {
              Cleanup(heap, thunk, locals, localc);
              return &value->_base._base;
            } else {
              locals[func_apply_instr->dest] = &value->_base._base;
            }
          } else {
            if (func_apply_instr->exit) {
              Cleanup(heap, thunk, locals, localc);
              thunk = &value->_base;
              tail_call = true;
              break;
            } else {
              locals[func_apply_instr->dest] = Run(heap, io, &value->_base, profile);
              if (locals[func_apply_instr->dest] == NULL) {
                Cleanup(heap, thunk, locals, localc);
                return NULL;
              }
            }
          }
          break;
        }

        case FBLE_PROC_VALUE_INSTR: {
          FbleProcValueInstr* proc_value_instr = (FbleProcValueInstr*)instr;
          size_t scopec = proc_value_instr->code->statics;

          FbleCodeThunkValue* value = FbleNewValueExtra(heap, FbleCodeThunkValue, sizeof(FbleValue*) * scopec);
          value->_base._base.tag = FBLE_THUNK_VALUE;
          value->_base.tag = FBLE_CODE_THUNK_VALUE;
          value->_base.args_needed = 0;
          value->code = proc_value_instr->code;
          value->code->refcount++;
          for (size_t i = 0; i < scopec; ++i) {
            FbleValue* arg = FrameGet(statics, locals, proc_value_instr->scope.xs[i]);
            value->scope[i] = arg;
            FbleValueAddRef(heap, &value->_base._base, arg);
          }
          locals[proc_value_instr->dest] = &value->_base._base;
          break;
        }

        case FBLE_COPY_INSTR: {
          FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
          FbleValue* value = FrameGet(statics, locals, copy_instr->source);
          locals[copy_instr->dest] = FbleValueRetain(heap, value);
          break;
        }

        case FBLE_GET_INSTR: {
          FbleGetInstr* get_instr = (FbleGetInstr*)instr;
          FbleValue* get_port = FrameGet(statics, locals, get_instr->port);
          if (get_port->tag == FBLE_LINK_VALUE) {
            FbleLinkValue* link = (FbleLinkValue*)get_port;

            if (link->head == NULL) {
              // Blocked on get.
              // TODO: Yield here and check for abort.
              fprintf(stderr, "BLOCKED ON LINK GET\n");
              Cleanup(heap, thunk, locals, localc);
              return NULL;
            }

            FbleValues* head = link->head;
            link->head = link->head->next;
            if (link->head == NULL) {
              link->tail = NULL;
            }

            locals[get_instr->dest] = FbleValueRetain(heap, head->value);
            FbleValueDelRef(heap, &link->_base, head->value);
            FbleFree(arena, head);
            break;
          }

          if (get_port->tag == FBLE_PORT_VALUE) {
            FblePortValue* port = (FblePortValue*)get_port;
            if (*port->data == NULL) {
              // Blocked on get.
              // TODO: Handle this properly.
              io->io(io, heap, true);
              if (*port->data == NULL) {
                fprintf(stderr, "BLOCKED ON IO GET\n");
                Cleanup(heap, thunk, locals, localc);
                return NULL;
              }
            }

            locals[get_instr->dest] = *port->data;
            *port->data = NULL;
            break;
          }

          UNREACHABLE("get port must be an input or port value");
          break;
        }

        case FBLE_PUT_INSTR: {
          FblePutInstr* put_instr = (FblePutInstr*)instr;
          FbleValue* put_port = FrameGet(statics, locals, put_instr->port);
          FbleValue* arg = FrameGet(statics, locals, put_instr->arg);

          FbleValueV args = { .size = 0, .xs = NULL, };
          FbleValue* unit = FbleNewStructValue(heap, args);

          if (put_port->tag == FBLE_LINK_VALUE) {
            FbleLinkValue* link = (FbleLinkValue*)put_port;

            FbleValues* tail = FbleAlloc(arena, FbleValues);
            tail->value = arg;
            tail->next = NULL;

            if (link->head == NULL) {
              link->head = tail;
              link->tail = tail;
            } else {
              assert(link->tail != NULL);
              link->tail->next = tail;
              link->tail = tail;
            }

            FbleValueAddRef(heap, &link->_base, tail->value);
            locals[put_instr->dest] = unit;
            break;
          }

          if (put_port->tag == FBLE_PORT_VALUE) {
            FblePortValue* port = (FblePortValue*)put_port;

            if (*port->data != NULL) {
              // Blocked on put.
              // TODO: Handle this properly.
              io->io(io, heap, true);
              if (*port->data != NULL) {
                fprintf(stderr, "BLOCKED ON IO PUT\n");
                Cleanup(heap, thunk, locals, localc);
                return NULL;
              }
            }

            *port->data = FbleValueRetain(heap, arg);
            locals[put_instr->dest] = unit;
            break;
          }

          UNREACHABLE("put port must be an output or port value");
          break;
        }

        case FBLE_LINK_INSTR: {
          FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

          FbleLinkValue* link = FbleNewValue(heap, FbleLinkValue);
          link->_base.tag = FBLE_LINK_VALUE;
          link->head = NULL;
          link->tail = NULL;

          FbleValue* get = FbleNewGetValue(heap, &link->_base);
          FbleValue* put = FbleNewPutValue(heap, &link->_base);
          FbleValueRelease(heap, &link->_base);

          locals[link_instr->get] = get;
          locals[link_instr->put] = put;
          break;
        }

        case FBLE_FORK_INSTR: {
          FbleForkInstr* fork_instr = (FbleForkInstr*)instr;

          for (size_t i = 0; i < fork_instr->args.size; ++i) {
            FbleThunkValue* arg = (FbleThunkValue*)FrameTaggedGet(FBLE_THUNK_VALUE, statics, locals, fork_instr->args.xs[i]);

            // You cannot execute a proc in a let binding, so it should be
            // impossible to ever have an undefined proc value.
            assert(arg != NULL && "undefined proc value");

            FbleValueRetain(heap, &arg->_base);

            // TODO: Add real support for forking.
            // For now, execute the children serially instead of in parallel and
            // hope for the best.
            locals[fork_instr->dests.xs[i]] = Run(heap, io, arg, profile);
            if (locals[fork_instr->dests.xs[i]] == NULL) {
              Cleanup(heap, thunk, locals, localc);
              return NULL;
            }
          }

          break;
        }

        case FBLE_PROC_INSTR: {
          FbleProcInstr* proc_instr = (FbleProcInstr*)instr;
          FbleThunkValue* proc = (FbleThunkValue*)FrameTaggedGet(FBLE_THUNK_VALUE, statics, locals, proc_instr->proc);

          // You cannot execute a proc in a let binding, so it should be
          // impossible to ever have an undefined proc value.
          assert(proc != NULL && "undefined proc value");
          FbleValueRetain(heap, &proc->_base);

          if (proc_instr->exit) {
            // TODO: Make this a proper tail call!
            Cleanup(heap, thunk, locals, localc);
            thunk = proc;
            tail_call = true;
            break;
          } else {
            locals[proc_instr->dest] = Run(heap, io, proc, profile);
            if (locals[proc_instr->dest] == NULL) {
              Cleanup(heap, thunk, locals, localc);
              return NULL;
            }
          }
          break;
        }

        case FBLE_REF_VALUE_INSTR: {
          FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
          FbleRefValue* rv = FbleNewValue(heap, FbleRefValue);
          rv->_base.tag = FBLE_REF_VALUE;
          rv->value = NULL;

          locals[ref_instr->dest] = &rv->_base;
          break;
        }

        case FBLE_REF_DEF_INSTR: {
          FbleRefDefInstr* ref_def_instr = (FbleRefDefInstr*)instr;
          FbleRefValue* rv = (FbleRefValue*)locals[ref_def_instr->ref];
          assert(rv->_base.tag == FBLE_REF_VALUE);

          FbleValue* value = FrameGet(statics, locals, ref_def_instr->value);
          assert(value != NULL);
          rv->value = value;
          FbleValueAddRef(heap, &rv->_base, rv->value);
          break;
        }

        case FBLE_RETURN_INSTR: {
          FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
          FbleValue* result = FrameGet(statics, locals, return_instr->result);
          FbleValueRetain(heap, result);
          Cleanup(heap, thunk, locals, localc);
          return result;
        }

        case FBLE_TYPE_INSTR: {
          FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
          FbleTypeValue* value = FbleNewValue(heap, FbleTypeValue);
          value->_base.tag = FBLE_TYPE_VALUE;
          locals[type_instr->dest] = &value->_base;
          break;
        }

        case FBLE_PROFILE_INSTR: {
          FbleProfileInstr* profile_instr = (FbleProfileInstr*)instr;
          switch (profile_instr->op) {
            case FBLE_PROFILE_ENTER_OP:
              FbleProfileEnterBlock(arena, profile, profile_instr->data.enter.block);
              break;

            case FBLE_PROFILE_EXIT_OP:
              FbleProfileExitBlock(arena, profile);
              break;

            case FBLE_PROFILE_AUTO_EXIT_OP: {
              FbleProfileAutoExitBlock(arena, profile);
              break;
            }

            case FBLE_PROFILE_FUNC_EXIT_OP: {
              FbleThunkValue* func = (FbleThunkValue*)FrameTaggedGet(FBLE_THUNK_VALUE, statics, locals, profile_instr->data.func_exit.func);
              if (func == NULL) {
                FbleReportError("undefined function value apply\n", &profile_instr->data.func_exit.loc);
                Cleanup(heap, thunk, locals, localc);
                return NULL;
              };

              if (func->args_needed > 1) {
                FbleProfileExitBlock(arena, profile);
              } else {
                FbleProfileAutoExitBlock(arena, profile);
              }
              break;
            }
          }

          break;
        }
      }
    } while (!tail_call);
  }

  UNREACHABLE("should never get here");
  return NULL;
}

// Eval --
//   Execute the given sequence of instructions to completion.
//
// Inputs:
//   heap - the value heap.
//   io - io to use for external ports.
//   thunk - the thunk to evaluate.
//   profile - profile to update with execution stats.
//
// Results:
//   The computed value, or NULL on error.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints a message to stderr in case of error.
//   Updates profile based on the execution.
//   Takes ownership of thunk.
static FbleValue* Eval(FbleValueHeap* heap, FbleIO* io, FbleThunkValue* thunk, FbleProfile* profile)
{
  FbleArena* arena = heap->arena;
  FbleProfileThread* thread = FbleNewProfileThread(arena, profile);
  FbleValue* result = Run(heap, io, thunk, thread);
  FbleFreeProfileThread(arena, thread);
  return result;
}

// FbleEval -- see documentation in fble.h
FbleValue* FbleEval(FbleValueHeap* heap, FbleProgram* program, FbleProfile* profile)
{
  FbleArena* arena = heap->arena;
  FbleInstrBlock* code = FbleCompile(arena, profile, program);
  if (code == NULL) {
    return NULL;
  }

  FbleCodeThunkValue* thunk = FbleNewValue(heap, FbleCodeThunkValue);
  thunk->_base._base.tag = FBLE_THUNK_VALUE;
  thunk->_base.tag = FBLE_CODE_THUNK_VALUE;
  thunk->_base.args_needed = 0;
  thunk->code = code;

  FbleIO io = { .io = &FbleNoIO };
  FbleValue* result = Eval(heap, &io, &thunk->_base, profile);
  return result;
}

// FbleApply -- see documentation in fble.h
FbleValue* FbleApply(FbleValueHeap* heap, FbleValue* func, FbleValueV args, FbleProfile* profile)
{
  FbleIO io = { .io = &FbleNoIO, };
  FbleValueRetain(heap, func);

  assert(func->tag == FBLE_THUNK_VALUE);
  FbleThunkValue* thunk = (FbleThunkValue*)func;

  for (size_t i = 0; i < args.size; ++i) {
    if (thunk->args_needed == 0) {
      FbleValue* result = Eval(heap, &io, thunk, profile);
      if (result == NULL) {
        return NULL;
      }

      assert(result->tag == FBLE_THUNK_VALUE && "too many args to apply");
      thunk = (FbleThunkValue*) result;
    }

    FbleAppThunkValue* app = FbleNewValue(heap, FbleAppThunkValue);
    app->_base._base.tag = FBLE_THUNK_VALUE;
    app->_base.tag = FBLE_APP_THUNK_VALUE;
    app->_base.args_needed = thunk->args_needed - 1;
    app->func = thunk;
    FbleValueAddRef(heap, &app->_base._base, &app->func->_base);
    app->arg = args.xs[i];
    FbleValueAddRef(heap, &app->_base._base, app->arg);

    FbleValueRelease(heap, &thunk->_base);
    thunk = &app->_base;
  }

  if (thunk->args_needed == 0) {
    return Eval(heap, &io, thunk, profile);
  }
  return &thunk->_base;
}

// FbleNoIO -- See documentation in fble.h
bool FbleNoIO(FbleIO* io, FbleValueHeap* heap, bool block)
{
  return false;
}

// FbleExec -- see documentation in fble.h
FbleValue* FbleExec(FbleValueHeap* heap, FbleIO* io, FbleValue* proc, FbleProfile* profile)
{
  FbleValueRetain(heap, proc);
  assert(proc->tag == FBLE_THUNK_VALUE);
  return Eval(heap, io, (FbleThunkValue*)proc, profile);
}
