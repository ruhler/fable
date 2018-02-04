// fbld.h --
//   This header file describes a representation and utilies for working with
//   fbld programs.

#ifndef FBLD_H_
#define FBLD_H_

#include <stdio.h>  // for FILE

#include "fblc.h"   // for FblcArena

// FbldLoc --
//   Represents a location in a source file.
//
// Fields:
//   source - The name of the source file or other description of the source
//            of the program text.
//   line - The line within the file for the location.
//   col - The column within the line for the location.
typedef struct {
  const char* source;
  int line;
  int col;
} FbldLoc;

// FbldReportError --
//   Report an error message associated with a location in a source file.
//
// Inputs:
//   format - A printf format string for the error message.
//   loc - The location of the error message to report.
//   ... - printf arguments as specified by the format string.
//
// Results:
//   None.
//
// Side effects:
//   Prints an error message to stderr with error location.
void FbldReportError(const char* format, FbldLoc* loc, ...);

// Forward declarations. See below for descriptions of these types.
typedef struct FbldInterf FbldInterf;
typedef struct FbldDecl FbldDecl;
typedef struct FbldDeclV FbldDeclV;
typedef struct FbldQRef FbldQRef;
typedef struct FbldQRefV FbldQRefV;

// FbldName -- 
//   A name along with its associated location in a source file. The location
//   is typically used for error reporting purposes.
typedef struct {
  const char* name;
  FbldLoc* loc;
} FbldName;

// FbldNameV --
//   A vector of fbld names.
typedef struct {
  size_t size;
  FbldName** xs;
} FbldNameV;

// FbldId --
//   A reference to a variable, field, or port.
//
// Fields:
//   name - The name of the field.
//   id - The fblc id of the field. This is set to FBLC_NULL_ID by the parser,
//        then later filled in during type check and used by the compiler.
typedef struct {
  FbldName* name;
  size_t id;
} FbldId;

// FbldIdV -- A vector of fbld ids.
typedef struct {
  size_t size;
  FbldId* xs;
} FbldIdV;

// FbldR --
//   Resolved qref information.
// 
// Fields:
//   decl - The declaration that the qref resolved to.
//          This may be a declaration of an entity from a module, a prototype
//          in an interface, the declaration of a static parameter, or NULL to
//          indicate failed resolution.
//   mref - The module/interf namespace that the entity belongs to. NULL for
//          entities defined at the top level.
//   param - True if the qref refers to a static parameter or a prototype from
//           an interface that has not had its module resolved.
//   interf - The interface that a prototype was defined in. NULL if decl
//            is not a prototype from an interface.
//
// States:
//   * Failed resolution:
//      decl == NULL, all others unused.
//   * Entity from module:
//      decl != NULL,
//      mref != NULL except top level decls,
//      !param, interf == NULL
//   * Static parameter:
//      decl != NULL,
//      mref != NULL except top level decls
//      param, interf == NULL
//   * Interface prototype with module not yet resolved:
//      decl != NULL,
//      mref != NULL except top level decls
//      param, interf != NULL.
//   * Interface prototype with module resolved:
//      decl != NULL,
//      mref != NULL,
//      !param, interf != NULL.
typedef struct {
  FbldDecl* decl;
  FbldQRef* mref;
  bool param;
  FbldInterf* interf;
} FbldR;

// FbldQRef --
//   A reference to a qualified interface, module, type, function or process,
//   such as:
//    find<type Nat, module eqNat(Eq<Nat>)>@ListM
//
// FbldQRef stores both unresolved and resolved versions of the qualified
// reference. The unresolved version is as the reference appears in the source
// code, the resolved version is the canonical global path to the relevant
// entity or type parameter. The parser leaves the qref unresolved. The type
// checker resolves all entities, updating r as appropriate.
//
// Fields:
//   name - The unresolved name of the entity.
//   paramv - The static paramters to the entity.
//   mref - The unresolved module the entity is from. NULL for references that
//          are not explicitly qualified.
//   r - The resolved reference, or NULL if the reference has not yet been
//       resolved.
struct FbldQRef {
  FbldName* name;
  FbldQRefV* paramv;
  FbldQRef* mref;
  FbldR* r;
};

// FbldQRefV --
//   A vector of qrefs.
struct FbldQRefV {
  size_t size;
  FbldQRef** xs;
}; 

// FbldArg --
//   An fbld name with associated (possibly qualified) type. Used for
//   declaring fields of structures and arguments to functions.
typedef struct {
  FbldQRef* type;
  FbldName* name;
} FbldArg;

