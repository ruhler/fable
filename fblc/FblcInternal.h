// FblcInternal.h --
//
//   This header file describes the internally-visible facilities of the Fblc
//   interpreter.

#ifndef FBLC_INTERNAL_H_
#define FBLC_INTERNAL_H_

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define GC_DEBUG
#include <gc/gc.h>
#define MALLOC_INIT() GC_INIT()
#define MALLOC(x) GC_MALLOC(x)
#define FREE(x) GC_FREE(x)
#define ENABLE_LEAK_DETECTION() GC_find_leak = 1
#define CHECK_FOR_LEAKS() GC_gcollect()

typedef struct FblcAllocList FblcAllocList;

// FblcAllocator
typedef struct {
  FblcAllocList* allocations;
} FblcAllocator;

// FblcVector is a helper for dynamically allocating an array of data whose
// size is not known ahead of time.
// 'size' is the number of bytes taken by a single element.
// 'capacity' is the maximum number of elements supported by the current
// allocation of data.
// 'count' is the number of elements currently in use.
// 'data' is where the data is stored.

typedef struct {
  FblcAllocator* allocator;
  int size;
  int capacity;
  int count;
  void* data;
} FblcVector ;

void FblcInitAllocator(FblcAllocator* alloc);
void* FblcAlloc(FblcAllocator* alloc, int size);
void FblcFreeAll(FblcAllocator* alloc);
void FblcVectorInit(FblcAllocator* alloc, FblcVector* vector, int size);
void* FblcVectorAppend(FblcVector* vector);
void* FblcVectorExtract(FblcVector* vector, int* count);

// FblcProgram
typedef const char* FblcName;
bool FblcNamesEqual(FblcName a, FblcName b);

typedef struct {
  const char* source;
  int line;
  int col;
} FblcLoc;

void FblcReportError(const char* format, const FblcLoc* loc, ...);

typedef struct FblcLocName {
  FblcLoc* loc;
  FblcName name;
} FblcLocName;

typedef enum {
  FBLC_VAR_EXPR,
  FBLC_APP_EXPR,
  FBLC_ACCESS_EXPR,
  FBLC_UNION_EXPR,
  FBLC_LET_EXPR,
  FBLC_COND_EXPR,
} FblcExprTag;

// FblcExpr is the base structure for all Fblc expressions. Each
// specialization of FblcExpr has the same initial layout with tag and
// location.

typedef struct {
  FblcExprTag tag;
  FblcLoc* loc;
} FblcExpr;

// FBLC_VAR_EXPR: Variable expressions of the form: <name>
typedef struct {
  FblcExprTag tag;
  FblcLoc* loc;
  FblcLocName name;
} FblcVarExpr;

// FBLC_APP_EXPR: Application expressions of the form: <func>(<argv>)
typedef struct {
  FblcExprTag tag;
  FblcLoc* loc;
  FblcLocName func;
  int argc;
  FblcExpr** argv;
} FblcAppExpr;

// FBLC_ACCESS_EXPR: Member access expressions of the form: <object>.<field>
typedef struct {
  FblcExprTag tag;
  FblcLoc* loc;
  const FblcExpr* object;
  FblcLocName field;
} FblcAccessExpr;

// FBLC_UNION_EXPR: Union literals of the form: <type>:<field>(<value>)
typedef struct {
  FblcExprTag tag;
  FblcLoc* loc;
  FblcLocName type;
  FblcLocName field;
  const FblcExpr* value;
} FblcUnionExpr;

// FBLC_LET_EXPR: let expressions of the form: <type> <name> = <def> ; <body>
typedef struct {
  FblcExprTag tag;
  FblcLoc* loc;
  FblcLocName type;
  FblcLocName name;
  const FblcExpr* def;
  const FblcExpr* body;
} FblcLetExpr;

// FBLC_COND_EXPR: Conditional expressions of the form: <select>?(<argv>)
typedef struct {
  FblcExprTag tag;
  FblcLoc* loc;
  const FblcExpr* select;
  int argc;
  FblcExpr** argv;
} FblcCondExpr;

