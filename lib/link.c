/**
 * @file link.c
 *  Routines for linking fble modules together into FbleValue*.
 */

#include <fble/fble-link.h>

#include <assert.h>     // for assert
#include <string.h>     // for strrchr

#include <fble/fble-alloc.h>     // for FbleAlloc, etc.
#include <fble/fble-interpret.h> // for FbleInterpret
#include <fble/fble-vector.h>    // for FbleInitVector, etc.

#include "code.h"

static FbleValue* Link(FbleValueHeap* heap, FbleExecutableProgram* program, FbleProfile* profile);
static FbleValue* LinkFromSource(FbleValueHeap* heap, FbleSearchPath* search_path, FbleModulePath* module_path, FbleProfile* profile);
static FbleValue* LinkFromCompiled(FbleCompiledModuleFunction* module, FbleValueHeap* heap, FbleProfile* profile);

/**
 * @func[Link] Links an fble program.
 *  Links the modules of an executable program together into a single
 *  FbleValue representing a zero-argument function that can be used to
 *  compute the value of the program.
 *
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleExecutableProgram*] program
 *   The executable program to link.
 *  @arg[FbleProfile*] profile
 *   Profile to construct for the linked program. May be NULL.
 *
 *  @returns FbleValue*
 *   An FbleValue representing a zero-argument function that can be used to
 *   compute the value of the program.
 *
 *  @sideeffects
 *   Allocates an FbleValue that should be freed using FbleReleaseValue when
 *   no longer needed.
 */
