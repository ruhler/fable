// generate_aarch64.c --
//   This file describes code to generate 64-bit ARM code for fble modules.

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <string.h>   // for strlen, strcat

#include "fble-vector.h"    // for FbleVectorInit, etc.

#include "code.h"
#include "fble-compile.h"
#include "tc.h"
#include "value.h"

#define UNREACHABLE(x) assert(false && x)

// LabelId --
//   Type representing a name as an integer.
//
// The number is turned into a label using printf format LABEL.
typedef unsigned int LabelId;
#define LABEL "L.%x"

typedef struct {
  size_t size;
  const char** xs;
} LocV;

static void AddLoc(const char* source, LocV* locs);
static void CollectBlocksAndLocs(FbleCodeV* blocks, LocV* locs, FbleCode* code);

static void StringLit(FILE* fout, const char* string);
static LabelId StaticString(FILE* fout, LabelId* label_id, const char* string);
static LabelId StaticNames(FILE* fout, LabelId* label_id, FbleNameV names);
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path);
static LabelId StaticExecutableModule(FILE* fout, LabelId* label_id, FbleCompiledModule* module);

static void GetFrameVar(FILE* fout, const char* rdst, FbleFrameIndex index);
static void SetFrameVar(FILE* fout, const char* rsrc, FbleLocalIndex index);
static void ReturnAbort(FILE* fout, void* code, size_t pc, const char* lmsg, FbleLoc loc);

static void EmitInstr(FILE* fout, void* code, size_t pc, FbleInstr* instr);
static void EmitCode(FILE* fout, FbleCode* code);
static void LabelForLocStr(FILE* fout, const char* str);
static FbleString* LabelForPath(FbleModulePath* path);

// AddLoc --
//   Add a source location to the list of locations.
//
// Inputs:
//   source - the source file name to add
//   locs - the list of locs to add to.
//
// Side effects:
//   Adds the source filename to the list of locations if it is not already
//   present in the list.
static void AddLoc(const char* source, LocV* locs)
{
  for (size_t i = 0; i < locs->size; ++i) {
    if (strcmp(source, locs->xs[i]) == 0) {
      return;
    }
  }
  FbleVectorAppend(*locs, source);
}

