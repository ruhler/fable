// Program.c --
//
//   The file contains utilities for working with the abstract syntax for 
//   programs.

#include "Internal.h"

static bool NameIsDeclared(Env* env, Name name);


// NameIsDeclared --
//
//   Test whether a particular name is declared in an environment.
//
// Inputs:
//   env - The environment to look for the name in.
//   name - The name to look for.
//
// Result:
//   True if the name is declared in the environment, false otherwise.
//
// Side effects:
//   None.

static bool NameIsDeclared(Env* env, Name name) {
  return LookupType(env, name) != NULL
    || LookupFunc(env, name) != NULL
    || LookupProc(env, name) != NULL;
}


// NamesEqual --
//
//   Test whether two names are the same.
//
// Inputs:
//   a - The first name for the comparison.
//   b - The second name for the comparison.
//
// Result:
//   The value true if the names 'a' and 'b' are the same, false otherwise.
//
// Side effects:
//   None

bool NamesEqual(Name a, Name b)
{
  return strcmp(a, b) == 0;
}

// ReportError --
//
//   Prints a formatted error message to standard error with location
//   information. The format is the same as for printf, with the first
//   argument for conversion following the loc argument.
//   
// Inputs:
//   format - A printf style format string.
//   loc - The location associated with the error.
//   ... - Subsequent arguments for conversion based on the format string.
//
// Result:
//   None.
//
// Side effects:
//   Prints an error message to standard error.

void ReportError(const char* format, Loc* loc, ...)
{
  va_list ap;
  va_start(ap, loc);
  fprintf(stderr, "%s:%d:%d: error: ", loc->source, loc->line, loc->col);
  vfprintf(stderr, format, ap);
  va_end(ap);
}

// NewEnv --
//
//   Create a new, empty,  environment.
//
// Inputs:
//   alloc - The allocator to use to allocate the environment.
//
// Result:
//   A new, empty,  environment.
//
// Side effects:
//   Allocations are performed using the allocator as necessary to allocate
//   the environment.

Env* NewEnv(Allocator* alloc)
{
  Env* env = Alloc(alloc, sizeof(Env));
  env->types = NULL;
  env->funcs = NULL;
  env->procs = NULL;
  return env;
}

// LookupType --
//
//   Look up the declaration of the type with the given name in the given
//   environment.
// 
// Inputs:
//   env - The environment to look up the type in.
//   name - The name of the type to look up.
//
// Result:
//   The declaration for the type with the given name, or NULL if there is no
//   type with the given name in the given environment.
//
// Side effects:
//   None.

Type* LookupType(Env* env, Name name)
{
  for (TypeEnv* tenv = env->types; tenv != NULL; tenv = tenv->next) {
    if (NamesEqual(tenv->decl->name.name, name)) {
      return tenv->decl;
    }
  }
  return NULL;
}

// LookupFunc --
//
//   Look up the declaration of the function with the given name in the given
//   environment.
// 
// Inputs:
//   env - The environment to look up the type in.
//   name - The name of the function to look up.
//
// Result:
//   The declaration for the function with the given name, or NULL if there is
//   no function with the given name in the given environment.
//
// Side effects:
//   None.
Func* LookupFunc(Env* env, Name name)
{
  for (FuncEnv* fenv = env->funcs; fenv != NULL; fenv = fenv->next) {
    if (NamesEqual(fenv->decl->name.name, name)) {
      return fenv->decl;
    }
  }
  return NULL;
}

// LookupProc --
//
//   Look up the declaration of the process with the given name in the given
//   environment.
//
// Inputs:
//   env - The environment to look up the type in.
//   name - The name of the process to look up.
//
// Result:
//   The declaration for the process with the given name, or NULL if there is
//   no process with the given name in the given environment.
//
// Side effects:
//   None.

Proc* LookupProc(Env* env, Name name)
{
  for (ProcEnv* penv = env->procs; penv != NULL; penv = penv->next) {
    if (NamesEqual(penv->decl->name.name, name)) {
      return penv->decl;
    }
  }
  return NULL;
}

// AddType --
//  
//   Add a type declaration to the given environment.
//
// Inputs:
//   alloc - The allocator to use for allocations.
//   env - The environment to add the type declaration to.
//   type - The type declaration to add.
//
// Result:
//   True if the type was successfully added, false otherwise.
//
// Side effects:
//   The type declaration is added to the environment. If the type declaration
//   could not be added to the environment, an error message is printed to
//   standard error explaining the problem.

bool AddType(Allocator* alloc, Env* env, Type* type)
{
  if (NameIsDeclared(env, type->name.name)) {
    ReportError("Multiple declarations for %s.\n",
       type->name.loc, type->name.name);
    return false;
  }

  TypeEnv* types = Alloc(alloc, sizeof(TypeEnv));
  types->decl = type;
  types->next = env->types;
  env->types = types;
  return true;
}

// AddFunc --
//  
//   Add a function declaration to the given environment.
//
// Inputs:
//   alloc - The allocator to use for allocations.
//   env - The environment to add the function declaration to.
//   func - The function declaration to add.
//
// Result:
//   True if the function was successfully added, false otherwise.
//
// Side effects:
//   The function declaration is added to the environment. If the function
//   could not be added to the environment, an error message is printed to
//   standard error describing the problem.

bool AddFunc(Allocator* alloc, Env* env, Func* func)
{
  if (NameIsDeclared(env, func->name.name)) {
    ReportError("Multiple declarations for %s.\n",
       func->name.loc, func->name.name);
    return false;
  }
  FuncEnv* funcs = Alloc(alloc, sizeof(FuncEnv));
  funcs->decl = func;
  funcs->next = env->funcs;
  env->funcs = funcs;
  return true;
}

// AddProc --
//
//   Add a process declaration to the given environment.
//
// Inputs:
//   alloc - The allocator to use for allocations.
//   env - The environment to add the process declaration to.
//   proc - The process declaration to add.
//
// Result:
//   True if the process was successfully added, false otherwise.
//
// Side effects:
//   The process declaration is added to the environment. If the process
//   could not be added to the environment, an error message is printed to
//   standard error describing the problem.

bool AddProc(Allocator* alloc, Env* env, Proc* proc)
{
  if (NameIsDeclared(env, proc->name.name)) {
    ReportError("Multiple declarations for %s.\n",
       proc->name.loc, proc->name.name);
    return false;
  }
  ProcEnv* procs = Alloc(alloc, sizeof(ProcEnv));
  procs->decl = proc;
  procs->next = env->procs;
  env->procs = procs;
  return true;
}
