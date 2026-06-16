/**
 * @file interpret.c
 *  Interpreter for FbleCode fble bytecode.
 */

#include "interpret.h"

#include <assert.h>   // for assert
#include <stdlib.h>   // for rand
#include <string.h>   // for memcpy

#include <fble/fble-alloc.h>    // for FbleAlloc, FbleFree
#include <fble/fble-function.h> // For FbleFunction, etc.
#include <fble/fble-runtime.h>  // for FbleNativeValueData, etc.
#include <fble/fble-vector.h>   // for FbleInitVector, etc.

#include "code.h"
#include "unreachable.h"

static void ReportRuntimeError(FbleValueHeap* heap, FbleLoc loc, FbleFunction* func, const char* msg);
static void FreeCode(void* code);

/**
 * @func[GET] Gets the value of a variable in scope.
 *  Assumes existance of a local vars variable:
 *
 *  @code[c] @
 *   FbleValue** vars[3];
 *   vars[FBLE_STATIC_VAR] = statics;
 *   vars[FBLE_ARG_VAR] = args;
 *   vars[FBLE_LOCAL_VAR] = locals;
 *
 *  @arg[FbleVar][var] The index of the variable.
 *
 *  @returns[FbleValue*]
 *   The value of the variable.
 *
 *  @sideeffects
 *   None.
 */
#define GET(var) (vars[var.tag][var.index])

/**
 * @func[ReportRuntimeError] Reports a runtime error.
 *  @arg[FbleValueHeap*][heap] The current heap.
 *  @arg[FbleLoc][loc] Location of the error.
 *  @arg[FbleFunction*][func] Function where the error occured.
 *  @arg[const char*][msg] Error message. Maybe be NULL.
 *  @sideeffects Outputs an error message.
 */
static void ReportRuntimeError(FbleValueHeap* heap, FbleLoc loc, FbleFunction* func, const char* msg)
{
  FbleName func_name = heap->profile->blocks.xs[func->profile_block_id];
  FbleReportError("in %s", loc, func_name.name->str);
  if (msg != NULL) {
    fprintf(stderr, ": %s", msg);
  }
  fprintf(stderr, "\n");
}

/**
 * @func[FreeCode] Calls FbleFreeCode.
 *  @arg[void*][code] The code to free.
 *  @sideeffects
 *   Calls FbleFreeCode on the given code.
 */
static void FreeCode(void* data)
{
  FbleFreeCode((FbleCode*)data);
}