// FbldArgV --
//   A vector of fbld args.
typedef struct {
  size_t size;
  FbldArg** xs;
} FbldArgV;

// FbldNamesEqual --
//   Test whether two names are equal.
//
// Inputs:
//   a - The first name.
//   b - The second name.
//
// Results:
//   true if the first name equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbldNamesEqual(const char* a, const char* b);

// FbldQRefsEqual --
//   Test whether two qrefs are structurally equal.
//
// Inputs:
//   a - The first qref.
//   b - The second qref.
//
// Results:
//   true if the first qref structurally equals the second, false otherwise.
//
// Side effects:
//   None.
bool FbldQRefsEqual(FbldQRef* a, FbldQRef* b);

// FbldImportQRef --
//   Reference a foreign type or module. A foreign qref is a type or module
//   referred to from a type, func, or proc declaration. The foreign qref
//   will have been resolved in the context of the possibly parameterized
//   type, func, or proc declaration. It is now being used with concrete
//   parameter arguments. This function substitutes the proper concrete
//   concrete arguments given by the src of the foreign qref.
//
//   For example, if the source of the foreign type is Maybe<Int>, and the
//   foreign type is the parameter T in the context of the definition of
//   Maybe<T>, then this returns Int.
//
//   Substitutions are performed for type arguments, module arguments, and
//   interfaces.
//
// Inputs:
//   arena - An arena to use for allocations.
//   src - The src reference in the current environment used to supply
//         parameter arguments.
//   qref - The foreign qref that has been resolved in the context of its
//          definition.
//
// Results:
//   The foreign type imported into the current environment by substituting
//   parameter arguments as appropriate given the src.
//
// Side effects:
//   May allocate a new qref used for the return value.
FbldQRef* FbldImportQRef(FblcArena* arena, FbldQRef* src, FbldQRef* qref);

// FbldPrintQRef --
//   Print a resolved qref in human readable format to the given stream.
//
// Inputs:
//   stream - Stream to print the type to.
//   qref - The qref to print.
//
// Results:
//   None.
//
// Side effects:
//   Prints the qref to the stream.
void FbldPrintQRef(FILE* stream, FbldQRef* qref);

// FbldExprTag --
//   A tag used to distinguish among different kinds of expressions.
typedef enum {
  FBLD_VAR_EXPR,
  FBLD_APP_EXPR,
  FBLD_UNION_EXPR,
  FBLD_ACCESS_EXPR,
  FBLD_COND_EXPR,
  FBLD_LET_EXPR
} FbldExprTag;

// FbldExpr --
//   Common base type for the following fbld expr types. The tag can be used
//   to determine what kind of expr this is to get access to additional fields
//   of the expr by first casting to that specific type.
typedef struct {
  FbldExprTag tag;
  FbldLoc* loc;
} FbldExpr;

// FbldExprV --
//   A vector of fbld expressions.
typedef struct {
  size_t size;
  FbldExpr** xs;
} FbldExprV;

// FbldVarExpr --
//   A variable expression of the form 'var' whose value is the value of the
//   corresponding variable in scope.
//
// Fields:
//   var - The name of the variable.
typedef struct {
  FbldExpr _base;
  FbldId var;
} FbldVarExpr;

// FbldAppExpr --
//   An application expression of the form 'func(arg0, arg1, ...)'. func may
//   refer to a function or a struct type.
typedef struct {
  FbldExpr _base;
  FbldQRef* func;
  FbldExprV* argv;
} FbldAppExpr;

// FbldUnionExpr --
//   A union expression of the form 'type:field(arg)', used to construct a
//   union value.
typedef struct {
  FbldExpr _base;
  FbldQRef* type;
  FbldId field;
  FbldExpr* arg;
} FbldUnionExpr;

// FbldAccessExpr --
//   An access expression of the form 'obj.field' used to access a field of
//   a struct or union value.
typedef struct {
  FbldExpr _base;
  FbldExpr* obj;
  FbldId field;
} FbldAccessExpr;

// FbldCondExpr --
//   A conditional expression of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FbldExpr _base;
  FbldExpr* select;
  FbldExprV* argv;
} FbldCondExpr;

// FbldLetExpr --
//   A let expression of the form '{ type var = def; body }'.
typedef struct {
  FbldExpr _base;
  FbldQRef* type;
  FbldName* var;
  FbldExpr* def;
  FbldExpr* body;
} FbldLetExpr;

// FbldActnTag --
//   A tag used to distinguish among different kinds of actions.
typedef enum {
  FBLD_EVAL_ACTN,
  FBLD_GET_ACTN,
  FBLD_PUT_ACTN,
  FBLD_COND_ACTN,
  FBLD_CALL_ACTN,
  FBLD_LINK_ACTN,
  FBLD_EXEC_ACTN
} FbldActnTag;

