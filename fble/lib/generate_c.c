// generate_c.c --
//   This file describes code to generate C code for fble modules.

#include <assert.h>   // for assert
#include <ctype.h>    // for isalnum
#include <stdio.h>    // for sprintf
#include <stdarg.h>   // for va_list, va_start, va_end.
#include <stddef.h>   // for offsetof
#include <string.h>   // for strlen, strcat
#include <unistd.h>   // for getcwd

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
#define LABEL "l%x"

typedef struct {
  size_t size;
  const char** xs;
} LocV;

// RunStackFrame --
//   A type representing the stack frame layout we'll use for _Run_ functions.
typedef struct {
  void* FP;
  void* LR;
  void* heap;
  void* thread;
  void* r_heap_save;
  void* r_locals_save;
  void* r_statics_save;
  void* r_profile_save;
  void* r_profile_base_id_save;
  void* r_scratch_0_save;
  void* r_scratch_1_save;
  void* padding;
} RunStackFrame;

// AbortStackFrame --
//   A type representing the stack frame layout we'll use for _Abort_ functions.
typedef struct {
  void* FP;
  void* LR;
  void* heap;
  void* stack;
  void* r_heap_save;
  void* r_locals_save;
} AbortStackFrame;

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

static size_t StackBytesForCount(size_t count);

static void Adr(FILE* fout, const char* r_dst, const char* fmt, ...);

static void EmitInstr(FILE* fout, FbleNameV profile_blocks, void* code, size_t pc, FbleInstr* instr);
static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static void EmitInstrForAbort(FILE* fout, void* code, size_t pc, FbleInstr* instr);
static void EmitCodeForAbort(FILE* fout, FbleNameV profile_blocks, FbleCode* code);
static size_t SizeofSanitizedString(const char* str);
static void SanitizeString(const char* str, char* dst);
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
      case FBLE_DATA_TYPE_INSTR: break;
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
      case FBLE_LIST_INSTR: break;
      case FBLE_LITERAL_INSTR: break;
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
  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, LABEL ":\n", deps_xs_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    fprintf(fout, "  .xword " LABEL "\n", dep_ids[i]);
  }

  LabelId profile_blocks_xs_id = StaticNames(fout, label_id, module->code->_base.profile_blocks);

  LabelId executable_id = (*label_id)++;
  fprintf(fout, "  .section .data\n");
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

  FbleName function_block = module->code->_base.profile_blocks.xs[module->code->_base.profile];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "  .xword _Run.%p.%s\n", (void*)module->code, function_label);
  fprintf(fout, "  .xword _Abort.%p.%s\n", (void*)module->code, function_label);
  fprintf(fout, "  .xword FbleExecutableNothingOnFree\n");

  LabelId module_id = (*label_id)++;
  fprintf(fout, "  .section .data\n");
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
//   Generate code to read a variable from the current frame into expression dst.
//
// Inputs:
//   fout - the output stream
//   dst - the name of the register to read the variable into
//   index - the variable to read.
//
// Side effects:
// * Writes to the output stream.
static void GetFrameVar(FILE* fout, const char* dst, FbleFrameIndex index)
{
  static const char* section[] = { "statics", "locals" };
  fprintf(fout, "    %s = %s[%zi];\n", dst, section[index.section], index.index);
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
  fprintf(fout, "  str %s, [R_LOCALS, #%zi]\n", rsrc, sizeof(FbleValue*) * index);
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
  fprintf(fout, "  ldr x0, [SP, #%zi]\n", offsetof(RunStackFrame, thread));
  fprintf(fout, "  ldr x0, [x0, #%zi]\n", offsetof(FbleThread, stack));
  fprintf(fout, "  mov x1, #%zi\n", pc);    // x1 = pc
  fprintf(fout, "  str x1, [x0, #%zi]\n", offsetof(FbleStack, pc));

  // Print error message.
  Adr(fout, "x0", "stderr");
  fprintf(fout, "  ldr x0, [x0]\n");
  Adr(fout, "x1", ".L.ErrorFormatString");

  char label[SizeofSanitizedString(loc.source->str)];
  SanitizeString(loc.source->str, label);
  Adr(fout, "x2", ".L.loc.%s", label);

  fprintf(fout, "  mov x3, %i\n", loc.line);
  fprintf(fout, "  mov x4, %i\n", loc.col);
  Adr(fout, "x5", "%s", lmsg);
  fprintf(fout, "  bl fprintf\n");

  // Return FBLE_EXEC_ABORTED
  fprintf(fout, "  mov x0, #%i\n", FBLE_EXEC_ABORTED);
  fprintf(fout, "  b .L._Run_.%p.exit\n", code);
}