// FbleRunFunction for running interpreted code.
// See documentation for FbleRunFunction in fble-function.h.
static FbleValue* Interpret(
    FbleValueHeap* heap,
    FbleProfileThread* profile,
    FbleFunction* function,
    FbleValue** args)
{
  size_t num_statics = function->executable.num_statics;
  FbleCode* code = (FbleCode*)FbleNativeValueData(function->statics[num_statics - 1]);
  FbleInstr** instrs = code->instrs.xs;
  FbleValue* locals[code->num_locals];

  FbleValue** vars[3];
  vars[FBLE_STATIC_VAR] = function->statics;
  vars[FBLE_ARG_VAR] = args;
  vars[FBLE_LOCAL_VAR] = locals;

  FbleBlockId profile_block_id = function->profile_block_id;

  size_t pc = 0;
  while (true) {
    FbleInstr* instr = instrs[pc];

    if (profile) {
      for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
        switch (op->tag) {
          case FBLE_PROFILE_ENTER_OP:
            FbleProfileEnterBlock(profile, profile_block_id + op->arg);
            break;

          case FBLE_PROFILE_REPLACE_OP:
            FbleProfileReplaceBlock(profile, profile_block_id + op->arg);
            break;

          case FBLE_PROFILE_EXIT_OP:
            FbleProfileExitBlock(profile);
            break;

          case FBLE_PROFILE_SAMPLE_OP:
            FbleProfileSample(profile, op->arg);
            break;
        }
      }
    }

    switch (instr->tag) {
      case FBLE_STRUCT_VALUE_INSTR: {
        FbleStructValueInstr* struct_value_instr = (FbleStructValueInstr*)instr;
        size_t argc = struct_value_instr->args.size;
        FbleValue* struct_args[argc];
        for (size_t i = 0; i < argc; ++i) {
          struct_args[i] = GET(struct_value_instr->args.xs[i]);
        }

        locals[struct_value_instr->dest] = FbleNewStructValue(heap, argc, struct_args);
        pc++;
        break;
      }

      case FBLE_UNION_VALUE_INSTR: {
        FbleUnionValueInstr* union_value_instr = (FbleUnionValueInstr*)instr;
        size_t tagwidth = union_value_instr->tagwidth;
        size_t tag = union_value_instr->tag;
        FbleValue* arg = GET(union_value_instr->arg);
        locals[union_value_instr->dest] = FbleNewUnionValue(heap, tagwidth, tag, arg);
        pc++;
        break;
      }

      case FBLE_STRUCT_ACCESS_INSTR: {
        FbleStructAccessInstr* access_instr = (FbleStructAccessInstr*)instr;

        FbleValue* obj = GET(access_instr->obj);
        locals[access_instr->dest] = FbleStructValueField(obj, access_instr->fieldc, access_instr->field);

        if (locals[access_instr->dest] == NULL) {
          ReportRuntimeError(heap, access_instr->loc, function, "undefined struct value access");
          return NULL;
        }

        pc++;
        break;
      }

      case FBLE_UNION_ACCESS_INSTR: {
        FbleUnionAccessInstr* access_instr = (FbleUnionAccessInstr*)instr;

        FbleValue* obj = GET(access_instr->obj);
        locals[access_instr->dest] = FbleUnionValueField(obj, access_instr->tagwidth, access_instr->tag);

        if (locals[access_instr->dest] == NULL) {
          ReportRuntimeError(heap, access_instr->loc, function, "undefined union value access");
          return NULL;
        }

        if (locals[access_instr->dest] == FbleWrongUnionTag) {
          locals[access_instr->dest] = NULL;
          ReportRuntimeError(heap, access_instr->loc, function, "union field access undefined: wrong tag");
          return NULL;
        }

        pc++;
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
        FbleValue* obj = GET(select_instr->condition);
        size_t tag = FbleUnionValueTag(obj, select_instr->tagwidth);

        if (tag == (size_t)(-1)) {
          ReportRuntimeError(heap, select_instr->loc, function, "undefined union value select");
          return NULL;
        }

        // Binary search for the matching tag.
        assert(select_instr->targets.size > 0);
        size_t target = select_instr->default_;
        size_t lo = 0;
        size_t hi = select_instr->targets.size;
        while (lo < hi) {
          size_t mid = (lo + hi)/2;
          size_t mid_tag = select_instr->targets.xs[mid].tag;
          if (tag == mid_tag) {
            target = select_instr->targets.xs[mid].target;
            break;
          }
          if (tag < mid_tag) {
            hi = mid;
          } else {
            lo = mid + 1;
          }
        }

        pc = target;
        break;
      }

      case FBLE_GOTO_INSTR: {
        FbleGotoInstr* goto_instr = (FbleGotoInstr*)instr;
        pc = goto_instr->target;
        break;
      }

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* func_value_instr = (FbleFuncValueInstr*)instr;
        FbleValue* func_statics[func_value_instr->scope.size];
        for (size_t i = 0; i < func_value_instr->scope.size; ++i) {
          func_statics[i] = GET(func_value_instr->scope.xs[i]);
        }

        locals[func_value_instr->dest] = FbleNewInterpretedFuncValue(heap, func_value_instr->code, profile_block_id + func_value_instr->profile_block_offset, func_statics);
        pc++;
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* call_instr = (FbleCallInstr*)instr;
        FbleValue* func = GET(call_instr->func);
        FbleValue* call_args[call_instr->args.size];
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          call_args[i] = GET(call_instr->args.xs[i]);
        }

        locals[call_instr->dest] = FbleCall(heap, profile, func, call_instr->args.size, call_args);
        if (locals[call_instr->dest] == NULL) {
          ReportRuntimeError(heap, call_instr->loc, function, NULL);
          return NULL;
        }

        pc++;
        break;
      }

      case FBLE_TAIL_CALL_INSTR: {
        FbleTailCallInstr* call_instr = (FbleTailCallInstr*)instr;
        FbleValue* func = GET(call_instr->func);
        if (func == NULL || ((uintptr_t)func & 0x3) == 0x2) {
          ReportRuntimeError(heap, call_instr->loc, function, "called undefined function");
          return NULL;
        };

        heap->tail_call_argc = call_instr->args.size;
        heap->tail_call_buffer[0] = func;
        for (size_t i = 0; i < call_instr->args.size; ++i) {
          heap->tail_call_buffer[i+1] = GET(call_instr->args.xs[i]);
        }

        return heap->tail_call_sentinel;
      }

      case FBLE_COPY_INSTR: {
        FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
        locals[copy_instr->dest] = GET(copy_instr->source);
        pc++;
        break;
      }

      case FBLE_REC_DECL_INSTR: {
        FbleRecDeclInstr* decl_instr = (FbleRecDeclInstr*)instr;
        locals[decl_instr->dest] = FbleDeclareRecursiveValues(heap, decl_instr->n);
        pc++;
        break;
      }

      case FBLE_REC_DEFN_INSTR: {
        FbleRecDefnInstr* defn_instr = (FbleRecDefnInstr*)instr;
        FbleValue* decl = locals[defn_instr->decl];
        FbleValue* defn = locals[defn_instr->defn];
        size_t r = FbleDefineRecursiveValues(heap, decl, defn);

        if (r != 0) {
          ReportRuntimeError(heap, defn_instr->locs.xs[r-1], function, "vacuous value");
          return NULL;
        }

        pc++;
        break;
      }

      case FBLE_RETURN_INSTR: {
        FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
        return GET(return_instr->result);
      }

      case FBLE_TYPE_INSTR: {
        FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
        locals[type_instr->dest] = FbleGenericTypeValue;
        pc++;
        break;
      }

      case FBLE_LIST_INSTR: {
        FbleListInstr* list_instr = (FbleListInstr*)instr;
        size_t argc = list_instr->args.size;
        FbleValue* list_args[argc];
        for (size_t i = 0; i < argc; ++i) {
          list_args[i] = GET(list_instr->args.xs[i]);
        }

        locals[list_instr->dest] = FbleNewListValue(heap, argc, list_args);
        pc++;
        break;
      }

      case FBLE_LITERAL_INSTR: {
        FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
        locals[literal_instr->dest] = FbleNewLiteralValue(heap, literal_instr->literal.size, literal_instr->literal.data);
        pc++;
        break;
      }

      case FBLE_FOREIGN_VALUE_INSTR: {
        FbleForeignValueInstr* foreign_instr = (FbleForeignValueInstr*)instr;
        FbleForeign* foreign = FbleLookupForeignValue(heap, foreign_instr->path, foreign_instr->name.name->str);
        if (foreign == NULL) {
          ReportRuntimeError(heap, foreign_instr->name.loc, function, "foreign value not found");
          return NULL;
        }

        FbleValue* value = FbleNewForeignValue(heap, profile, foreign, profile_block_id + foreign_instr->profile_block_offset);
        locals[foreign_instr->dest] = value;
        pc++;
        break;
      }

      case FBLE_NOP_INSTR: {
        pc++;
        break;
      }
    }
  }
}

// See documentation in interpret.h
FbleValue* FbleNewInterpretedFuncValue(FbleValueHeap* heap, FbleCode* code, size_t profile_block_id, FbleValue** statics)
{
  // Add an extra static to store the FbleCode for access in the interpreter.
  FbleExecutable exe = {
    .num_args = code->executable.num_args,
    .num_statics = code->executable.num_statics + 1,
    .max_call_args = code->executable.max_call_args,
    .run = &Interpret
  };
  FbleValue* nstatics[exe.num_statics];
  memcpy(nstatics, statics, code->executable.num_statics * sizeof(FbleValue*));

  code->refcount++;
  FbleValue* vcode = FbleNewNativeValue(heap, code, &FreeCode);
  nstatics[exe.num_statics - 1 ] = vcode;

  return FbleNewFuncValue(heap, &exe, profile_block_id, nstatics);
}
