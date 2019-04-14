// fble.h --
//   This header file describes the externally visible interface to the
//   fble library.

#ifndef FBLE_H_
#define FBLE_H_

#include <stdbool.h>      // for bool
#include <sys/types.h>    // for size_t

#include "fble-alloc.h"
#include "fble-vector.h"
#include "fble-ref.h"
#include "fble-syntax.h"


// FbleParse --
//   Parse an expression from a file.
//
// Inputs:
//   arena - The arena to use for allocating the parsed program.
//   filename - The name of the file to parse the program from.
//   include_path - The directory to search for includes in. May be NULL.
//
// Results:
//   The parsed program, or NULL in case of error.
//
// Side effects:
//   Prints an error message to stderr if the program cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned program if there is no error.
//
// Note:
//   A copy of the filename will be made for use in locations. The user need
//   not ensure that filename remains valid for the duration of the lifetime
//   of the program.
FbleExpr* FbleParse(FbleArena* arena, const char* filename, const char* include_path);

typedef FbleRefArena FbleValueArena;

// FbleValueTag --
//   A tag used to distinguish among different kinds of values.
typedef enum {
  FBLE_STRUCT_VALUE,
  FBLE_UNION_VALUE,
  FBLE_FUNC_VALUE,
  FBLE_PROC_VALUE,
  FBLE_INPUT_VALUE,
  FBLE_OUTPUT_VALUE,
  FBLE_PORT_VALUE,
  FBLE_REF_VALUE,
  FBLE_TYPE_VALUE,
} FbleValueTag;

// FbleValue --
//   A tagged union of value types. All values have the same initial
//   layout as FbleValue. The tag can be used to determine what kind of
//   value this is to get access to additional fields of the value
//   by first casting to that specific type of value.
typedef struct FbleValue {
  FbleRef ref;
  FbleValueTag tag;
} FbleValue;

// FbleValueV --
//   A vector of FbleValue*
typedef struct {
  size_t size;
  FbleValue** xs;
} FbleValueV;

// FbleStructValue --
//   FBLE_STRUCT_VALUE
typedef struct {
  FbleValue _base;
  FbleValueV fields;
} FbleStructValue;

// FbleUnionValue --
//   FBLE_UNION_VALUE
typedef struct {
  FbleValue _base;
  size_t tag;
  FbleValue* arg;
} FbleUnionValue;

// FbleFuncValue --
//   FBLE_FUNC_VALUE
//
// Defined internally, because representing the body of the function depends
// on the internals of the evaluator.
typedef struct FbleFuncValue FbleFuncValue;

// FbleProcValue --
//   FBLE_PROC_VALUE
//
// Defined internally, because representing a process depends on the internals
// of the evaluator.
typedef struct FbleProcValue FbleProcValue;

// FbleInputValue --
//   FBLE_INPUT_VALUE
//
// Defined internally.
typedef struct FbleInputValue FbleInputValue;

// FbleOutputValue --
//   FBLE_OUTPUT_VALUE
//
// Defined internally.
typedef struct FbleOutputValue FbleOutputValue;

// FblePortValue --
//   FBLE_PORT_VALUE
//
// Use for input and output values linked to external IO.
//
// Defined internally.
typedef struct FblePortValue FblePortValue;

// FbleRefValue --
//   FBLE_REF_VALUE
//
// A implementation-specific value introduced to support recursive values. A
// ref value is simply a reference to another value. All values must be
// dereferenced before being otherwise accessed in case they are reference
// values.
//
// Fields:
//   value - the value being referenced, or NULL if no value is referenced.
typedef struct FbleRefValue {
  FbleValue _base;
  FbleValue* value;
} FbleRefValue;

// FbleTypeValue --
//   FBLE_TYPE_VALUE
//
// A value representing a type. Because types are compile-time concepts, not
// runtime concepts, the type value contains no information.
typedef struct FbleTypeValue {
  FbleValue _base;
} FbleTypeValue;

// FbleNewValueArena --
//   Create a new arena for allocation of values.
// 
// Inputs: 
//   arena - the arena to use for underlying allocations.
//
// Results:
//   A value arena that can be used to allocate values.
//
// Side effects:
//   Allocates a value arena that should be freed using FbleDeleteValueArena.
FbleValueArena* FbleNewValueArena(FbleArena* arena);

// FbleDeleteValueArena --
//   Reclaim resources associated with a value arena.
//
// Inputs:
//   arena - the arena to free.
//
// Results:
//   None.
//
// Side effects:
//   The resources associated with the given arena are freed. The arena should
//   not be used after this call.
void FbleDeleteValueArena(FbleValueArena* arena);

// FbleValueRetain --
//   Keep the given value alive until a corresponding FbleValueRelease is
//   called.
//
// Inputs:
//   arena - The arena used to allocate the value.
//   value - The value to retain. The value may be NULL, in which case nothing
//           is done.
//
// Results:
//   The given value, for the caller's convenience when retaining the
//   value and assigning it at the same time.
//
// Side effects:
//   Causes the value to be retained until a corresponding FbleValueRelease
//   calls is made on the value. FbleValueRelease must be called when the
//   value is no longer needed.
FbleValue* FbleValueRetain(FbleValueArena* arena, FbleValue* src);

