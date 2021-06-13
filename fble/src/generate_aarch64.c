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

static void EmitCode(FILE* fout, FbleCode* code);
static FbleString* CIdentifierForPath(FbleModulePath* path);

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
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "_Run_%p:\n", (void*)code);
  fprintf(fout, "  stp FP, LR, [SP, #-16]!\n");
  fprintf(fout, "  mov FP, SP\n");
  fprintf(fout, "  bl abort\n");  // TODO: Implement me.
  fprintf(fout, "  ldp FP, LR, [SP], #16\n");
  fprintf(fout, "  ret\n");
}

// CIdentifierForPath --
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
static FbleString* CIdentifierForPath(FbleModulePath* path)
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
    FbleString* dep_name = CIdentifierForPath(module->deps.xs[i]);
    fprintf(fout, "  .xword %s\n", dep_name->str);
    FbleFreeString(dep_name);
  }

  FbleString* func_name = CIdentifierForPath(module->path);
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

  FbleString* module_name = CIdentifierForPath(path);
  fprintf(fout, "  bl %s\n\n", module_name->str);
  FbleFreeString(module_name);

  fprintf(fout, "  ldp FP, LR, [SP], #16\n");
  fprintf(fout, "  ret\n");
}
