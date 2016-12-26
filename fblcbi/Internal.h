// Internal.h --
//
//   This header file describes the internally-visible facilities of the 
//   interpreter.

#ifndef INTERNAL_H_
#define INTERNAL_H_

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

typedef struct AllocList AllocList;

// Allocator
typedef struct {
  AllocList* allocations;
} Allocator;

// Vector is a helper for dynamically allocating an array of data whose
// size is not known ahead of time.
// 'size' is the number of bytes taken by a single element.
// 'capacity' is the maximum number of elements supported by the current
// allocation of data.
// 'count' is the number of elements currently in use.
// 'data' is where the data is stored.

typedef struct {
  Allocator* allocator;
  int size;
  int capacity;
  int count;
  void* data;
} Vector ;

void InitAllocator(Allocator* alloc);
void* Alloc(Allocator* alloc, int size);
void FreeAll(Allocator* alloc);
void VectorInit(Allocator* alloc, Vector* vector, int size);
void* VectorAppend(Vector* vector);
void* VectorExtract(Vector* vector, int* count);

// Program
typedef const char* Name;
bool NamesEqual(Name a, Name b);

typedef struct {
  const char* source;
  int line;
  int col;
} Loc;

void ReportError(const char* format, Loc* loc, ...);

// LocName stores a name along with a location for error reporting purposes.
// The id field contains the id of name as used in the binary encoded program.
// Initial id will be UNRESOLVED_ID. Relevant ids are resolved during the type
// checking phase.
#define UNRESOLVED_ID (-1)
typedef struct LocName {
  Loc* loc;
  Name name;
  size_t id;
} LocName;

typedef enum {
  VAR_EXPR,
  APP_EXPR,
  UNION_EXPR,
  ACCESS_EXPR,
  COND_EXPR,
  LET_EXPR,
} ExprTag;

// Expr is the base structure for all  expressions. Each
// specialization of Expr has the same initial layout with tag and
// location.

typedef struct {
  ExprTag tag;
  Loc* loc;
} Expr;

// VAR_EXPR: Variable expressions of the form: <name>
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName name;
} VarExpr;

// APP_EXPR: Application expressions of the form: <func>(<argv>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName func;
  int argc;
  Expr** argv;
} AppExpr;

// ACCESS_EXPR: Member access expressions of the form: <object>.<field>
typedef struct {
  ExprTag tag;
  Loc* loc;
  Expr* object;
  LocName field;
} AccessExpr;

// UNION_EXPR: Union literals of the form: <type>:<field>(<value>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName type;
  LocName field;
  Expr* value;
} UnionExpr;

// LET_EXPR: let expressions of the form: <type> <name> = <def> ; <body>
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName type;
  LocName name;
  Expr* def;
  Expr* body;
} LetExpr;

// COND_EXPR: Conditional expressions of the form: <select>?(<argv>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  Expr* select;
  int argc;
  Expr** argv;
} CondExpr;

typedef struct {
  LocName type;
  LocName name;
} Field;

typedef enum { STRUCT_DECL, UNION_DECL, FUNC_DECL, PROC_DECL } DeclTag;

typedef struct {
  DeclTag tag;
  LocName name;
} Decl;

typedef struct {
  DeclTag tag;
  LocName name;
  int fieldc;
  Field* fieldv;
} TypeDecl;

typedef struct {
  DeclTag tag;
  LocName name;
  LocName return_type;
  Expr* body;
  int argc;
  Field* argv;
} FuncDecl;

typedef enum {
  POLARITY_GET,
  POLARITY_PUT
} Polarity;

typedef struct {
  LocName type;
  LocName name;
  Polarity polarity;
} Port;

typedef enum {
  EVAL_ACTN,
  GET_ACTN,
  PUT_ACTN,
  COND_ACTN,
  CALL_ACTN,
  LINK_ACTN,
  EXEC_ACTN,
} ActnTag;

// Actn is the base structure for all  actions. Each specialization of
// Actn will have the same initial layout with tag and location.

typedef struct {
  ActnTag tag;
  Loc* loc;
} Actn;

// EVAL_ACTN: Processes of the form: $(<expr>)
typedef struct {
  ActnTag tag;
  Loc* loc;
  Expr* expr;
} EvalActn;

// GET_ACTN: Processes of the form: <pname>~()
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName port;
} GetActn;

// PUT_ACTN: Processes of the form: <pname>~(<expr>)
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName port;
  Expr* expr;
} PutActn;

// CALL_ACTN: Processes of the form: <tname>(<port>, ... ; <expr>, ...)
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName proc;
  int portc;
  LocName* ports;   // Array of portc ports
  int exprc;
  Expr** exprs;      // Array of exprc exprs
} CallActn;

// LINK_ACTN: Processes of the form:
//    <tname> '<~>' <pname> ',' <pname> ';' <actn>
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName type;
  LocName getname;
  LocName putname;
  Actn* body;
} LinkActn;

typedef struct {
  Field var;
  Actn* actn;
} Exec;

// EXEC_ACTN: Processes of the form:
//    <tname> <vname> = <actn>,  ...  ; <body>
typedef struct {
  ActnTag tag;
  Loc* loc;
  int execc;
  Exec* execv;          // Array of execc execs.
  Actn* body;
} ExecActn;

// COND_ACTN: Processes of the form: <expr>?(<proc>, ...)
typedef struct {
  ActnTag tag;
  Loc* loc;
  Expr* select;
  int argc;
  Actn** args;    //  Array of argc args.
} CondActn;

typedef struct {
  DeclTag tag;
  LocName name;
  LocName return_type;
  Actn* body;
  int portc;
  Port* portv;              // Array of portc ports.
  int argc;
  Field* argv;              // Array of argv fields.
} ProcDecl;

// An environment contains all the type, function, and process declarations
// for a program.
typedef struct {
  int declc;
  Decl** declv;
} Env;

Env* NewEnv(Allocator* alloc, int declc, Decl** declv);

// Tokenizer
// A stream of tokens is represented using the TokenStream data structure.
// Tokens can be read either from a file or a string.
//
// The conventional variable name for a TokenStream* is 'toks'.

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
  Loc loc;
} TokenStream;

bool OpenFdTokenStream(TokenStream* toks, int fd, const char* source);
bool OpenFileTokenStream(TokenStream* toks, const char* filename);
bool OpenStringTokenStream(TokenStream* toks, const char* source, const char* string);
void CloseTokenStream(TokenStream* toks);
bool IsEOFToken(TokenStream* toks);
bool IsToken(TokenStream* toks, char which);
bool GetToken(TokenStream* toks, char which);
bool IsNameToken(TokenStream* toks);
bool GetNameToken(Allocator* alloc, TokenStream* toks, const char* expected, LocName* name);
void UnexpectedToken(TokenStream* toks, const char* expected);

// Parser
Env* ParseProgram(Allocator* alloc, TokenStream* toks);

// Checker
bool CheckProgram(Env* env);

// BitStream
// Bit streams are represented as sequences of ascii digits '0' and '1'.
// TODO: Support more efficient encodings of bit streams when desired.
typedef struct {
  int fd;
} OutputBitStream;

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd);
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits);

// Encoder
void EncodeProgram(OutputBitStream* stream, Env* env);

#endif  // INTERNAL_H_