// FbldActn --
//   A tagged union of action types. All actions have the same initial
//   layout as FbldActn. The tag can be used to determine what kind of
//   action this is to get access to additional fields of the action
//   by first casting to that specific type of action.
typedef struct {
  FbldActnTag tag;
  FbldLoc* loc;
} FbldActn;

// FbldActnV --
//   A vector of fblcs actions.
typedef struct {
  size_t size;
  FbldActn** xs;
} FbldActnV;

// FbldEvalActn --
//   An evaluation action of the form '$(arg)' which evaluates the given
//   expression without side effects.
typedef struct {
  FbldActn _base;
  FbldExpr* arg;
} FbldEvalActn;

// FbldGetActn --
//   A get action of the form '~port()' used to get a value from a port.
typedef struct {
  FbldActn _base;
  FbldId port;
} FbldGetActn;

// FbldPutActn --
//   A put action of the form '~port(arg)' used to put a value onto a port.
typedef struct {
  FbldActn _base;
  FbldId port;
  FbldExpr* arg;
} FbldPutActn;

// FbldCondActn --
//   A conditional action of the form '?(select; arg0, arg1, ...)', which
//   conditionally selects an argument based on the tag of the select value.
typedef struct {
  FbldActn _base;
  FbldExpr* select;
  FbldActnV* argv;
} FbldCondActn;

// FbldCallActn --
//   A call action of the form 'proc(port0, port1, ... ; arg0, arg1, ...)',
//   which calls a process with the given port and value arguments.
typedef struct {
  FbldActn _base;
  FbldQRef* proc;
  FbldIdV* portv;
  FbldExprV* argv;
} FbldCallActn;

// FbldLinkActn --
//   A link action of the form 'type <~> get, put; body'. The names of the get
//   and put ports are De Bruijn indices based on the context where the ports
//   are accessed.
typedef struct {
  FbldActn _base;
  FbldQRef* type;
  FbldName* get;
  FbldName* put;
  FbldActn* body;
} FbldLinkActn;

// FbldExec --
//   Trip of type, name, and action used in the FbldExecActn.
typedef struct {
  FbldQRef* type;
  FbldName* name;
  FbldActn* actn;
} FbldExec;

// FbldExecV --
//   Vector of FbldExec.
typedef struct {
  size_t size;
  FbldExec* xs;
} FbldExecV;

// FbldExecActn --
//   An exec action of the form 'type0 var0 = exec0, type1 var1 = exec1, ...; body',
//   which executes processes in parallel.
typedef struct {
  FbldActn _base;
  FbldExecV* execv;
  FbldActn* body;
} FbldExecActn;

// FbldImportItem --
//   Specification for a single item in an import declaration of the form
//   <dest>[=<source>].
//
// Fields:
//   source - The name of the entity in the original module.
//   dest - The name under which the entity is referred to in the importing
//          module.
//
// It is not uncommon to have the source and destination names be the same, in
// which case the source and dest fields should be identical, rather than
// setting one or the other to NULL.
typedef struct {
  FbldQRef* source;
  FbldName* dest;
} FbldImportItem;

// FbldImportItemV --
//   A vector of fbld import items.
typedef struct {
  size_t size;
  FbldImportItem** xs;
} FbldImportItemV;

// FbldImport --
//   An import declaration of the form:
//    using <mref> {<name>[=<name>]; <name>[=<name>]; ...; }
//
//   mref is set to NULL to indicate we are importing from the parent
//   namespace '@'.
typedef struct {
  FbldQRef* mref;
  FbldImportItemV* itemv;
} FbldImport;

// FbldImportV --
//   A vector of FbldImports.
typedef struct {
  size_t size;
  FbldImport** xs;
} FbldImportV;

// FbldMArg --
//   A module argument, such as: Map<Int, String> M
//
// Fields:
//   iref - The interface of the module argument.
//   name - The name of the module argument.
// TODO: Remove this type?
typedef struct {
  FbldQRef* iref;
  FbldName* name;
} FbldMArg;

// FbldMArgV --
//   A vector of module arguments.
// TODO: Remove this type?
typedef struct {
  size_t size;
  FbldMArg** xs;
} FbldMArgV;

// FbldDeclTag --
//   A tag used to distinguish among different kinds of declarations.
typedef enum {
  FBLD_TYPE_DECL,
  FBLD_FUNC_DECL,
  FBLD_PROC_DECL,
  FBLD_INTERF_DECL,
  FBLD_MODULE_DECL
} FbldDeclTag;