// StackBytesForCount --
//   Calculate a 16 byte aligned number of bytes sufficient to store count
//   xwords.
//
// Inputs:
//   count - the number of xwords.
//
// Returns:
//   The number of bytes to allocate on the stack.
//
// Side effects:
//   None.
static size_t StackBytesForCount(size_t count)
{
  return 16 * ((count + 1) / 2);
}

// Adr --
//   Emit an adr instruction to load a label into a register.
//
// Inputs:
//   fout - the output stream
//   r_dst - the name of the register to load the label into
//   fmt, ... - a printf format string for the label to load.
//
// Side effects:
//   Emits a sequence of instructions to load the label into the register.
static void Adr(FILE* fout, const char* r_dst, const char* fmt, ...)
{
  va_list ap;

  fprintf(fout, "  adrp %s, ", r_dst);
  va_start(ap, fmt);
  vfprintf(fout, fmt, ap);
  va_end(ap);
  fprintf(fout, "\n");

  fprintf(fout, "  add %s, %s, :lo12:", r_dst, r_dst);
  va_start(ap, fmt);
  vfprintf(fout, fmt, ap);
  va_end(ap);
  fprintf(fout, "\n");
}

// EmitInstr --
//   Generate code to execute an instruction.
//
// Inputs:
//   fout - the output stream to write the code to.
//   profile_blocks - the list of profile block names for the module.
//   code - pointer to the current code block, for referencing labels.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//
// Side effects:
// * Outputs code to fout.
static void EmitInstr(FILE* fout, FbleNameV profile_blocks, void* code, size_t pc, FbleInstr* instr)
{
  for (FbleDebugInfo* info = instr->debug_info; info != NULL; info = info->next) {
    if (info->tag == FBLE_STATEMENT_DEBUG_INFO) {
      FbleStatementDebugInfo* stmt = (FbleStatementDebugInfo*)info;
      fprintf(fout, "#line %i\n", stmt->loc.line);
    }
  }

  fprintf(fout, "  if (profile) {\n");
  fprintf(fout, "    if (rand() %% 1024 == 0) FbleProfileSample(profile, 1);\n");
  for (FbleProfileOp* op = instr->profile_ops; op != NULL; op = op->next) {
    switch (op->tag) {
      case FBLE_PROFILE_ENTER_OP: {
        fprintf(fout, "    FbleProfileEnterBlock(profile, %zi);\n", op->block);
        break;
      }

      case FBLE_PROFILE_REPLACE_OP: {
        fprintf(fout, "    FbleProfileReplaceBlock(profile, %zi);\n", op->block);
        break;
      }

      case FBLE_PROFILE_EXIT_OP: {
        fprintf(fout, "    FbleProfileExitBlock(profile);\n");
        break;
      }
    }
  }
  fprintf(fout, "  }\n");

  static const char* section[] = { "s", "l" };
  switch (instr->tag) {
    case FBLE_DATA_TYPE_INSTR: {
      FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
      size_t fieldc = dt_instr->fields.size;

      fprintf(fout, "  {\n");
      fprintf(fout, "    FbleValue* fields[%zi];\n", fieldc);
      for (size_t i = 0; i < fieldc; ++i) {
        fprintf(fout, "    fields[%zi] = %s[%zi];\n", i,
            section[dt_instr->fields.xs[i].section],
            dt_instr->fields.xs[i].index);
      };

      static const char* dtkind[] = { "FBLE_STRUCT_DATATYPE", "FBLE_UNION_DATATYPE" };
      fprintf(fout, "    l[%zi] = FbleNewDataTypeValue(heap, %s, %zi, fields);\n",
          dt_instr->dest, dtkind[dt_instr->kind], fieldc);
      fprintf(fout, "  }\n");
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      size_t argc = struct_instr->args.size;

      fprintf(fout, "  l[%zi] = FbleNewStructValue(heap, %zi", struct_instr->dest, argc);
      for (size_t i = 0; i < argc; ++i) {
        fprintf(fout, ", %s[%zi]",
            section[struct_instr->args.xs[i].section],
            struct_instr->args.xs[i].index);
      };

      fprintf(fout, ");\n");
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      fprintf(fout, "  l[%zi] = FbleNewUnionValue(heap, %zi, %s[%zi]);\n",
          union_instr->dest, union_instr->tag,
          section[union_instr->arg.section],
          union_instr->arg.index);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      GetFrameVar(fout, "x0", access_instr->obj);
      fprintf(fout, "  bl FbleStrictValue\n");

      // Abort if the struct object is NULL.
      fprintf(fout, "  cbnz x0, .L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, ".L.UndefinedStructValue", access_instr->loc);

      fprintf(fout, ".L.%p.%zi.ok:\n", code, pc);
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
      fprintf(fout, "  cbnz x0, .L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, ".L.UndefinedUnionValue", access_instr->loc);

      // Abort if the tag is wrong.
      fprintf(fout, ".L.%p.%zi.ok:\n", code, pc);
      fprintf(fout, "  mov R_SCRATCH_0, x0\n");
      fprintf(fout, "  bl FbleUnionValueTag\n");
      fprintf(fout, "  cmp x0, %zi\n", access_instr->tag);
      fprintf(fout, "  b.eq .L.%p.%zi.tagok\n", code, pc);
      ReturnAbort(fout, code, pc, ".L.WrongUnionTag", access_instr->loc);

      // Access the field.
      fprintf(fout, ".L.%p.%zi.tagok:\n", code, pc);
      fprintf(fout, "  mov x0, R_SCRATCH_0\n");
      fprintf(fout, "  bl FbleUnionValueAccess\n");
      SetFrameVar(fout, "x0", access_instr->dest);
      fprintf(fout, "  mov x1, x0\n");
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  bl FbleRetainValue\n");
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;

      // Jump table data for jumping to the right fble pc.
      fprintf(fout, "  .section .data\n");
      fprintf(fout, "  .align 3\n");
      fprintf(fout, ".L._Run_%p.%zi.pcs:\n", code, pc);
      for (size_t i = 0; i < select_instr->jumps.size; ++i) {
        fprintf(fout, "  .xword .L._Run_%p.pc.%zi\n", (void*)code, pc + 1 + select_instr->jumps.xs[i]);
      }

      // Get the union value.
      fprintf(fout, "  .text\n");
      GetFrameVar(fout, "x0", select_instr->condition);
      fprintf(fout, "  bl FbleStrictValue\n");

      // Abort if the union object is NULL.
      fprintf(fout, "  cbnz x0, .L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, ".L.UndefinedUnionSelect", select_instr->loc);

      fprintf(fout, ".L.%p.%zi.ok:\n", code, pc);
      fprintf(fout, "  bl FbleUnionValueTag\n");
      fprintf(fout, "  lsl x0, x0, #3\n");    // x0 = 8 * (uv->tag)
      Adr(fout, "x1", ".L._Run_%p.%zi.pcs", code, pc); // x1 = pcs
      fprintf(fout, "  add x0, x0, x1\n");   // x0 = &pcs[uv->tag] 
      fprintf(fout, "  ldr x0, [x0]\n");     // x0 = pcs[uv->tag]
      fprintf(fout, "  br x0\n");            // goto pcs[uv->tag]
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "  goto pc_%zi\n", pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      fprintf(fout, "  .section .data\n");
      fprintf(fout, "  .align 3\n");
      fprintf(fout, ".L._Run_%p.%zi.exe:\n", code, pc);
      fprintf(fout, "  .xword 1\n");                          // .refcount
      fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MAGIC);  // .magic
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.args);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.statics);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.locals);
      fprintf(fout, "  .xword %zi\n", func_instr->code->_base.profile);
      fprintf(fout, "  .xword 0\n"); // .profile_blocks.size
      fprintf(fout, "  .xword 0\n"); // .profile_blocks.xs

      FbleName function_block = profile_blocks.xs[func_instr->code->_base.profile];
      char function_label[SizeofSanitizedString(function_block.name->str)];
      SanitizeString(function_block.name->str, function_label);
      fprintf(fout, "  .xword _Run.%p.%s\n", (void*)func_instr->code, function_label);
      fprintf(fout, "  .xword _Abort.%p.%s\n", (void*)func_instr->code, function_label);
      fprintf(fout, "  .xword 0\n"); // .on_free

      fprintf(fout, "  .text\n");
      fprintf(fout, "  .align 2\n");

      // Allocate space for the statics array on the stack.
      size_t sp_offset = StackBytesForCount(func_instr->code->_base.statics);
      fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
      for (size_t i = 0; i < func_instr->code->_base.statics; ++i) {
        GetFrameVar(fout, "x0", func_instr->scope.xs[i]);
        fprintf(fout, "  str x0, [SP, #%zi]\n", sizeof(FbleValue*) * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      Adr(fout, "x1", ".L._Run_%p.%zi.exe", code, pc);
      fprintf(fout, "  mov x2, R_PROFILE_BASE_ID\n");
      fprintf(fout, "  mov x3, SP\n");
      fprintf(fout, "  bl FbleNewFuncValue\n");
      SetFrameVar(fout, "x0", func_instr->dest);

      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      GetFrameVar(fout, "x0", call_instr->func);
      fprintf(fout, "  bl FbleStrictValue\n");
      fprintf(fout, "  mov R_SCRATCH_0, x0\n");

      fprintf(fout, "  cbnz R_SCRATCH_0, .L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, ".L.UndefinedFunctionValue", call_instr->loc);

      fprintf(fout, ".L.%p.%zi.ok:\n", code, pc);

      // Allocate space for the arguments array on the stack.
      size_t sp_offset = StackBytesForCount(call_instr->args.size);
      fprintf(fout, "  sub SP, SP, %zi\n", sp_offset);
      for (size_t i = 0; i < call_instr->args.size; ++i) {
        GetFrameVar(fout, "x0", call_instr->args.xs[i]);
        fprintf(fout, "  str x0, [SP, #%zi]\n", sizeof(FbleValue*) * i);
      }

      if (call_instr->exit) {
        fprintf(fout, "  mov x0, R_HEAP\n");
        fprintf(fout, "  mov x1, R_SCRATCH_0\n");
        fprintf(fout, "  bl FbleRetainValue\n");

        for (size_t i = 0; i < call_instr->args.size; ++i) {
          // We need to do a Retain on every arg from statics. For args from
          // locals, we don't need to do a Retain on the arg the first time we
          // see the local, because we can transfer the caller's ownership of
          // the local to the callee for that arg.
          bool retain = call_instr->args.xs[i].section != FBLE_LOCALS_FRAME_SECTION;
          for (size_t j = 0; j < i; ++j) {
            if (call_instr->args.xs[i].section == call_instr->args.xs[j].section
                && call_instr->args.xs[i].index == call_instr->args.xs[j].index) {
              retain = true;
              break;
            }
          }

          if (retain) {
            fprintf(fout, "  mov x0, R_HEAP\n");
            fprintf(fout, "  ldr x1, [SP, #%zi]\n", sizeof(FbleValue*)*i);
            fprintf(fout, "  bl FbleRetainValue\n");
          }
        }

        if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
          fprintf(fout, "  mov x0, R_HEAP\n");
          fprintf(fout, "  ldr x1, [R_LOCALS, #%zi]\n", sizeof(FbleValue*)*call_instr->func.index);
          fprintf(fout, "  bl FbleReleaseValue\n");
        }

        fprintf(fout, "  mov x0, R_HEAP\n");
        fprintf(fout, "  mov x1, R_SCRATCH_0\n");                   // func
        fprintf(fout, "  mov x2, SP\n");                            // args
        fprintf(fout, "  ldr x3, [SP, #%zi]\n", sp_offset + offsetof(RunStackFrame, thread));
        fprintf(fout, "  bl FbleThreadTailCall\n");

        fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
        fprintf(fout, "  mov x0, R_SCRATCH_0\n");

        // Restore stack and frame pointer and do a tail call.
        fprintf(fout, "  bl FbleFuncValueExecutable\n");
        fprintf(fout, "  ldr x4, [x0, #%zi]\n", offsetof(FbleExecutable, run));
        fprintf(fout, "  mov x0, R_HEAP\n");
        fprintf(fout, "  ldr x1, [SP, #%zi]\n", offsetof(RunStackFrame, thread));
        fprintf(fout, "  ldr R_HEAP, [SP, #%zi]\n", offsetof(RunStackFrame, r_heap_save));
        fprintf(fout, "  ldr R_LOCALS, [SP, #%zi]\n", offsetof(RunStackFrame, r_locals_save));
        fprintf(fout, "  ldr R_STATICS, [SP, #%zi]\n", offsetof(RunStackFrame, r_statics_save));
        fprintf(fout, "  ldr R_PROFILE, [SP, #%zi]\n", offsetof(RunStackFrame, r_profile_save));
        fprintf(fout, "  ldr R_PROFILE_BASE_ID, [SP, #%zi]\n", offsetof(RunStackFrame, r_profile_base_id_save));
        fprintf(fout, "  ldr R_SCRATCH_0, [SP, #%zi]\n", offsetof(RunStackFrame, r_scratch_0_save));
        fprintf(fout, "  ldr R_SCRATCH_1, [SP, #%zi]\n", offsetof(RunStackFrame, r_scratch_1_save));
        fprintf(fout, "  ldp FP, LR, [SP], #%zi\n", sizeof(RunStackFrame));
        fprintf(fout, "  br x4\n");
        return;
      }

      // stack->pc = pc + 1
      fprintf(fout, "  ldr x0, [SP, #%zi]\n", sp_offset + offsetof(RunStackFrame, thread));
      fprintf(fout, "  ldr x0, [x0, #%zi]\n", offsetof(FbleThread, stack));
      fprintf(fout, "  mov x1, #%zi\n", pc + 1);  // x1 = pc + 1
      fprintf(fout, "  str x1, [x0, #%zi]\n", offsetof(FbleStack, pc));

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  add x1, R_LOCALS, #%zi\n", sizeof(FbleValue*)*call_instr->dest);
      fprintf(fout, "  mov x2, R_SCRATCH_0\n");   // func
      fprintf(fout, "  mov x3, SP\n");
      fprintf(fout, "  ldr x4, [SP, #%zi]\n", sp_offset + offsetof(RunStackFrame, thread)); // thread
      fprintf(fout, "  bl FbleThreadCall\n");
      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);

      // Call the function repeatedly for as long as it results in
      // FBLE_EXEC_CONTINUED.
      fprintf(fout, ".L._Run_.%p.call.%zi:\n", code, pc);
      fprintf(fout, "  mov x0, R_SCRATCH_0\n");   // func
      fprintf(fout, "  bl FbleFuncValueExecutable\n");
      fprintf(fout, "  ldr x4, [x0, #%zi]\n", offsetof(FbleExecutable, run));
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  ldr x1, [SP, #%zi]\n", offsetof(RunStackFrame, thread));
      fprintf(fout, "  blr x4\n");               // call run function
      fprintf(fout, "  cmp x0, %i\n", FBLE_EXEC_CONTINUED);
      fprintf(fout, "  b.eq .L._Run_.%p.call.%zi\n", code, pc);

      // If the called function finished, continue to the next instruction.
      fprintf(fout, "  cmp x0, %i\n", FBLE_EXEC_FINISHED);
      fprintf(fout, "  b.eq .L._Run_%p.pc.%zi\n", code, pc + 1);

      // If the called function aborted or yielded or blocked, return now.
      fprintf(fout, "  b .L._Run_.%p.exit\n", code);
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      fprintf(fout, "  locals[%zi] = %s[%zi];\n",
          copy_instr->dest,
          section[copy_instr->source.section],
          copy_instr->source.index);
      fprintf(fout, "  FbleRetainValue(heap, locals[%zi]);\n", copy_instr->dest);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      fprintf(fout, "  locals[%zi] = FbleNewRefValue(heap);\n", ref_instr->dest);
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      FbleRefDefInstr* ref_instr = (FbleRefDefInstr*)instr;

      FbleFrameIndex ref_index = {
        .section = FBLE_LOCALS_FRAME_SECTION,
        .index = ref_instr->ref
      };

      fprintf(fout, "  mov x0, R_HEAP\n");
      GetFrameVar(fout, "x1", ref_index);
      GetFrameVar(fout, "x2", ref_instr->value);
      fprintf(fout, "  bl FbleAssignRefValue\n");
      fprintf(fout, "  cbnz x0, .L.%p.%zi.ok\n", code, pc);
      ReturnAbort(fout, code, pc, ".L.VacuousValue", ref_instr->loc);
      fprintf(fout, ".L.%p.%zi.ok:\n", code, pc);
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
      fprintf(fout, "  ldr x1, [SP, #%zi]\n", offsetof(RunStackFrame, thread));
      fprintf(fout, "  mov x2, R_SCRATCH_0\n");
      fprintf(fout, "  bl FbleThreadReturn\n");

      fprintf(fout, "  mov x0, #%i\n", FBLE_EXEC_FINISHED);
      fprintf(fout, "  b .L._Run_.%p.exit\n", code);
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      Adr(fout, "x0", "FbleGenericTypeValue");
      fprintf(fout, "  ldr x0, [x0]\n");
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

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      size_t argc = list_instr->args.size;

      // Allocate space on the stack for the array of arguments.
      size_t sp_offset = StackBytesForCount(argc);
      fprintf(fout, "  sub SP, SP, #%zi\n", sp_offset);
      for (size_t i = 0; i < argc; ++i) {
        GetFrameVar(fout, "x9", list_instr->args.xs[i]);
        fprintf(fout, "  str x9, [SP, #%zi]\n", 8 * i);
      }

      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", argc);
      fprintf(fout, "  mov x2, SP\n");
      fprintf(fout, "  bl FbleNewListValue\n");

      SetFrameVar(fout, "x0", list_instr->dest);
      fprintf(fout, "  add SP, SP, #%zi\n", sp_offset);
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      size_t argc = literal_instr->letters.size;

      fprintf(fout, "  .section .data\n");
      fprintf(fout, "  .align 3\n");
      fprintf(fout, ".L._Run_%p.%zi.letters:\n", code, pc);
      for (size_t i = 0; i < argc; ++i) {
        fprintf(fout, "  .xword %zi\n", literal_instr->letters.xs[i]);
      }

      fprintf(fout, "  .text\n");
      fprintf(fout, "  .align 2\n");
      fprintf(fout, "  mov x0, R_HEAP\n");
      fprintf(fout, "  mov x1, %zi\n", argc);
      Adr(fout, "x2", ".L._Run_%p.%zi.letters", code, pc);
      fprintf(fout, "  bl FbleNewLiteralValue\n");
      SetFrameVar(fout, "x0", literal_instr->dest);
      return;
    }
  }
}

// EmitCode --
//   Generate code to execute an FbleCode block.
//
// Inputs:
//   fout - the output stream to write the code to.
//   profile_blocks - the list of profile block names for the module.
//   code - the block of code to generate a C function for.
//
// Side effects:
// * Outputs code to fout with two space indent.
static void EmitCode(FILE* fout, FbleNameV profile_blocks, FbleCode* code)
{
  FbleName function_block = profile_blocks.xs[code->_base.profile];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "#line %d\n", function_block.loc.line);
  fprintf(fout, "FbleExecStatus _Run_%p_%s(FbleValueHeap* heap, FbleThread* thread)\n", (void*)code, function_label);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleProfilethread* profile = thread->profile;\n");
  fprintf(fout, "  FbleStack* stack = thread->stack;\n");
  fprintf(fout, "  FbleValue** l = stack->locals;\n");
  fprintf(fout, "  FbleValue* func = stack->func;\n");
  fprintf(fout, "  FbleValue** s = FbleFuncValueStatics(func);\n");
  fprintf(fout, "  size_t profile_base_id = FbleFuncValueProfileBaseId(func);\n");
  fprintf(fout, "  FbleValue* x = NULL;\n");

  // Emit code for each fble instruction
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "pc_%zi:\n", i);
    EmitInstr(fout, profile_blocks, code, i, code->instrs.xs[i]);
  }
  fprintf(fout, "}\n");
}