// CollectBlocksAndLocs --
//   Get the list of all instruction blocks and location source file names
//   referenced from the given block of code, including the code itself.
//
// Inputs:
//   blocks - the collection of blocks to add to.
//   locs - the collection of location source names to add to.
//   code - the code to collect the blocks from.
static void CollectBlocksAndLocs(FbleCodeV* blocks, LocV* locs, FbleCode* code)
{
  FbleVectorAppend(*blocks, code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    switch (code->instrs.xs[i]->tag) {
      case FBLE_STRUCT_VALUE_INSTR: break;
      case FBLE_UNION_VALUE_INSTR: break;

      case FBLE_STRUCT_ACCESS_INSTR:
      case FBLE_UNION_ACCESS_INSTR: {
        FbleAccessInstr* instr = (FbleAccessInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_UNION_SELECT_INSTR: {
        FbleUnionSelectInstr* instr = (FbleUnionSelectInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_JUMP_INSTR: break;

      case FBLE_FUNC_VALUE_INSTR: {
        FbleFuncValueInstr* instr = (FbleFuncValueInstr*)code->instrs.xs[i];
        CollectBlocksAndLocs(blocks, locs, instr->code);
        break;
      }

      case FBLE_CALL_INSTR: {
        FbleCallInstr* instr = (FbleCallInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_LINK_INSTR: break;
      case FBLE_FORK_INSTR: break;
      case FBLE_COPY_INSTR: break;
      case FBLE_REF_VALUE_INSTR: break;

      case FBLE_REF_DEF_INSTR: {
        FbleRefDefInstr* instr = (FbleRefDefInstr*)code->instrs.xs[i];
        AddLoc(instr->loc.source->str, locs);
        break;
      }

      case FBLE_RETURN_INSTR: break;
      case FBLE_TYPE_INSTR: break;
      case FBLE_RELEASE_INSTR: break;
    }
  }
}

// StringLit --
//   Output a string literal to fout.
//
// Inputs:
//   fout - the file to write to.
//   string - the contents of the string to write.
//
// Side effects:
//   Outputs the given string as a C string literal to the given file.
static void StringLit(FILE* fout, const char* string)
{
  fprintf(fout, "\"");
  for (const char* p = string; *p; p++) {
    // TODO: Handle other special characters too.
    switch (*p) {
      case '\n': fprintf(fout, "\\n"); break;
      case '"': fprintf(fout, "\\\""); break;
      case '\\': fprintf(fout, "\\\\"); break;
      default: fprintf(fout, "%c", *p); break;
    }
  }
  fprintf(fout, "\"");
}

// StaticString --
//   Output code to declare a static FbleString value.
//
// Inputs:
//   fout - the file to write to
//   label_id - pointer to next available label id for use.
//   string - the value of the string.
//
// Returns:
//   A label id of a local, static FbleString.
//
// Side effects:
//   Writes code to fout and allocates label ids out of label_id.
static LabelId StaticString(FILE* fout, LabelId* label_id, const char* string)
{
  LabelId id = (*label_id)++;

  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");                       // 64 bit alignment
  fprintf(fout, LABEL ":\n", id);
  fprintf(fout, "  .xword 1\n");                       // .refcount = 1
  fprintf(fout, "  .xword %i\n", FBLE_STRING_MAGIC);   // .magic
  fprintf(fout, "  .string ");                         // .str
  StringLit(fout, string);
  fprintf(fout, "\n");
  return id;
}

// StaticNames --
//   Output code to declare a static FbleNameV.xs value.
//
// Inputs:
//   fout - the file to write to
//   label_id - pointer to next available label id for use.
//   names - the value of the names.
//
// Returns:
//   A label id of a local, static FbleNameV.xs.
//
// Side effects:
//   Writes code to fout and allocates label ids out of label_id.
static LabelId StaticNames(FILE* fout, LabelId* label_id, FbleNameV names)
{
  LabelId str_ids[names.size];
  LabelId src_ids[names.size];
  for (size_t i = 0; i < names.size; ++i) {
    str_ids[i] = StaticString(fout, label_id, names.xs[i].name->str);
    src_ids[i] = StaticString(fout, label_id, names.xs[i].loc.source->str);
  }

  LabelId id = (*label_id)++;
  fprintf(fout, "  .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", id);
  for (size_t i = 0; i < names.size; ++i) {
    fprintf(fout, "  .xword " LABEL "\n", str_ids[i]);    // name
    fprintf(fout, "  .word %i\n", names.xs[i].space);     // space
    fprintf(fout, "  .zero 4\n");                         // padding
    fprintf(fout, "  .xword " LABEL "\n", src_ids[i]);    // loc.src
    fprintf(fout, "  .word %i\n", names.xs[i].loc.line);  // loc.line
    fprintf(fout, "  .word %i\n", names.xs[i].loc.col);   // loc.col
  }
  return id;
}

// StaticModulePath --
//   Generate code to declare a static FbleModulePath value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   label_id - pointer to next available label id for use.
//   path - the FbleModulePath to generate code for.
//
// Results:
//   The label id of a local, static FbleModulePath.
//
// Side effects:
// * Outputs code to fout.
// * Increments label_id based on the number of internal labels used.
static LabelId StaticModulePath(FILE* fout, LabelId* label_id, FbleModulePath* path)
{
  LabelId src_id = StaticString(fout, label_id, path->loc.source->str);
  LabelId names_id = StaticNames(fout, label_id, path->path);
  LabelId path_id = (*label_id)++;

  fprintf(fout, "  .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", path_id);
  fprintf(fout, "  .xword 1\n");                  // .refcount
  fprintf(fout, "  .xword 2004903300\n");         // .magic
  fprintf(fout, "  .xword " LABEL "\n", src_id);        // path->loc.src
  fprintf(fout, "  .word %i\n", path->loc.line);
  fprintf(fout, "  .word %i\n", path->loc.col);
  fprintf(fout, "  .xword %zi\n", path->path.size);
  fprintf(fout, "  .xword " LABEL "\n", names_id);
  return path_id;
}

// StaticExecutableModule --
//   Generate code to declare a static FbleExecutableModule value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   label_id - pointer to next available label id for use.
//   module - the FbleCompiledModule to generate code for.
//
// Results:
//   The label id of a local, static FbleExecutableModule.
//
// Side effects:
// * Outputs code to fout.
// * Increments label_id based on the number of internal labels used.
static LabelId StaticExecutableModule(FILE* fout, LabelId* label_id, FbleCompiledModule* module)
{
  LabelId path_id = StaticModulePath(fout, label_id, module->path);

  LabelId dep_ids[module->deps.size];
  for (size_t i = 0; i < module->deps.size; ++i) {
    dep_ids[i] = StaticModulePath(fout, label_id, module->deps.xs[i]);
  }

  LabelId deps_xs_id = (*label_id)++;
  fprintf(fout, "  .section .data.rel.local\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", deps_xs_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    fprintf(fout, "  .xword " LABEL "\n", dep_ids[i]);
  }

  LabelId profile_blocks_xs_id = StaticNames(fout, label_id, module->code->_base.profile_blocks);

  LabelId executable_id = (*label_id)++;
  fprintf(fout, "  .section .data.rel,\"aw\"\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", executable_id);
  fprintf(fout, "  .xword 1\n");                          // .refcount
  fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MAGIC);  // .magic
  fprintf(fout, "  .xword %zi\n", module->code->_base.args);
  fprintf(fout, "  .xword %zi\n", module->code->_base.statics);
  fprintf(fout, "  .xword %zi\n", module->code->_base.locals);
  fprintf(fout, "  .xword %zi\n", module->code->_base.profile);
  fprintf(fout, "  .xword %zi\n", module->code->_base.profile_blocks.size);
  fprintf(fout, "  .xword " LABEL "\n", profile_blocks_xs_id);
  fprintf(fout, "  .xword _Run_%p\n", (void*)module->code);
  fprintf(fout, "  .xword _Abort_%p\n", (void*)module->code);
  fprintf(fout, "  .xword FbleExecutableNothingOnFree\n");

  LabelId module_id = (*label_id)++;
  fprintf(fout, "  .section .data.rel.local\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", module_id);
  fprintf(fout, "  .xword 1\n");                                  // .refcount
  fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MODULE_MAGIC);   // .magic
  fprintf(fout, "  .xword " LABEL "\n", path_id);                       // .path
  fprintf(fout, "  .xword %zi\n", module->deps.size);
  fprintf(fout, "  .xword " LABEL "\n", deps_xs_id);
  fprintf(fout, "  .xword " LABEL "\n", executable_id);
  return module_id;
}

// GetFrameVar --
//   Generate code to read a variable from the current frame into register rdst.
//
// Inputs:
//   fout - the output stream
//   rdst - the name of the register to read the variable into
//   index - the variable to read.
//
// Side effects:
// * Writes to the output stream.
static void GetFrameVar(FILE* fout, const char* rdst, FbleFrameIndex index)
{
  static const char* section[] = { "R_STATICS", "R_LOCALS" };
  fprintf(fout, "  ldr %s, [%s, #%zi]\n", rdst, section[index.section], 8 * index.index);
}

// SetFrameVar --
//   Generate code to write a variable to the current frame from register rsrc.
//
// Inputs:
//   fout - the output stream
//   rsrc - the name of the register with the value to write.
//   index - the index of the value to write.
//
// Side effects:
// * Writes to the output stream.
static void SetFrameVar(FILE* fout, const char* rsrc, FbleLocalIndex index)
{
  fprintf(fout, "  str %s, [R_LOCALS, #%zi]\n", rsrc, 8 * index);
}

// ReturnAbort --
//   Emit code to return an error from a Run function.
//
// Inputs:
//   fout - the output stream.
//   code - pointer to current code block to use for labels.
//   pc - the program counter of the abort location.
//   lmsg - the label of the error message to use.
//   loc - the location to report with the error message.
//
// Side effects:
//   Emit code to return the error.
static void ReturnAbort(FILE* fout, void* code, size_t pc, const char* lmsg, FbleLoc loc)
{
  // stack->pc = pc
  fprintf(fout, "  ldr x0, [SP, #32]\n");   // x0 = thread. TODO: keep in sync with EmitCode.
  fprintf(fout, "  ldr x0, [x0, #0]\n");    // x0 = thread->stack
  fprintf(fout, "  mov x1, #%zi\n", pc);    // x1 = pc
  fprintf(fout, "  str x1, [x0, #16]\n");   // stack->pc = pc

  // Print error message.
  fprintf(fout, "  adr x0, stderr\n");
  fprintf(fout, "  adr x1, L.ErrorFormatString\n");

  fprintf(fout, "  adr x2, ");
  LabelForLocStr(fout, loc.source->str);
  fprintf(fout, "\n");

  fprintf(fout, "  mov x3, %i\n", loc.line);
  fprintf(fout, "  mov x4, %i\n", loc.col);
  fprintf(fout, "  adr x5, %s\n", lmsg);
  fprintf(fout, "  bl fprintf\n");

  // Return FBLE_EXEC_ABORTED
  fprintf(fout, "  mov x0, #%i\n", FBLE_EXEC_ABORTED);
  fprintf(fout, "  b L._Run_.%p.exit\n", code);
}

// EmitInstr --
//   Generate code to execute an instruction.
//
// Inputs:
//   fout - the output stream to write the code to.
//   code - pointer to the current code block, for referencing labels.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//
// Side effects:
// * Outputs code to fout.
static void EmitInstr(FILE* fout, void* code, size_t pc, FbleInstr* instr)
{
// #define TODO assert(false && "TODO")
#define TODO fprintf(fout, "  bl abort\n"); return

  // TODO: Emit profiling instructions.
  switch (instr->tag) {
    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      size_t argc = struct_instr->args.size;

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", argc);

      // The first 6 args go in registers x2 through x7.
      const size_t num_reg_args = 6;
      for (size_t i = 0; i < argc && i < num_reg_args; ++i) {
        char rdst[5];
        sprintf(rdst, "x%zi", i + 2);
        GetFrameVar(fout, rdst, struct_instr->args.xs[i]);
      }

      // Subsequent args go onto the stack.
      // We need to keep the stack 16-byte aligned.
      size_t stack = 16 * ((argc - num_reg_args + 1) / 2);
      if (argc > num_reg_args) {
        fprintf(fout, "  sub SP, SP, #%zi\n", stack);
      }

      for (size_t i = num_reg_args; i < argc; ++i) {
        GetFrameVar(fout, "x9", struct_instr->args.xs[i]);
        fprintf(fout, "  str x9, [SP, #%zi]\n", 8 * (i - num_reg_args));
      }

      fprintf(fout, "  bl FbleNewStructValue\n");
      SetFrameVar(fout, "x0", struct_instr->dest);

      if (argc > num_reg_args) {
        fprintf(fout, "  add SP, SP, #%zi\n", stack);
      }
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", union_instr->tag);
      GetFrameVar(fout, "x2", union_instr->arg);
      fprintf(fout, "  bl FbleNewUnionValue\n");
      SetFrameVar(fout, "x0", union_instr->dest);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      GetFrameVar(fout, "x0", access_instr->obj);
      fprintf(fout, "  bl FbleStrictValue\n");

      // Abort if the struct object is NULL.
      fprintf(fout, "  cbnz x0, L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, "L.UndefinedStructValue", access_instr->loc);

      fprintf(fout, "L.%p.%zi.ok:\n", code, pc);
      fprintf(fout, "  mov x1, #%zi\n", access_instr->tag);
      fprintf(fout, "  bl FbleStructValueAccess\n");
      SetFrameVar(fout, "x0", access_instr->dest);
      fprintf(fout, "  mov x1, x0\n");
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  bl FbleRetainValue\n");
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      // Get the union value.
      GetFrameVar(fout, "x0", access_instr->obj);
      fprintf(fout, "  bl FbleStrictValue\n");

      // Abort if the union object is NULL.
      fprintf(fout, "  cbnz x0, L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, "L.UndefinedUnionValue", access_instr->loc);

      // Abort if the tag is wrong.
      fprintf(fout, "L.%p.%zi.ok:\n", code, pc);
      fprintf(fout, "  ldr x1, [x0, #8]\n"); // uv->tag
      fprintf(fout, "  cmp x1, %zi\n", access_instr->tag);
      fprintf(fout, "  b.eq L.%p.%zi.tagok\n", code, pc);
      ReturnAbort(fout, code, pc, "L.WrongUnionTag", access_instr->loc);

      // Access the field.
      fprintf(fout, "L.%p.%zi.tagok:\n", code, pc);
      fprintf(fout, "  ldr x0, [x0, #16]\n"); // uv->ar
      SetFrameVar(fout, "x0", access_instr->dest);
      fprintf(fout, "  mov x1, x0\n");
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  bl FbleRetainValue\n");
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;

      // Jump table data for jumping to the right fble pc.
      fprintf(fout, "  .section .data.rel.local\n");
      fprintf(fout, "  .align 3\n");
      fprintf(fout, "L._Run_%p.%zi.pcs:\n", code, pc);
      for (size_t i = 0; i < select_instr->jumps.size; ++i) {
        fprintf(fout, "  .xword L._Run_%p.pc.%zi\n", (void*)code, pc + 1 + select_instr->jumps.xs[i]);
      }

      // Get the union value.
      fprintf(fout, "  .text\n");
      GetFrameVar(fout, "x0", select_instr->condition);
      fprintf(fout, "  bl FbleStrictValue\n");

      // Abort if the union object is NULL.
      fprintf(fout, "  cbnz x0, L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, "L.UndefinedUnionSelect", select_instr->loc);

      fprintf(fout, "L.%p.%zi.ok:\n", code, pc);
      fprintf(fout, "  ldr x0, [x0, #8]\n");  // x0 = uv->tag
      fprintf(fout, "  lsl x0, x0, #3\n");    // x0 = 8 * (uv->tag)
      fprintf(fout, "  adr x1, L._Run_%p.%zi.pcs\n", code, pc); // x1 = pcs
      fprintf(fout, "  add x0, x0, x1\n");   // x0 = &pcs[uv->tag] 
      fprintf(fout, "  ldr x0, [x0]\n");     // x0 = pcs[uv->tag]
      fprintf(fout, "  br x0\n");            // goto pcs[uv->tag]
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "  b L._Run_%p.pc.%zi\n", code, pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      fprintf(fout, "  .section .data.rel,\"aw\"\n");
      fprintf(fout, "  .align 3\n");
      fprintf(fout, "L._Run_%p.%zi.exe:\n", code, pc);
      fprintf(fout, "  .xword 1\n");                          // .refcount
      fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MAGIC);  // .magic
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.args);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.statics);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.locals);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.profile);
      fprintf(fout, "  .xword 0\n"); // .profile_blocks.size
      fprintf(fout, "  .xword 0\n"); // .profile_blocks.xs
      fprintf(fout, "  .xword _Run_%p\n", (void*)func_instr->code);
      fprintf(fout, "  .xword _Abort_%p\n", (void*)func_instr->code);
      fprintf(fout, "  .xword 0\n"); // .on_free

      fprintf(fout, "  .text\n");
      fprintf(fout, "  .align 2\n");

      fprintf(fout, "  ldr x0, [SP, #32]\n");   // x0 = thread. TODO: keep in sync with EmitCode.
      fprintf(fout, "  ldr x0, [x0, #0]\n");    // x0 = thread->stack
      fprintf(fout, "  ldr x0, [x0, #8]\n");    // x0 = thread->stack->func
      fprintf(fout, "  ldr x2, [x0, #16]\n");   // x2 = thread->stack->func->profile_base_id

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  adr x1, L._Run_%p.%zi.exe\n", code, pc);
      fprintf(fout, "  bl FbleNewFuncValue\n");
      fprintf(fout, "  mov R_SCRATCH_0, x0\n");
      SetFrameVar(fout, "R_SCRATCH_0", func_instr->dest);

      for (size_t i = 0; i < func_instr->code->_base.statics; ++i) {
        fprintf(fout, "  mov x0, R_HEAP\n");
        fprintf(fout, "  mov x1, R_SCRATCH_0\n");
        GetFrameVar(fout, "x2", func_instr->scope.xs[i]);
        fprintf(fout, "  bl FbleValueAddRef\n");
      }
      return;
    }

    case FBLE_CALL_INSTR: TODO;

    case FBLE_LINK_INSTR: {
      FbleLinkInstr* link_instr = (FbleLinkInstr*)instr;

      // Allocate and initialize the FbleLinkValue.
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, #%zi\n", sizeof(FbleLinkValue));
      fprintf(fout, "  bl FbleNewHeapObject\n");
      fprintf(fout, "  mov w1, #%i\n", FBLE_LINK_VALUE);
      fprintf(fout, "  str w1, [x0]\n");       // link->_base.tag = FBLE_LINK_VALUE.
      fprintf(fout, "  str XZR, [x0, #8]\n");  // link->head = NULL;
      fprintf(fout, "  str XZR, [x0, #16]\n"); // link->tail = NULL;
      fprintf(fout, "  mov R_SCRATCH_0, x0\n");

      // Compute thread->stack->func->profile_base_id
      fprintf(fout, "  ldr x0, [SP, #32]\n"); // thread
      fprintf(fout, "  ldr x0, [x0]\n");      // ->stack
      fprintf(fout, "  ldr x0, [x0, #8]\n");  // ->func
      fprintf(fout, "  ldr R_SCRATCH_1, [x0, #16]\n");   // ->profile_base_id

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, R_SCRATCH_0\n");
      fprintf(fout, "  add x2, R_SCRATCH_1, #%zi\n", link_instr->profile);
      fprintf(fout, "  bl FbleNewGetValue\n");
      SetFrameVar(fout, "x0", link_instr->get);

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, R_SCRATCH_0\n");
      fprintf(fout, "  add x2, R_SCRATCH_1, #%zi\n", link_instr->profile + 1);
      fprintf(fout, "  bl FbleNewPutValue\n");
      SetFrameVar(fout, "x0", link_instr->put);

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, R_SCRATCH_0\n");
      fprintf(fout, "  bl FbleReleaseValue\n");
      return;
    }

    case FBLE_FORK_INSTR: TODO;

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      GetFrameVar(fout, "x1", copy_instr->source);
      SetFrameVar(fout, "x1", copy_instr->dest);
      fprintf(fout, "  bl FbleRetainValue\n");
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, #%zi\n", sizeof(FbleRefValue));
      fprintf(fout, "  bl FbleNewHeapObject\n");
      fprintf(fout, "  mov w1, #%i\n", FBLE_REF_VALUE);
      fprintf(fout, "  str w1, [x0]\n");      // v->_base.tag = FBLE_REF_VALUE
      fprintf(fout, "  str XZR, [x0, #8]\n"); // v->value = NULL.
      SetFrameVar(fout, "x0", ref_instr->dest);
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;
      GetFrameVar(fout, "x0", ref_instr->value);
      fprintf(fout, "  bl FbleStrictRefValue\n");

      FbleFrameIndex ref_index = {
        .section = FBLE_LOCALS_FRAME_SECTION,
        .index = ref_instr->ref
      };
      GetFrameVar(fout, "x1", ref_index);
      fprintf(fout, "  cmp x0, x1\n");
      fprintf(fout, "  b.ne L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, "L.VacuousValue", ref_instr->loc);

      fprintf(fout, "L.%p.%zi.ok:\n", code, pc);
      fprintf(fout, "  str x0, [x1, #8]\n");  // rv->value = v;
      fprintf(fout, "  mov x2, x0\n");
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  bl FbleValueAddRef\n");
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      GetFrameVar(fout, "R_SCRATCH_0", return_instr->result);

      switch (return_instr->result.section) {
        case FBLE_STATICS_FRAME_SECTION: {
          fprintf(fout, "  mov x0, R_HEAP\n");
          fprintf(fout, "  mov x1, R_SCRATCH_0\n");
          fprintf(fout, "  bl FbleRetainValue\n");
          break;
        }

        case FBLE_LOCALS_FRAME_SECTION: break;
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  ldr x1, [SP, #32]\n");
      fprintf(fout, "  mov x2, R_SCRATCH_0\n");
      fprintf(fout, "  bl FbleThreadReturn\n");

      fprintf(fout, "  mov x0, #%i\n", FBLE_EXEC_FINISHED);
      fprintf(fout, "  b L._Run_.%p.exit\n", code);
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, #%zi\n", sizeof(FbleTypeValue));
      fprintf(fout, "  bl FbleNewHeapObject\n");
      fprintf(fout, "  mov w1, #%i\n", FBLE_TYPE_VALUE);
      fprintf(fout, "  str w1, [x0]\n"); // v->_base.tag = FBLE_TYPE_VALUE.
      SetFrameVar(fout, "x0", type_instr->dest);
      return;
    }

    case FBLE_RELEASE_INSTR: {
      FbleReleaseInstr* release_instr = (FbleReleaseInstr*)instr;
      fprintf(fout, "  mov x0, R_HEAP\n");

      FbleFrameIndex target_index = {
        .section = FBLE_LOCALS_FRAME_SECTION,
        .index = release_instr->target
      };
      GetFrameVar(fout, "x1", target_index);
      fprintf(fout, "  bl FbleReleaseValue\n");
      return;
    }
  }
}

// EmitCode --
//   Generate code to execute an FbleCode block.
//
// Inputs:
//   fout - the output stream to write the code to.
//   code - the block of code to generate a C function for.
//
// Side effects:
// * Outputs code to fout with two space indent.
static void EmitCode(FILE* fout, FbleCode* code)
{
  // Jump table data for jumping to the right fble pc.
  fprintf(fout, "  .section .data.rel.local\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, "L._Run_%p.pcs:\n", (void*)code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "  .xword L._Run_%p.pc.%zi\n", (void*)code, i);
  }

  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "_Run_%p:\n", (void*)code);

  // Set up stack and frame pointer.
  fprintf(fout, "  stp FP, LR, [SP, #-80]!\n");
  fprintf(fout, "  mov FP, SP\n");

  // Save args to the stack for later reference.
  fprintf(fout, "  str x0, [SP, #16]\n"); // heap
  fprintf(fout, "  str x1, [SP, #24]\n"); // threads
  fprintf(fout, "  str x2, [SP, #32]\n"); // thread. TODO: keep in sync with users!
  fprintf(fout, "  str x3, [SP, #40]\n"); // io_activity

  // Save callee saved registers for later restoration.
  fprintf(fout, "  str R_HEAP, [SP, #48]\n");
  fprintf(fout, "  str R_LOCALS, [SP, #56]\n");
  fprintf(fout, "  str R_STATICS, [SP, #64]\n");
  fprintf(fout, "  str R_SCRATCH_0, [SP, #72]\n");
  fprintf(fout, "  str R_SCRATCH_1, [SP, #80]\n");

  // Set up common registers.
  fprintf(fout, "  ldr x4, [x2]\n");          // thread->stack
  fprintf(fout, "  ldr x5, [x4, 8]\n");       // thread->stack->func
  fprintf(fout, "  mov R_HEAP, x0\n");
  fprintf(fout, "  add R_LOCALS, x4, #40\n");
  fprintf(fout, "  add R_STATICS, x5, #%zi\n", sizeof(FbleValue) + 16);

  // Jump to the fble instruction at thread->stack->pc.
  fprintf(fout, "  ldr x0, [x4, #16]\n");  // x0 = (thread->stack)->pc
  fprintf(fout, "  lsl x0, x0, #3\n");     // x0 = 8 * (thread->stack->pc)
  fprintf(fout, "  adr x1, L._Run_%p.pcs\n", (void*)code); // x1 = pcs
  fprintf(fout, "  add x0, x0, x1\n");     // x0 = &pcs[thread->stack->pc]
  fprintf(fout, "  ldr x0, [x0]\n");       // x0 = pcs[thread->stack->pc]
  fprintf(fout, "  br x0\n");              // goto pcs[thread->stack->pc]

  // Emit code for each fble instruction
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "L._Run_%p.pc.%zi:\n", (void*)code, i);
    EmitInstr(fout, code, i, code->instrs.xs[i]);
  }

  // Restore stack and frame pointer and return whatever is in x0.
  // Common code for exiting a run function.
  fprintf(fout, "L._Run_.%p.exit:\n", (void*)code);
  fprintf(fout, "  ldr R_HEAP, [SP, #48]\n");
  fprintf(fout, "  ldr R_LOCALS, [SP, #56]\n");
  fprintf(fout, "  ldr R_STATICS, [SP, #64]\n");
  fprintf(fout, "  ldr R_SCRATCH_0, [SP, #72]\n");
  fprintf(fout, "  ldr R_SCRATCH_1, [SP, #80]\n");
  fprintf(fout, "  ldp FP, LR, [SP], #80\n");
  fprintf(fout, "  ret\n");
}