// FbldDecl --
//   Common base type for the following fbld decl types. The tag can be
//   used to determine what kind of decl this is to get access to additional
//   fields of the decl by first casting to that specific type.
//
// Fields:
//   tag - the kind of declaration.
//   name - the name of the declaration.
//   paramv - the static parameters of the declaration.
struct FbldDecl {
  FbldDeclTag tag;
  FbldName* name;
  FbldDeclV* paramv;
};

// FbldDeclV --
//   A vector of FbldDecls.
struct FbldDeclV {
  size_t size;
  FbldDecl** xs;
};

// FbldKind --
//   An enum used to distinguish between struct, union, and abstract types.
typedef enum {
  FBLD_STRUCT_KIND,
  FBLD_UNION_KIND,
  FBLD_ABSTRACT_KIND
} FbldKind;

// FbldType --
//   An fbld type declaration.
//   fieldv is NULL for abstract type declarations.
typedef struct {
  FbldDecl _base;
  FbldKind kind;
  FbldArgV* fieldv;
} FbldType;

// FbldFunc--
//   A declaration of a function.
//   The body is NULL for function prototypes specified in module
//   declarations.
typedef struct {
  FbldDecl _base;
  FbldArgV* argv;
  FbldQRef* return_type;
  FbldExpr* body;
} FbldFunc;

// FbldPolarity --
//   The polarity of a port.
typedef enum {
  FBLD_GET_POLARITY,
  FBLD_PUT_POLARITY
} FbldPolarity;

// FbldPort --
//   The type, name, and polarity of a port.
typedef struct {
  FbldQRef* type;
  FbldName* name;
  FbldPolarity polarity;
} FbldPort;

// FbldPortV --
//   A vector of fbld ports.
typedef struct {
  size_t size;
  FbldPort* xs;
} FbldPortV;

// FbldProc --
//   A declaration of a process.
//   The body is NULL for process prototypes specified in module
//   declarations.
typedef struct {
  FbldDecl _base;
  FbldPortV* portv;
  FbldArgV* argv;
  FbldQRef* return_type;
  FbldActn* body;
} FbldProc;

// FbldProgram --
typedef struct {
  FbldImportV* importv;
  FbldDeclV* declv;
} FbldProgram;

// FbldInterf --
//   An fbld interface declaration.
struct FbldInterf {
  FbldDecl _base;
  FbldProgram* body;
};

// FbldModule --
//   An fbld module declaration.
//
// Fields:
//   name - The name of the module being defined.
//   params - The polymorphic type and module parameters of the module.
//   iref - The interface the module implements.
//   importv - The import statements within the module definition.
//   declv - The declarations within the module definition.
typedef struct {
  FbldDecl _base;
  FbldQRef* iref;
  FbldProgram* body;
} FbldModule;

// Forward declaration of FbldValue type.
typedef struct FbldValue FbldValue;

// FbldValueV -- 
//   A vector of fbld values.
typedef struct {
  size_t size;
  FbldValue** xs;
} FbldValueV;

// FbldValue --
//   fbld representation of an value object.
struct FbldValue {
  FbldKind kind;
  FbldQRef* type;
  FbldName* tag;
  FbldValueV* fieldv;
};

// FbldParseProgram --
//   Parse a program the file with the given filename.
//
// Inputs:
//   arena - The arena to use for allocating the parsed program.
//   filename - The name of the file to parse the program from.
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
FbldProgram* FbldParseProgram(FblcArena* arena, const char* filename);

// FbldParseValueFromString --
//   Parse an fbld value from the given string.
//
// Inputs:
//   arena - The arena to use for allocating the parsed definition.
//   string - The string to parse the value from.
//
// Results:
//   The parsed value, or NULL if the value could not be parsed.
//
// Side effects:
//   Prints an error message to stderr if the value cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the returned value if there is no error.
FbldValue* FbldParseValueFromString(FblcArena* arena, const char* string);

// FbldParseQRefFromString --
//   Parse an FbldQRef from the given string.
//
// Inputs:
//   arena - The arena to use for allocating the parsed definition.
//   string - The string to parse the value from.
//
// Results:
//   The parsed qref, or NULL if the qref could not be parsed.
//
// Side effects:
//   Prints an error message to stderr if the qref cannot be parsed.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function.
FbldQRef* FbldParseQRefFromString(FblcArena* arena, const char* string);