// EmitInstrForAbort --
//   Generate code to execute an instruction for the purposes of abort.
//
// Inputs:
//   fout - the output stream to write the code to.
//   code - pointer to the current code block, for referencing labels.
//   pc - the program counter of the instruction.
//   instr - the instruction to execute.
//
// Side effects:
// * Outputs code to fout.
static void EmitInstrForAbort(FILE* fout, void* code, size_t pc, FbleInstr* instr)
{
  switch (instr->tag) {
    case FBLE_DATA_TYPE_INSTR: {
      FbleDataTypeInstr* dt_instr = (FbleDataTypeInstr*)instr;
      SetFrameVar(fout, "XZR", dt_instr->dest);
      return;
    }

    case FBLE_STRUCT_VALUE_INSTR: {
      FbleStructValueInstr* struct_instr = (FbleStructValueInstr*)instr;
      SetFrameVar(fout, "XZR", struct_instr->dest);
      return;
    }

    case FBLE_UNION_VALUE_INSTR: {
      FbleUnionValueInstr* union_instr = (FbleUnionValueInstr*)instr;
      SetFrameVar(fout, "XZR", union_instr->dest);
      return;
    }

    case FBLE_STRUCT_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      SetFrameVar(fout, "XZR", access_instr->dest);
      return;
    }

    case FBLE_UNION_ACCESS_INSTR: {
      FbleAccessInstr* access_instr = (FbleAccessInstr*)instr;
      SetFrameVar(fout, "XZR", access_instr->dest);
      return;
    }

    case FBLE_UNION_SELECT_INSTR: {
      FbleUnionSelectInstr* select_instr = (FbleUnionSelectInstr*)instr;
      fprintf(fout, "  b .L._Abort_%p.pc.%zi\n", code, pc + 1 + select_instr->jumps.xs[0]);
      return;
    }

    case FBLE_JUMP_INSTR: {
      FbleJumpInstr* jump_instr = (FbleJumpInstr*)instr;
      fprintf(fout, "  b .L._Abort_%p.pc.%zi\n", code, pc + 1 + jump_instr->count);
      return;
    }

    case FBLE_FUNC_VALUE_INSTR: {
      FbleFuncValueInstr* func_instr = (FbleFuncValueInstr*)instr;
      SetFrameVar(fout, "XZR", func_instr->dest);
      return;
    }

    case FBLE_CALL_INSTR: {
      FbleCallInstr* call_instr = (FbleCallInstr*)instr;
      if (call_instr->exit) {
        if (call_instr->func.section == FBLE_LOCALS_FRAME_SECTION) {
          fprintf(fout, "  mov x0, R_HEAP\n");
          GetFrameVar(fout, "x1", call_instr->func);
          fprintf(fout, "  bl FbleReleaseValue\n");
          SetFrameVar(fout, "XZR", call_instr->func.index);
        }

        for (size_t i = 0; i < call_instr->args.size; ++i) {
          if (call_instr->args.xs[i].section == FBLE_LOCALS_FRAME_SECTION) {
            fprintf(fout, "  mov x0, R_HEAP\n");
            GetFrameVar(fout, "x1", call_instr->args.xs[i]);
            fprintf(fout, "  bl FbleReleaseValue\n");
            SetFrameVar(fout, "XZR", call_instr->args.xs[i].index);
          }
        }

        fprintf(fout, "  ldr x0, [SP, #%zi]\n", offsetof(AbortStackFrame, stack));
        fprintf(fout, "  ldr x1, [x0, #%zi]\n", offsetof(FbleStack, result));
        fprintf(fout, "  str XZR, [x1]\n");       // stack->result = NULL;
      }

      SetFrameVar(fout, "XZR", call_instr->dest);
      return;
    }

    case FBLE_COPY_INSTR: {
      FbleCopyInstr* copy_instr = (FbleCopyInstr*)instr;
      SetFrameVar(fout, "XZR", copy_instr->dest);
      return;
    }

    case FBLE_REF_VALUE_INSTR: {
      FbleRefValueInstr* ref_instr = (FbleRefValueInstr*)instr;
      SetFrameVar(fout, "XZR", ref_instr->dest);
      return;
    }

    case FBLE_REF_DEF_INSTR: {
      return;
    }

    case FBLE_RETURN_INSTR: {
      FbleReturnInstr* return_instr = (FbleReturnInstr*)instr;
      switch (return_instr->result.section) {
        case FBLE_STATICS_FRAME_SECTION: break;
        case FBLE_LOCALS_FRAME_SECTION: {
          fprintf(fout, "  mov x0, R_HEAP\n");
          GetFrameVar(fout, "x1", return_instr->result);
          fprintf(fout, "  bl FbleReleaseValue\n");
          break;
        }
      }

      fprintf(fout, "  ldr x0, [SP, #%zi]\n", offsetof(AbortStackFrame, stack));
      fprintf(fout, "  ldr x1, [x0, #%zi]\n", offsetof(FbleStack, result));
      fprintf(fout, "  str XZR, [x1]\n");       // stack->result = NULL;

      fprintf(fout, "  b .L._Abort_.%p.exit\n", code);
      return;
    }

    case FBLE_TYPE_INSTR: {
      FbleTypeInstr* type_instr = (FbleTypeInstr*)instr;
      SetFrameVar(fout, "XZR", type_instr->dest);
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

    case FBLE_LIST_INSTR: {
      FbleListInstr* list_instr = (FbleListInstr*)instr;
      SetFrameVar(fout, "XZR", list_instr->dest);
      return;
    }

    case FBLE_LITERAL_INSTR: {
      FbleLiteralInstr* literal_instr = (FbleLiteralInstr*)instr;
      SetFrameVar(fout, "XZR", literal_instr->dest);
      return;
    }
  }
}