// FbleValueRelease --
//
//   Decrement the strong reference count of a value and free the resources
//   associated with that value if it has no more references.
//
// Inputs:
//   arena - The value arena the value was allocated with.
//   value - The value to decrement the strong reference count of. The value
//           may be NULL, in which case no action is performed.
//
// Results:
//   None.
//
// Side effect:
//   Decrements the strong reference count of the value and frees resources
//   associated with the value if there are no more references to it.
void FbleValueRelease(FbleValueArena* arena, FbleValue* value);

// FbleNewStructValue --
//   Create a new struct value with given arguments.
//
// Inputs:
//   arena - The arena to use for allocations.
//   args - The arguments to the struct value.
//
// Results:
//   A newly allocated struct value with given args.
//
// Side effects:
//   The returned struct value must be freed using FbleValueRelease when no
//   longer in use. The args are freed using FbleValueRelease as part of
//   calling this function. The function does not take ownership of the args
//   array.
FbleValue* FbleNewStructValue(FbleValueArena* arena, FbleValueV args);

// FbleNewUnionValue --
//   Create a new union value with given tag and argument.
//
// Inputs:
//   arena - The arena to use for allocations.
//   tag - The tag of the union value.
//   arg - The argument of the union value.
//
// Results:
//   A newly allocated union value with given tag and arg.
//
// Side effects:
//   The returned union value must be freed using FbleValueRelease when no
//   longer in use. The arg is freed using FbleValueRelease as part of calling
//   this function.
FbleValue* FbleNewUnionValue(FbleValueArena* arena, size_t tag, FbleValue* arg);

// FbleNewPortValue --
//   Create a new io port value with given id.
//
// Inputs:
//   arena - the arena to use for allocations.
//   id - the id of the port for use with FbleIO.
//
// Results:
//   A newly allocated port value.
//
// Side effects:
//   The returned port value must be freed using FbleValueRelease when no
//   longer in use.
FbleValue* FbleNewPortValue(FbleValueArena* arena, size_t id);

// FbleEval --
//   Type check and evaluate an expression.
//
// Inputs:
//   arena - The arena to use for allocating values.
//   expr - The expression to evaluate.
//
// Results:
//   The value of the evaluated expression, or NULL in case of error. The
//   error could be a type error or an undefined union field access.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints an error message to stderr in case of error.
FbleValue* FbleEval(FbleValueArena* arena, FbleExpr* expr);

// FbleApply --
//   Apply a function to the given arguments.
//
// Inputs:
//   arena - the arena to use for allocating values.
//   func - the function to apply.
//   args - the arguments to apply the function to.
//
// Results:
//   The result of applying the function to the given arguments.
//
// Side effects:
//   The returned value must be freed with FbleValueRelease when no longer in
//   use. Prints an error message to stderr in case of error.
FbleValue* FbleApply(FbleValueArena* arena, FbleFuncValue* func, FbleValueV args);

// FbleIO --
//   An interface for reading or writing values over external ports.
typedef struct FbleIO {
  // io --
  //   Read or write values over the external ports. An FbleValue* is supplied
  //   for each port. The behavior of the function depends on the port type and
  //   whether the provided FbleValue* is NULL or non-NULL as follows:
  //
  //   Get Ports:
  //     NULL: the io function may, at its option, read the next input
  //       value and replace NULL with the newly read value.
  //     non-NULL: the io function should do nothing for this port.
  //
  //   Put Ports:
  //     NULL: the io function should do nothing for this port.
  //     non-NULL: the io function may, at its option, output the value.
  //       If the io function chooses to output the value, it should
  //       FbleValueRelease the value and replace it with NULL. Otherwise the
  //       io function should leave the existing value as is.
  //
  //   io may blocking or non-blocking depending on the 'block' input. For
  //   non-blocking io, the io function should return immediately without
  //   blocking for inputs to be ready. For blocking io, the io function
  //   should block until there is an input available on one of the NULL get
  //   ports.
  //
  //   The function should return true if any of the ports have been read or
  //   written, false otherwise.
  //
  // Inputs:
  //   io - The FbleIO associated with this io.
  //   arena - The arena to use for allocating and freeing values.
  //   block - true if io should be blocking, false if it should be
  //           non-blocking.
  //
  // Result:
  //   true if one or more ports have been read or written, false otherwise.
  //
  // Side effects:
  //   Reads or writes values to external ports depending on the provided
  //   arguments and may block.
  //   
  bool (*io)(struct FbleIO* io, FbleValueArena* arena, bool block);

  // Vector of port values to read from and write to. The caller must
  // initialize and clear this vector.
  FbleValueV ports;
} FbleIO;

// FbleExec --
//   Execute a process.
//
// Inputs:
//   arena - The arena to use for allocating values.
//   io - The io to use for external ports.
//   proc - The process to execute.
//
// Results:
//   The result of executing the process, or NULL in case of error. The
//   error could be an undefined union field access.
//
// Side effects:
//   Prints an error message to stderr in case of error.
FbleValue* FbleExec(FbleValueArena* arena, FbleIO* io, FbleProcValue* proc);

#endif // FBLE_H_