// FbldCheckProgram --
//   Check that the given top-level program environment describes a well
//   formed and well typed program.
//
// Inputs:
//   arena - Arena used for allocations.
//   prgm - The program environment to check.
//
// Results:
//   true if the program environment is well formed and well typed,
//   false otherwise.
//
// Side effects:
//   If the program environment is not well formed, an error message is
//   printed to stderr describing the problem with the program environment.
bool FbldCheckProgram(FblcArena* arena, FbldProgram* prgm);

// FbldCheckQRef --
//   Check that the given qualified reference is well formed and well typed in
//   the global context.
//
// Inputs:
//   arena - Arena used for allocations.
//   prgm - The program.
//   qref - The qualified reference to check.
//
// Results:
//   true if the qualified reference is well formed and well typed, false
//   otherwise.
//
// Side effects:
//   If this qualified reference or any of the required declarations are not
//   well formed, error messages are printed to stderr describing the
//   problems.
bool FbldCheckQRef(FblcArena* arena, FbldProgram* prgm, FbldQRef* qref);

// FbldCheckValue --
//   Check that the value is well formed in a global context.
//
// Inputs:
//   arena - Arena to use for any necessary internal allocations.
//   prgm - The program environment.
//   value - The value to check.
//
// Results:
//   true if the value is well formed, false otherwise.
//
// Side effects:
//   Resolves references in the value.
//   Prints a message to stderr if the value is not well formed.
bool FbldCheckValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value);

// FbldAccessLoc --
//   The location of an access expression, for aid in debugging undefined
//   member access.
//
// Fields:
//   expr - An access expression.
//   loc - The source location of the access expression.
typedef struct {
  FblcExpr* expr;
  FbldLoc* loc;
} FbldAccessLoc;

// FbldAccessLocV --
//   A vector of access locs.
typedef struct {
  size_t size;
  FbldAccessLoc* xs;
} FbldAccessLocV;

// FbldLoaded --
//   The fbld program, fbld proc, and fblc proc corresponding to a compiled
//   entry point of a loaded program.
typedef struct {
  FbldProgram* prog;
  FbldProc* proc_d;
  FblcProc* proc_c;
} FbldLoaded;

// FbldCompileProgram --
//   Compile an fbld program from an already checked fbld program.
//
// Inputs:
//   arena - arena to use for allocating the fblc program.
//   accessv - collection of access expression locations.
//   prgm - The fbld program environment.
//   entity - The name of the main entry to compile the program for.
//
// Result:
//   The loaded program and entry points, or NULL if the entry could not be
//   found.
//
// Side effects:
//   Updates accessv with the location of compiled access expressions.
//   The behavior is undefined if the program environment is not well formed.
FbldLoaded* FbldCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, FbldProgram* prgm, FbldQRef* entity);

// FbldCompileValue --
//   Compile an fbld value to an fblc value.
//
// Inputs:
//   arena - Arena to use for allocating the fblc program.
//   modulev - Modules describing a valid fbld program.
//   value - The fbld value to compile.
//
// Result:
//   An fblc value equivalent to the given fbld value.
//
// Side effects:
//   The behavior is undefined if the fbld program is not a valid fbld
//   program or the fbld value is not well typed.
FblcValue* FbldCompileValue(FblcArena* arena, FbldProgram* prgm, FbldValue* value);

// FbldLoadProgram --
//   Load the top level program from the given file using the given arena for
//   allocations. Performs parsing and type check of the program.
//
// Inputs:
//   arena - Arena to use for allocations.
//   path - The name of the file to load the program from.
//
// Results:
//   The loaded program or NULL on error.
//
// Side effects:
//   Arena allocations are made in order to load the program. If the program
//   cannot be loaded, an error message is printed to stderr.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the program if there is no error.
FbldProgram* FbldLoadProgram(FblcArena* arena, const char* path);

// FbldLoadCompileProgram --
//   Load the top level program from the given file using the given arena for
//   allocations. Performs parsing, type check, and compilation of the given
//   entry point of the program.
//
// Inputs:
//   arena - Arena to use for allocations.
//   accessv - collection of access expression locations.
//   path - The name of the file to load the program from.
//   entry - The main entry point.
//
// Results:
//   The loaded program and entry points or NULL on error.
//
// Side effects:
//   Arena allocations are made in order to load the program. If the program
//   cannot be loaded, an error message is printed to stderr.
//
// Allocations:
//   The user is responsible for tracking and freeing any allocations made by
//   this function. The total number of allocations made will be linear in the
//   size of the program if there is no error.
FbldLoaded* FbldLoadCompileProgram(FblcArena* arena, FbldAccessLocV* accessv, const char* path, FbldQRef* entry);
#endif // FBLD_H_
