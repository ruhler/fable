/**
 * @file link.c
 *  Routines for linking fble modules together into FbleValue*.
 */

#include <fble/fble-link.h>

#include <assert.h>     // for assert
#include <string.h>     // for strrchr

#include <fble/fble-alloc.h>   // for FbleAlloc, etc.
#include <fble/fble-vector.h>  // for FbleInitVector, etc.

#include "code.h"       // for FbleCode
#include "interpret.h"  // for FbleNewInterpretedFuncValue

static FbleValue* LinkInterpreted(FbleValueHeap* heap, FbleProfile* profile, FbleSearchPath* search_path, FbleModulePath* module_path);

static void LoadGenerated(FbleGeneratedModuleV* collected, FbleGeneratedModule* module);
static FbleValue* LinkGenerated(FbleValueHeap* heap, FbleProfile* profile, FbleGeneratedModule* module);


static FbleValue* LinkInterpreted(FbleValueHeap* heap, FbleProfile* profile, FbleSearchPath* search_path, FbleModulePath* module_path)
{
  // Load the source files.
  FbleLoadedProgram* program = FbleLoad(search_path, module_path, NULL);
  if (program == NULL) {
    return NULL;
  }

  // Compiled to bytecode.
  FbleCompiledProgram* compiled = FbleCompileProgram(program);
  FbleFreeLoadedProgram(program);
  if (compiled == NULL) {
   return NULL;
  }

  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleName main_block = {
    .name = FbleNewString("<main>"),
    .loc = FbleNewLoc(__FILE__, __LINE__-2, 12)
  };
  FbleBlockId main_id = FbleAddBlockToProfile(profile, main_block);

  size_t modulec = compiled->modules.size;
  FbleCode* code = FbleNewCode(0, modulec, modulec, main_id);
  FbleValue* funcs[modulec];
  for (size_t i = 0; i < modulec; ++i) {
    FbleCompiledModule* module = compiled->modules.xs[i];

    size_t profile_block_offset = FbleAddBlocksToProfile(profile, module->profile_blocks);
    funcs[i] = FbleNewInterpretedFuncValue(heap, module->code, profile_block_offset, NULL);

    FbleCallInstr* call = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
    call->loc.source = FbleNewString(__FILE__);
    call->loc.line = __LINE__ - 1;
    call->loc.col = 5;
    call->func.tag = FBLE_STATIC_VAR;
    call->func.index = i;
    FbleInitVector(call->args);
    for (size_t d = 0; d < module->deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->deps.xs[d], compiled->modules.xs[v]->path)) {
          FbleVar var = { .tag = FBLE_LOCAL_VAR, .index = v };
          FbleAppendToVector(call->args, var);
          break;
        }
      }
    }
    assert(call->args.size == module->deps.size);

    call->dest = i;
    FbleAppendToVector(code->instrs, &call->_base);
  }

  FbleReleaseInstr* release_instr = FbleAllocInstr(FbleReleaseInstr, FBLE_RELEASE_INSTR);
  FbleInitVector(release_instr->targets);
  for (size_t i = 0; i + 1 < compiled->modules.size; ++i) {
    FbleAppendToVector(release_instr->targets, i);
  }
  FbleAppendToVector(code->instrs, &release_instr->_base);

  FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
  return_instr->result.tag = FBLE_LOCAL_VAR;
  return_instr->result.index = modulec - 1;
  FbleAppendToVector(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleValue* linked = FbleNewInterpretedFuncValue(heap, code, 0, funcs);
  for (size_t i = 0; i < modulec; ++i) {
    FbleReleaseValue(heap, funcs[i]);
  }
  FbleFreeCode(code);
  FbleFreeCompiledProgram(compiled);
  return linked;
}

/**
 * @func[LoadGenerated] Loads the list of generated modules for a program.
 *  Loads the module and all its dependencies in topological order, such that
 *  later modules in the list depend on earlier modules, but not the other way
 *  around.
 *
 *  @arg[FbleGeneratedModuleV*][collected] The modules loaded so far.
 *  @arg[FbleGeneratedModule*][module] The module to load.
 *  @sideeffects
 *   Adds the module and all dependencies to the list of loaded modules.
 */