// EmitCodeForAbort --
//   Generate code to abort an FbleCode block.
//
// Inputs:
//   fout - the output stream to write the code to.
//   profile_blocks - the list of profile block names for the module.
//   code - the block of code to generate assembly.
//
// Side effects:
// * Outputs generated code to the given output stream.
static void EmitCodeForAbort(FILE* fout, FbleNameV profile_blocks, FbleCode* code)
{
  // Jump table data for jumping to the right fble pc.
  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, ".L._Abort_%p.pcs:\n", (void*)code);
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, "  .xword .L._Abort_%p.pc.%zi\n", (void*)code, i);
  }

  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  FbleName function_block = profile_blocks.xs[code->_base.profile];
  char function_label[SizeofSanitizedString(function_block.name->str)];
  SanitizeString(function_block.name->str, function_label);
  fprintf(fout, "_Abort.%p.%s:\n", (void*)code, function_label);

  // Set up stack and frame pointer.
  fprintf(fout, "  stp FP, LR, [SP, #-%zi]!\n", sizeof(AbortStackFrame));
  fprintf(fout, "  mov FP, SP\n");

  // Save args to the stack for later reference.
  fprintf(fout, "  str x0, [SP, #%zi]\n", offsetof(AbortStackFrame, heap));
  fprintf(fout, "  str x1, [SP, #%zi]\n", offsetof(AbortStackFrame, stack)); // stack

  // Save callee saved registers for later restoration.
  fprintf(fout, "  str R_HEAP, [SP, #%zi]\n", offsetof(AbortStackFrame, r_heap_save));
  fprintf(fout, "  str R_LOCALS, [SP, #%zi]\n", offsetof(AbortStackFrame, r_locals_save));

  // Set up common registers.
  fprintf(fout, "  ldr x2, [x1, #%zi]\n", offsetof(FbleStack, func));
  fprintf(fout, "  mov R_HEAP, x0\n");
  fprintf(fout, "  add R_LOCALS, x1, #%zi\n", offsetof(FbleStack, locals));

  // Jump to the fble instruction at thread->stack->pc.
  fprintf(fout, "  ldr x0, [x1, #%zi]\n", offsetof(FbleStack, pc));
  fprintf(fout, "  lsl x0, x0, #3\n");     // x0 = 8 * (stack->pc)
  Adr(fout, "x1", ".L._Abort_%p.pcs", (void*)code); // x1 = pcs
  fprintf(fout, "  add x0, x0, x1\n");     // x0 = &pcs[stack->pc]
  fprintf(fout, "  ldr x0, [x0]\n");       // x0 = pcs[stack->pc]
  fprintf(fout, "  br x0\n");              // goto pcs[stack->pc]

  // Emit code for each fble instruction
  for (size_t i = 0; i < code->instrs.size; ++i) {
    fprintf(fout, ".L._Abort_%p.pc.%zi:\n", (void*)code, i);
    EmitInstrForAbort(fout, code, i, code->instrs.xs[i]);
  }

  // Restore stack and frame pointer and return whatever is in x0.
  // Common code for exiting a run function.
  fprintf(fout, ".L._Abort_.%p.exit:\n", (void*)code);
  fprintf(fout, "  ldr R_HEAP, [SP, #%zi]\n", offsetof(AbortStackFrame, r_heap_save));
  fprintf(fout, "  ldr R_LOCALS, [SP, #%zi]\n", offsetof(AbortStackFrame, r_locals_save));
  fprintf(fout, "  ldp FP, LR, [SP], #%zi\n", sizeof(AbortStackFrame));
  fprintf(fout, "  ret\n");
}