static FbleValue* Link(FbleValueHeap* heap, FbleExecutableProgram* program, FbleProfile* profile)
{
  size_t modulec = program->modules.size;

  FbleName main_block = {
    .name = FbleNewString("<main>"),
    .loc = FbleNewLoc(__FILE__, __LINE__-2, 12)
  };
  FbleBlockId main_id = FbleAddBlockToProfile(profile, main_block);

  // Make an FbleFuncValue for each module that computes the value of the
  // module given values of the modules it depends on.
  FbleValue* funcs[modulec];
  for (size_t i = 0; i < modulec; ++i) {
    FbleExecutableModule* module = program->modules.xs[i];
    FbleExecutable* exe = module->executable;

    // TODO: Return error messages in these cases instead of assert failures?
    assert(exe->num_statics == 0 && "Module cannot have statics");
    assert(module->deps.size == exe->num_args && "Module args mismatch");

    size_t profile_block_offset = FbleAddBlocksToProfile(profile, module->profile_blocks);
    funcs[i] = FbleNewFuncValue(heap, exe, profile_block_offset, NULL);
  }

  // Write some code to call each of module functions in turn with the
  // appropriate module arguments. The function for module i will be static
  // variable i, and the value computed for module i will be local variable i.
  FbleCode* code = FbleNewCode(0, modulec, modulec, main_id);

  for (size_t i = 0; i < program->modules.size; ++i) {
    FbleExecutableModule* module = program->modules.xs[i];

    FbleCallInstr* call = FbleAllocInstr(FbleCallInstr, FBLE_CALL_INSTR);
    call->loc.source = FbleNewString(__FILE__);
    call->loc.line = __LINE__ - 1;
    call->loc.col = 5;
    call->func.tag = FBLE_STATIC_VAR;
    call->func.index = i;
    FbleInitVector(call->args);
    for (size_t d = 0; d < module->deps.size; ++d) {
      for (size_t v = 0; v < i; ++v) {
        if (FbleModulePathsEqual(module->deps.xs[d], program->modules.xs[v]->path)) {
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
  for (size_t i = 0; i + 1 < program->modules.size; ++i) {
    FbleAppendToVector(release_instr->targets, i);
  }
  FbleAppendToVector(code->instrs, &release_instr->_base);

  FbleReturnInstr* return_instr = FbleAllocInstr(FbleReturnInstr, FBLE_RETURN_INSTR);
  return_instr->result.tag = FBLE_LOCAL_VAR;
  return_instr->result.index = modulec - 1;
  FbleAppendToVector(code->instrs, &return_instr->_base);

  // Wrap that all up into an FbleFuncValue.
  FbleValue* linked = FbleNewFuncValue(heap, &code->_base, 0, funcs);
  for (size_t i = 0; i < modulec; ++i) {
    FbleReleaseValue(heap, funcs[i]);
  }
  FbleFreeCode(code);

  return linked;
}

/**
 * @func[LinkFromSource] Loads an fble program from source.
 *  Loads, compiles, and links a full program from source.
 *
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleSearchPath*] search_path
 *   The search path to use for locating .fble files.
 *  @arg[FbleModulePath*] module_path
 *   The module path for the main module to load. Borrowed.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *
 *  @returns FbleValue*
 *   A zero-argument function that computes the value of the program when
 *   executed, or NULL in case of error.
 *
 *  @sideeffects
 *   @i Prints an error message to stderr if the program fails to load.
 *   @item
 *    The user should call FbleReleaseValue on the returned value when it is
 *    no longer needed.
 */
static FbleValue* LinkFromSource(FbleValueHeap* heap, FbleSearchPath* search_path, FbleModulePath* module_path, FbleProfile* profile)
{
  FbleLoadedProgram* program = FbleLoad(search_path, module_path, NULL);
  if (program == NULL) {
    return NULL;
  }

  FbleCompiledProgram* compiled = FbleCompileProgram(program);
  FbleFreeLoadedProgram(program);
  if (compiled == NULL) {
   return NULL;
  }

  FbleExecutableProgram* executable = FbleInterpret(compiled);
  FbleFreeCompiledProgram(compiled);

  FbleValue* linked = Link(heap, executable, profile);
  FbleFreeExecutableProgram(executable);
  return linked;
}

// See documentation in fble-link.h
void FbleLoadFromCompiled(FbleExecutableProgram* program, FbleExecutableModule* module, size_t depc, FbleCompiledModuleFunction** deps)
{
  // Don't do anything if the module has already been loaded.
  for (size_t i = 0; i < program->modules.size; ++i) {
    if (FbleModulePathsEqual(module->path, program->modules.xs[i]->path)) {
      return;
    }
  }

  // Otherwise, load its dependencies and add it to the list.
  for (size_t i = 0; i < depc; ++i) {
    deps[i](program);
  }

  module->refcount++;
  FbleAppendToVector(program->modules, module);
}

/**
 * @func[LinkFromCompiled] Loads and links a precompield fble program.
 *  @arg[FbleCompiledModuleFunction] module
 *   The compiled main module function.
 *  @arg[FbleValueHeap*] heap
 *   Heap to use for allocations.
 *  @arg[FbleProfile*] profile
 *   Profile to populate with blocks. May be NULL.
 *
 *  @returns FbleValue*
 *   A zero-argument fble function that computes the value of the program when
 *   executed.
 *
 *  @sideeffects
 *   The user should call FbleReleaseValue on the returned value when it is no
 *   longer needed.
 */
static FbleValue* LinkFromCompiled(FbleCompiledModuleFunction* module, FbleValueHeap* heap, FbleProfile* profile)
{
  FbleExecutableProgram* program = FbleAlloc(FbleExecutableProgram);
  FbleInitVector(program->modules);
  module(program);
  FbleValue* value = Link(heap, program, profile);
  FbleFreeExecutableProgram(program);
  return value;
}

// FbleLinkFromCompiledOrSource -- see documentation in fble-link.h
FbleValue* FbleLinkFromCompiledOrSource(FbleValueHeap* heap, FbleProfile* profile, FbleCompiledModuleFunction* module, FbleSearchPath* search_path, FbleModulePath* module_path)
{
  if (module != NULL) {
    return LinkFromCompiled(module, heap, profile);
  }

  assert(module_path != NULL);
  FbleValue* linked = LinkFromSource(heap, search_path, module_path, profile);
  return linked;
}
// See documentation in fble-link.h
void FblePrintCompiledHeaderLine(FILE* stream, const char* tool, const char* arg0, FbleCompiledModuleFunction* module)
{
  if (module != NULL) {
    const char* binary_name = strrchr(arg0, '/');
    if (binary_name == NULL) {
      binary_name = arg0;
    } else {
      binary_name++;
    }

    // Load the module to figure out the path to it.
    FbleExecutableProgram* program = FbleAlloc(FbleExecutableProgram);
    FbleInitVector(program->modules);
    module(program);
    FbleExecutableModule* mod = program->modules.xs[program->modules.size-1];

    fprintf(stream, "%s: %s -m ", binary_name, tool);
    FblePrintModulePath(stream, mod->path);
    fprintf(stream, " (compiled)\n");

    FbleFreeExecutableProgram(program);
  }
}