// LabelForLocStr --
//   Output the label for a location source file name string.
//
// Inputs:
//   fout - where to output the label.
//   str - the file name string to output the label for.
static void LabelForLocStr(FILE* fout, const char* str)
{
  fprintf(fout, "L.loc.");
  for (const char* p = str; *p != '\0'; p++) {
    fprintf(fout, isalnum(*p) ? "%c" : "_%02x_", *p);
  }
}

// LabelForPath --
//   Returns a name suitable for use as a C function identifier to use for the
//   give module path.
//
// Inputs:
//   path - the path to get the name for.
//
// Results:
//   A C function name for the module path.
//
// Side effects:
//   Allocates an FbleString* that should be freed with FbleFreeString when no
//   longer needed.
static FbleString* LabelForPath(FbleModulePath* path)
{
  // The conversion from path to name works as followed:
  // * We add _Fble as a prefix.
  // * Characters [0-9], [a-z], [A-Z] are kept as is.
  // * Other characters are translated to _XX_, where XX is the 2 digit hex
  //   representation of the ascii value of the character.
  // * We include translated '/' and '%' characters where expected in the
  //   path.

  // Determine the length of the name.
  size_t len = strlen("_Fble") + 1; // prefix and terminating '\0'.
  for (size_t i = 0; i < path->path.size; ++i) {
    len += 4;   // translated '/' character.
    for (const char* p = path->path.xs[i].name->str; *p != '\0'; p++) {
      if (isalnum(*p)) {
        len++;        // untranslated character
      } else {
        len += 4;     // translated character
      }
    }
  }
  len += 4; // translated '%' character.

  // Construct the name.
  char name[len];
  char translated[5]; 
  name[0] = '\0';
  strcat(name, "_Fble");
  for (size_t i = 0; i < path->path.size; ++i) {
    sprintf(translated, "_%02x_", '/');
    strcat(name, translated);
    for (const char* p = path->path.xs[i].name->str; *p != '\0'; p++) {
      if (isalnum(*p)) {
        sprintf(translated, "%c", *p);
        strcat(name, translated);
      } else {
        sprintf(translated, "_%02x_", *p);
        strcat(name, translated);
      }
    }
  }
  sprintf(translated, "_%02x_", '%');
  strcat(name, translated);

  return FbleNewString(name);
}