// SizeofSanitizedString --
//   Return the size of the label-sanitized version of a given string.
//
// Inputs:
//   str - the string to get the sanitized size of.
//
// Results:
//   The number of bytes needed for the sanitized version of the given string,
//   including nul terminator.
static size_t SizeofSanitizedString(const char* str)
{
  size_t size = 1;
  for (const char* p = str; *p != '\0'; p++) {
    size += isalnum((unsigned char)*p) ? 1 : 4;
  }
  return size;
}

// SanitizeString --
//   Return a version of the string suitable for use in labels.
//
// Inputs:
//   str - the string to sanitize.
//   dst - a character buffer of size SizeofSanitizedString(str) to write
//         the sanitized string to.
//
// Side effects:
//   Fills in dst with the sanitized version of the string.
static void SanitizeString(const char* str, char* dst)
{
  dst[0] = '\0';
  for (const char* p = str; *p != '\0'; p++) {
    char x[5];
    sprintf(x, isalnum((unsigned char)*p) ? "%c" : "_%02x_", *p);
    strcat(dst, x);
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
      if (isalnum((unsigned char)*p)) {
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
      if (isalnum((unsigned char)*p)) {
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

// FbleGenerateC -- see documentation in fble-compile.h
void FbleGenerateC(FILE* fout, FbleCompiledModule* module)
{
  FbleCodeV blocks;
  FbleVectorInit(blocks);

  LocV locs;
  FbleVectorInit(locs);

  CollectBlocksAndLocs(&blocks, &locs, module->code);

  fprintf(fout, "#line 1 \"%s\"\n", module->path->loc.source->str);

  FbleNameV profile_blocks = module->code->_base.profile_blocks;
  for (int i = 0; i < blocks.size; ++i) {
    // RunFunction
    EmitCode(fout, profile_blocks, blocks.xs[i]);
    EmitCodeForAbort(fout, profile_blocks, blocks.xs[i]);
  }

  LabelId label_id = 0;
  LabelId module_id = StaticExecutableModule(fout, &label_id, module);

  LabelId deps_id = label_id++;
  fprintf(fout, "FbleCompiledModuleFunction* " LABEL "[] = {\n", deps_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    FbleString* dep_name = LabelForPath(module->deps.xs[i]);
    fprintf(fout, "  %s\n", dep_name->str);
    FbleFreeString(dep_name);
  }
  fprintf(fout, "}\n");

  FbleString* func_name = LabelForPath(module->path);
  fprintf(fout, "void %s(FbleCompiledProgram* program)\n", func_name->str);
  fprintf(fout, "{\n");
  fprintf(fout, "  FbleLoadFromCompiled(program, " LABEL ", %zi, " LABEL ");\n",
      module_id, module->deps.size, deps_id);
  fprintf(fout, "}\n");
  FbleFreeString(func_name);

  FbleFree(blocks.xs);
  FbleFree(locs.xs);
}

// FbleGenerateCExport -- see documentation in fble-compile.h
void FbleGenerateCExport(FILE* fout, const char* name, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);

  fprintf(fout, "FbleValue* %s(FbleValueHeap* heap)\n", name);
  fprintf(fout, "{\n");
  fprintf(fout, "  return %s(heap)\n", module_name->str);
  fprintf(fout, "}\n");
  FbleFreeString(module_name);
}

// FbleGenerateCMain -- see documentation in fble-compile.h
void FbleGenerateCMain(FILE* fout, const char* main, FbleModulePath* path)
{
  FbleString* module_name = LabelForPath(path);

  fprintf(fout, "int main(int argc, const char** argv)\n");
  fprintf(fout, "{\n");
  fprintf(fout, "  return %s(argc, argv, %s);\n", main, module_name->str);
  fprintf(fout, "}\n");

  FbleFreeString(module_name);
}