typedef enum { FBLC_KIND_UNION, FBLC_KIND_STRUCT } FblcKind;

typedef struct {
  FblcLocName type;
  FblcLocName name;
} FblcField;

typedef struct {
  FblcLocName name;
  FblcKind kind;
  int fieldc;
  FblcField* fieldv;
} FblcType;

typedef struct {
  FblcLocName name;
  FblcLocName return_type;
  FblcExpr* body;
  int argc;
  FblcField* argv;
} FblcFunc;

typedef enum { FBLC_POLARITY_PUT, FBLC_POLARITY_GET } FblcPolarity;

typedef struct {
  FblcLocName type;
  FblcLocName name;
  FblcPolarity polarity;
} FblcPort;

typedef enum {
  FBLC_EVAL_ACTN,
  FBLC_GET_ACTN,
  FBLC_PUT_ACTN,
  FBLC_CALL_ACTN,
  FBLC_LINK_ACTN,
  FBLC_EXEC_ACTN,
  FBLC_COND_ACTN,
} FblcActnTag;

// FblcActn is the base structure for all Fblc actions. Each specialization of
// FblcActn will have the same initial layout with tag and location.

typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
} FblcActn;

// FBLC_EVAL_ACTN: Processes of the form: $(<expr>)
typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
  const FblcExpr* expr;
} FblcEvalActn;

// FBLC_GET_ACTN: Processes of the form: <pname>~()
typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
  FblcLocName port;
} FblcGetActn;

// FBLC_PUT_ACTN: Processes of the form: <pname>~(<expr>)
typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
  FblcLocName port;
  FblcExpr* expr;
} FblcPutActn;

// FBLC_CALL_ACTN: Processes of the form: <tname>(<port>, ... ; <expr>, ...)
typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
  FblcLocName proc;
  int portc;
  FblcLocName* ports;   // Array of portc ports
  int exprc;
  FblcExpr** exprs;      // Array of exprc exprs
} FblcCallActn;

// FBLC_LINK_ACTN: Processes of the form:
//    <tname> '<~>' <pname> ',' <pname> ';' <actn>
typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
  FblcLocName type;
  FblcLocName getname;
  FblcLocName putname;
  FblcActn* body;
} FblcLinkActn;

typedef struct {
  FblcField var;
  FblcActn* actn;
} FblcExec;

// FBLC_EXEC_ACTN: Processes of the form:
//    <tname> <vname> = <actn>,  ...  ; <body>
typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
  int execc;
  FblcExec* execv;          // Array of execc execs.
  FblcActn* body;
} FblcExecActn;

// FBLC_COND_ACTN: Processes of the form: <expr>?(<proc>, ...)
typedef struct {
  FblcActnTag tag;
  FblcLoc* loc;
  FblcExpr* select;
  int argc;
  FblcActn** args;    //  Array of argc args.
} FblcCondActn;

typedef struct {
  FblcLocName name;
  FblcLocName return_type;
  FblcActn* body;
  int portc;
  FblcPort* portv;              // Array of portc ports.
  int argc;
  FblcField* argv;              // Array of argv fields.
} FblcProc;

// An environment contains all the type, function, and process declarations
// for a program. All names used for types, functions, and processes must be
// unique. This is enforced during the construction of the environment.
typedef struct FblcTypeEnv {
  FblcType* decl;
  struct FblcTypeEnv* next;
} FblcTypeEnv;

typedef struct FblcFuncEnv {
  FblcFunc* decl;
  struct FblcFuncEnv* next;
} FblcFuncEnv;

typedef struct FblcProcEnv {
  FblcProc* decl;
  struct FblcProcEnv* next;
} FblcProcEnv;

typedef struct FblcEnv {
  FblcTypeEnv* types;
  FblcFuncEnv* funcs;
  FblcProcEnv* procs;
} FblcEnv;