// FbleGenerateAArch64 -- see documentation in fble-compile.h
void FbleGenerateAArch64(FILE* fout, FbleCompiledModule* module)
{
  FbleCodeV blocks;
  FbleVectorInit(blocks);

  LocV locs;
  FbleVectorInit(locs);

  CollectBlocksAndLocs(&blocks, &locs, module->code);

  // Common things we hold in callee saved registers for Run and Abort
  // functions.
  fprintf(fout, "  R_HEAP .req x19\n");
  fprintf(fout, "  R_LOCALS .req x20\n");
  fprintf(fout, "  R_STATICS .req x21\n");
  fprintf(fout, "  R_SCRATCH_0 .req x22\n");
  fprintf(fout, "  R_SCRATCH_1 .req x23\n");

  // Error messages.
  fprintf(fout, "  .section .data\n");
  fprintf(fout, "L.ErrorFormatString:\n");
  fprintf(fout, "  .string \"%%s:%%d:%%d: error: %%s\"\n");
  fprintf(fout, "L.UndefinedStructValue:\n");
  fprintf(fout, "  .string \"undefined struct value access\\n\"\n");
  fprintf(fout, "L.UndefinedUnionValue:\n");
  fprintf(fout, "  .string \"undefined union value access\\n\";\n");
  fprintf(fout, "L.UndefinedUnionSelect:\n");
  fprintf(fout, "  .string \"undefined union value select\\n\";\n");
  fprintf(fout, "L.WrongUnionTag:\n");
  fprintf(fout, "  .string \"union field access undefined: wrong tag\\n\";\n");
  fprintf(fout, "L.UndefinedFunctionValue:\n");
  fprintf(fout, "  .string \"called undefined function\\n\";\n");
  fprintf(fout, "L.VacuousValue:\n");
  fprintf(fout, "  .string \"vacuous value\\n\";\n");

  // Definitions of source code locations.
  for (size_t i = 0; i < locs.size; ++i) {
    LabelForLocStr(fout, locs.xs[i]);
    fprintf(fout, ":\n  .string \"%s\"\n", locs.xs[i]);
  }

  for (int i = 0; i < blocks.size; ++i) {
    // RunFunction
    EmitCode(fout, blocks.xs[i]);

    fprintf(fout, "  .text\n");
    fprintf(fout, "  .align 2\n");
    fprintf(fout, "_Abort_%p:\n", (void*)blocks.xs[i]);
    fprintf(fout, "  stp FP, LR, [SP, #-16]!\n");
    fprintf(fout, "  mov FP, SP\n");
    fprintf(fout, "  bl abort\n");  // TODO: Implement me.
    fprintf(fout, "  ldp FP, LR, [SP], #16\n");
    fprintf(fout, "  ret\n");
  }
  FbleFree(blocks.xs);
  FbleFree(locs.xs);

  LabelId label_id = 0;
  LabelId module_id = StaticExecutableModule(fout, &label_id, module);
  LabelId deps_id = label_id++;

  fprintf(fout, "  .section .data.rel,\"aw\"\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", deps_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = LabelForPath(module->deps.xs[i]);
    fprintf(fout, "  .xword %s\n", dep_name->str);
    FbleFreeString(dep_name);
  }

  FbleString* func_name = LabelForPath(module->path);
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "  .global %s\n", func_name->str);
  fprintf(fout, "%s:\n", func_name->str);
  fprintf(fout, "  stp FP, LR, [SP, #-16]!\n");
  fprintf(fout, "  mov FP, SP\n");

  fprintf(fout, "  adr x1, " LABEL "\n", module_id);
  fprintf(fout, "  mov x2, %zi\n", module->deps.size);
  fprintf(fout, "  adr x3, " LABEL "\n", deps_id);
  fprintf(fout, "  bl FbleLoadFromCompiled\n");

  fprintf(fout, "  ldp FP, LR, [SP], #16\n");
  fprintf(fout, "  ret\n");
  FbleFreeString(func_name);
}

// FbleGenerateAArch64Export -- see documentation in fble-compile.h
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path)
{
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "  .global %s\n", name);
  fprintf(fout, "%s:\n", name);
  fprintf(fout, "  stp FP, LR, [SP, #-16]!\n");
  fprintf(fout, "  mov FP, SP\n");

  FbleString* module_name = LabelForPath(path);
  fprintf(fout, "  bl %s\n\n", module_name->str);
  FbleFreeString(module_name);

  fprintf(fout, "  ldp FP, LR, [SP], #16\n");
  fprintf(fout, "  ret\n");
}