static void LoadGenerated(FbleGeneratedModuleV* loaded, FbleGeneratedModule* module)
{
  // Check if we've already loaded the module.
  for (size_t i = 0; i < loaded->size; ++i) {
    if (FbleModulePathsEqual(loaded->xs[i]->path, module->path)) {
      return;
    }
  }

  // Load the dependencies.
  for (size_t i = 0; i < module->deps.size; ++i) {
    LoadGenerated(loaded, module->deps.xs[i]);
  }

  FbleAppendToVector(*loaded, module);
}

static FbleValue* LinkGenerated(FbleValueHeap* heap, FbleProfile* profile, FbleGeneratedModule* program)
{
  // Load the modules.
  FbleGeneratedModuleV modules;
  FbleInitVector(modules);
  LoadGenerated(&modules, program);

  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleName main_block = {
    .name = FbleNewString("<main>"),
    .loc = FbleNewLoc(__FILE__, __LINE__-2, 12)
  };
  FbleBlockId main_id = FbleAddBlockToProfile(profile, main_block);

  size_t modulec = modules.size;
  FbleCode* code = FbleNewCode(0, modulec, modulec, main_id);
  FbleValue* funcs[modulec];
  for (size_t i = 0; i < modulec; ++i) {
    FbleGeneratedModule* module = modules.xs[i];

    FbleExecutable* exe = module->executable;

    // TODO: Return error messages in these cases instead of assert failures?
    assert(exe->num_statics == 0 && "Module cannot have statics");
    assert(module->deps.size == exe->num_args && "Module args mismatch");

    size_t profile_block_offset = FbleAddBlocksToProfile(profile, module->profile_blocks);
    funcs[i] = FbleNewFuncValue(heap, exe, profile_block_offset, NULL);

    FbleCallInstr* call = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
    call->loc.source = FbleNewString(__FILE__);
    call->loc.line = __LINE__ - 1;
    call->loc.col = 5;
    call->func.tag = FBLE_STATIC_VAR;
    call->func.index = i;
    FbleInitVector(call->args);
    for (size_t d = 0; d < module->deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->deps.xs[d]->path, modules.xs[v]->path)) {
          FbleVar var = { .tag = FBLE_LOCAL_VAR, .index = v };
          FbleAppendToVector(call->args, var);
          break;
        }
      }
    }
    assert(call->args.size == module->deps.size);

    call->dest = i;
    FbleAppendToVector(code->instrs, &call->_base);
  }

  FbleReleaseInstr* release_instr = FbleAllocInstr(FbleReleaseInstr, FBLE_RELEASE_INSTR);
  FbleInitVector(release_instr->targets);
  for (size_t i = 0; i + 1 < modules.size; ++i) {
    FbleAppendToVector(release_instr->targets, i);
  }
  FbleAppendToVector(code->instrs, &release_instr->_base);

  FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
  return_instr->result.tag = FBLE_LOCAL_VAR;
  return_instr->result.index = modulec - 1;
  FbleAppendToVector(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleValue* linked = FbleNewInterpretedFuncValue(heap, code, 0, funcs);
  for (size_t i = 0; i < modulec; ++i) {
    FbleReleaseValue(heap, funcs[i]);
  }
  FbleFreeCode(code);
  FbleFreeVector(modules);
  return linked;
}

// FbleLink -- see documentation in fble-link.h
FbleValue* FbleLink(FbleValueHeap* heap, FbleProfile* profile, FbleGeneratedModule* module, FbleSearchPath* search_path, FbleModulePath* module_path)
{
  if (module != NULL) {
    return LinkGenerated(heap, profile, module);
  }

  assert(module_path != NULL);
  FbleValue* linked = LinkInterpreted(heap, profile, search_path, module_path);
  return linked;
}
// See documentation in fble-link.h
void FblePrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FbleGeneratedModule* module)
{
  if (module != NULL) {
    const char* binary_name = strrchr(arg0, '/');
    if (binary_name == NULL) {
      binary_name = arg0;
    } else {
      binary_name++;
    }

    fprintf(stream, "%s: %s -m ", binary_name, tool);
    FblePrintModulePath(stream, module->path);
    fprintf(stream, " (compiled)\n");
  }
}
