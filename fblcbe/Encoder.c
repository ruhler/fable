// Encoder.c --
//
//   This file implements routines for encoding an fblc program in binary.

#include "Internal.h"

// IdForDecl -
//   
//   Return the identifier for the declaration with the given name.
//
// Inputs:
//   env - the program environment.
//   name - the name of the declaration to identify.
//
// Returns:
//   The identifier to use for the declaration.
//
// Side effects:
//   None. The behavior is undefined if name does not refer to a declaration
//   in the environment.

static uint32_t IdForDecl(const Env* env, Name name)
{
  // TODO: Linear search through the entire environment for every lookup seems
  // excessively slow. Consider using a more efficient data structure, such as
  // a hash table.

  // Note: The search order here corresponds to the order the declarations are
  // generated in the EncodeProgram function.
  size_t i = 0;
  for (TypeEnv types = env->types; types != NULL; types = types->next) {
    if (NamesEqual(name, types->decl->name.name)) {
      return i;
    }
    ++i;
  }

  for (FuncEnv funcs = env->funcs; funcs != NULL; funcs = funcs->next) {
    if (NamesEqual(name, funcs->decl->name.name)) {
      return i;
    }
    ++i;
  }

  for (ProcEnv procs = env->procs; procs != NULL; procs = procs->next) {
    if (NamesEqual(name, procs->decl->name.name)) {
      return i;
    }
    ++i;
  }

  assert(false && "Name not found in environment.");
  return -1;
}

// EncodeProgram -
//
//   Write the given program to the given bit stream.
//
// Inputs:
//   stream - The stream to output the program to.
//   env - The program to write.
//
// Returns:
//   None.
//
// Side effects:
//   The program is written to the bit stream.

void EncodeProgram(BitOutputStream* stream, const Env* env)
{
  bool initial = true;
  for (TypeEnv types = env->types; types != NULL; types = types->next) {
    if (!initial) {
      WriteBits(stream, 1, 1);
    }
    initial = false;
    EncodeType(stream, env, types->decl);
  }

  for (FuncEnv funcs = env->funcs; funcs != NULL; funcs = funcs->next) {
    assert(!initial);
    WriteBits(stream, 1, 1);
    EncodeFunc(stream, env, funcs->decl);
  }

  for (ProcEnv procs = env->procs; procs != NULL; procs = procs->next) {
    assert(!initial);
    WriteBits(stream, 1, 1);
    EncodeProc(stream, env, procs->decl);
  }
  WriteBits(stream, 1, 0);
}
