// fble-execute.h --
//   Public header file for fble execution related types and functions.

#ifndef FBLE_EXECUTE_H_
#define FBLE_EXECUTE_H_

#include "fble-module-path.h"

// FbleExecutable --
//   Abstract type representing executable code.
typedef struct FbleExecutable FbleExecutable;

// FbleExecutableModule --
//   Represents an executable module.
// 
// Fields:
//   path - the path to the module.
//   deps - a list of distinct modules this module depends on.
//   executable - code to compute the value of the module, suitable for use in
//          the body of a that function takes the computed module values for
//          each module listed in module->deps as arguments to the function.
typedef struct {
  FbleModulePath* path;
  FbleModulePathV deps;
  FbleExecutable* executable;
} FbleExecutableModule;

// FbleExecutableModuleV -- A vector of FbleExecutableModule.
typedef struct {
  size_t size;
  FbleExecutableModule* xs;
} FbleExecutableModuleV;

// FbleExecutableProgram --
//   An executable program.
//
// The program is represented as a list of executable modules in topological
// dependancy order. Later modules in the list may depend on earlier modules
// in the list, but not the other way around.
//
// The last module in the list is the main program. The module path for the
// main module is /%.
typedef struct {
  FbleExecutableModuleV modules;
} FbleExecutableProgram;

// FbleFreeExecutableProgram --
//   Free resources associated with the given program.
//
// Inputs:
//   program - the program to free, may be NULL.
//
// Side effects:
//   Frees resources associated with the given program.
void FbleFreeExecutableProgram(FbleExecutableProgram* program);

#endif // FBLE_EXECUTE_H_