FblcEnv* FblcNewEnv(FblcAllocator* alloc);
FblcType* FblcLookupType(const FblcEnv* env, FblcName name);
FblcFunc* FblcLookupFunc(const FblcEnv* env, FblcName name);
FblcProc* FblcLookupProc(const FblcEnv* env, FblcName name);
bool FblcAddType(FblcAllocator* alloc, FblcEnv* env, FblcType* type);
bool FblcAddFunc(FblcAllocator* alloc, FblcEnv* env, FblcFunc* func);
bool FblcAddProc(FblcAllocator* alloc, FblcEnv* env, FblcProc* proc);

// FblcValue

// FblcValue is the base structure for FblcStructValue and FblcUnionValue,
// both of which have the same initial layout.

typedef struct {
  int refcount;
  FblcType* type;
} FblcValue;

// For struct values 'fieldv' contains the field data in the order the fields
// are declared in the type declaration.

typedef struct {
  int refcount;
  FblcType* type;
  FblcValue** fieldv;
} FblcStructValue;

// For union values, 'tag' is the index of the active field and 'field'
// is stores the value of the active field.

typedef struct {
  int refcount;
  FblcType* type;
  int tag;
  FblcValue* field;
} FblcUnionValue;

FblcStructValue* FblcNewStructValue(FblcType* type);
FblcUnionValue* FblcNewUnionValue(FblcType* type);
FblcValue* FblcCopy(FblcValue* src);
void FblcRelease(FblcValue* value);
void FblcPrintValue(FILE* fout, FblcValue* value);
int FblcTagForField(const FblcType* type, FblcName field);

// FblcTokenizer
// A stream of tokens is represented using the FblcTokenStream data structure.
// Tokens can be read either from a file or a string.
//
// The conventional variable name for a FblcTokenStream* is 'toks'.

typedef struct {
  // When reading from a file, fd is the file descriptor for the underlying
  // file and buffer contains the most recently read data from the file. When
  // reading from a string, fd is -1 and buffer is unused.
  int fd;
  char buffer[BUFSIZ];

  // curr points to the current character in the stream, if any.
  // end points just past the last character in either the buffer or the
  // string. If the end of the string has been reached or the no more file
  // data is available in the buffer, curr and end are equal.
  const char* curr;
  const char* end;

  // Location information for the next token for the purposes of error
  // reporting.
  FblcLoc loc;
} FblcTokenStream;

bool FblcOpenFileTokenStream(FblcTokenStream* toks, const char* filename);
bool FblcOpenStringTokenStream(FblcTokenStream* toks, const char* source,
    const char* string);
void FblcCloseTokenStream(FblcTokenStream* toks);
bool FblcIsEOFToken(FblcTokenStream* toks);
bool FblcIsToken(FblcTokenStream* toks, char which);
bool FblcGetToken(FblcTokenStream* toks, char which);
bool FblcIsNameToken(FblcTokenStream* toks);
bool FblcGetNameToken(FblcAllocator* alloc,
    FblcTokenStream* toks, const char* expected, FblcLocName* name);
void FblcUnexpectedToken(FblcTokenStream* toks, const char* expected);

// FblcParser
FblcEnv* FblcParseProgram(FblcAllocator* alloc, FblcTokenStream* toks);
FblcValue* FblcParseValue(FblcEnv* env, FblcType* type, FblcTokenStream* toks);

// FblcChecker
bool FblcCheckProgram(const FblcEnv* env);

// FblcEvaluator

// FblcIO represents an abstract function for performing port IO.
//
// For ports with FBLC_POLARITY_GET, the 'io' function is passed its user data
// with NULL as the value. The 'io' function should return the next value to
// get, or NULL to indicate no value is currently available to be gotten.
//
// For ports with FBLC_POLARITY_PUT, the 'io' function is passed its user data
// and the value to put. The 'io' function must return NULL.

typedef struct {
  FblcValue* (*io)(void* user, FblcValue* value);
  void* user;
} FblcIO;

FblcValue* FblcExecute(const FblcEnv* env, FblcProc* proc,
    FblcIO* portios, FblcValue** args);

#endif  // FBLC_INTERNAL_H_
