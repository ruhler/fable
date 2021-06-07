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

// VarId --
//   Type representing a C variable name as an integer.
//
// The number is turned into a C variable name using printf format "v%x".
typedef unsigned int VarId;

typedef struct {
  size_t size;
  const char** xs;
} LocV;

static void AddLoc(const char* source, LocV* locs);
static void CollectBlocksAndLocs(FbleCodeV* blocks, LocV* locs, FbleCode* code);

static void StringLit(FILE* fout, const char* string);
static VarId StaticString(FILE* fout, VarId* var_id, const char* string);
static VarId StaticNames(FILE* fout, VarId* var_id, FbleNameV names);
static VarId StaticModulePath(FILE* fout, VarId* var_id, FbleModulePath* path);
static VarId StaticExecutableModule(FILE* fout, VarId* var_id, FbleCompiledModule* module);

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
//   var_id - pointer to next available variable id for use.
//   string - the value of the string.
//
// Returns:
//   A variable name of a local, static FbleString.
//
// Side effects:
//   Writes code to fout and allocates variable ids out of var_id.
static VarId StaticString(FILE* fout, VarId* var_id, const char* string)
{
  VarId id = (*var_id)++;

  fprintf(fout, "  .section .data\n");
  fprintf(fout, "  .align 3\n");                       // 64 bit alignment
  fprintf(fout, "v%x:\n", id);
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
//   var_id - pointer to next available variable id for use.
//   names - the value of the names.
//
// Returns:
//   A variable id of a local, static FbleNameV.xs.
//
// Side effects:
//   Writes code to fout and allocates variable ids out of var_id.
static VarId StaticNames(FILE* fout, VarId* var_id, FbleNameV names)
{
  VarId str_ids[names.size];
  VarId src_ids[names.size];
  for (size_t i = 0; i < names.size; ++i) {
    str_ids[i] = StaticString(fout, var_id, names.xs[i].name->str);
    src_ids[i] = StaticString(fout, var_id, names.xs[i].loc.source->str);
  }

  VarId id = (*var_id)++;
  fprintf(fout, "  .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, "v%x:\n", id);
  for (size_t i = 0; i < names.size; ++i) {
    fprintf(fout, "  .xword v%x\n", str_ids[i]);          // name
    fprintf(fout, "  .word %i\n", names.xs[i].space);     // space
    fprintf(fout, "  .zero 4\n");                         // padding
    fprintf(fout, "  .xword v%x\n", src_ids[i]);          // loc.src
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
//   var_id - pointer to next available variable id for use.
//   path - the FbleModulePath to generate code for.
//
// Results:
//   The variable id of a local, static FbleModulePath.
//
// Side effects:
// * Outputs code to fout.
// * Increments var_id based on the number of internal variables used.
static VarId StaticModulePath(FILE* fout, VarId* var_id, FbleModulePath* path)
{
  VarId src_id = StaticString(fout, var_id, path->loc.source->str);
  VarId names_id = StaticNames(fout, var_id, path->path);
  VarId path_id = (*var_id)++;

  fprintf(fout, "  .data\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, "v%x:\n", path_id);
  fprintf(fout, "  .xword 1\n");                  // .refcount
  fprintf(fout, "  .xword 2004903300\n");         // .magic
  fprintf(fout, "  .xword v%x\n", src_id);        // path->loc.src
  fprintf(fout, "  .word %i\n", path->loc.line);
  fprintf(fout, "  .word %i\n", path->loc.col);
  fprintf(fout, "  .xword %zi\n", path->path.size);
  fprintf(fout, "  .xword v%x\n", names_id);
  return path_id;
}

// StaticExecutableModule --
//   Generate code to declare a static FbleExecutableModule value.
//
// Inputs:
//   fout - the output stream to write the code to.
//   var_id - pointer to next available variable id for use.
//   module - the FbleCompiledModule to generate code for.
//
// Results:
//   The variable id of a local, static FbleExecutableModule.
//
// Side effects:
// * Outputs code to fout.
// * Increments var_id based on the number of internal variables used.
static VarId StaticExecutableModule(FILE* fout, VarId* var_id, FbleCompiledModule* module)
{
  VarId path_id = StaticModulePath(fout, var_id, module->path);

  VarId dep_ids[module->deps.size];
  for (size_t i = 0; i < module->deps.size; ++i) {
    dep_ids[i] = StaticModulePath(fout, var_id, module->deps.xs[i]);
  }

  VarId deps_xs_id = (*var_id)++;
  fprintf(fout, "  .section .data.rel.local\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, "v%x:\n", deps_xs_id);
  for (size_t i = 0; i < module->deps.size; ++i) {
    fprintf(fout, "  .xword v%x\n", dep_ids[i]);
  }

  VarId profile_blocks_xs_id = StaticNames(fout, var_id, module->code->_base.profile_blocks);

  VarId executable_id = (*var_id)++;
  fprintf(fout, "  .section .data.rel,\"aw\"\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, "v%x:\n", executable_id);
  fprintf(fout, "  .xword 1\n");                          // .refcount
  fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MAGIC);  // .magic
  fprintf(fout, "  .xword %zi\n", module->code->_base.args);
  fprintf(fout, "  .xword %zi\n", module->code->_base.statics);
  fprintf(fout, "  .xword %zi\n", module->code->_base.locals);
  fprintf(fout, "  .xword %zi\n", module->code->_base.profile);
  fprintf(fout, "  .xword %zi\n", module->code->_base.profile_blocks.size);
  fprintf(fout, "  .xword v%x\n", profile_blocks_xs_id);
  fprintf(fout, "  .xword _Run_%p\n", (void*)module->code);
  fprintf(fout, "  .xword _Abort_%p\n", (void*)module->code);
  fprintf(fout, "  .xword FbleExecutableNothingOnFree\n");

  VarId module_id = (*var_id)++;
  fprintf(fout, "  .section .data.rel.local\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, "v%x:\n", module_id);
  fprintf(fout, "  .xword 1\n");                                  // .refcount
  fprintf(fout, "  .xword %i\n", FBLE_EXECUTABLE_MODULE_MAGIC);   // .magic
  fprintf(fout, "  .xword v%x\n", path_id);                       // .path
  fprintf(fout, "  .xword %zi\n", module->deps.size);
  fprintf(fout, "  .xword v%x\n", deps_xs_id);
  fprintf(fout, "  .xword v%x\n", executable_id);
  return module_id;
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
    fprintf(fout, "  .text\n");
    fprintf(fout, "  .align 2\n");
    fprintf(fout, "_Run_%p:\n", (void*)blocks.xs[i]);
    fprintf(fout, "  b abort\n");  // TODO: Implement me.

    fprintf(fout, "  .text\n");
    fprintf(fout, "  .align 2\n");
    fprintf(fout, "_Abort_%p:\n", (void*)blocks.xs[i]);
    fprintf(fout, "  b abort\n");  // TODO: Implement me.
  }
  FbleFree(blocks.xs);
  FbleFree(locs.xs);

  VarId var_id = 0;
  VarId module_id = StaticExecutableModule(fout, &var_id, module);
  VarId deps_id = var_id++;

  fprintf(fout, "  .section .data.rel,\"aw\"\n");
  fprintf(fout, "  .align 3\n");
  fprintf(fout, "v%x:\n", deps_id);
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
  fprintf(fout, "  mov x1, v%x\n", module_id);
  fprintf(fout, "  mov x2, %zi\n", module->deps.size);
  fprintf(fout, "  mov x3, v%x\n", deps_id);
  fprintf(fout, "  b FbleLoadFromCompiled\n");
  FbleFreeString(func_name);
}

// FbleGenerateAArch64Export -- see documentation in fble-compile.h
void FbleGenerateAArch64Export(FILE* fout, const char* name, FbleModulePath* path)
{
  fprintf(fout, "  .text\n");
  fprintf(fout, "  .align 2\n");
  fprintf(fout, "  .global %s\n", name);
  fprintf(fout, "%s:\n", name);

  FbleString* module_name = CIdentifierForPath(path);
  fprintf(fout, "  b %s\n\n", module_name->str);
  FbleFreeString(module_name);
}
