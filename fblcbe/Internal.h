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

void ReportError(const char* format, const Loc* loc, ...);

typedef struct LocName {
  Loc* loc;
  Name name;
} LocName;

typedef enum {
  FBLC_VAR_EXPR,
  FBLC_APP_EXPR,
  FBLC_ACCESS_EXPR,
  FBLC_UNION_EXPR,
  FBLC_LET_EXPR,
  FBLC_COND_EXPR,
} ExprTag;

// Expr is the base structure for all  expressions. Each
// specialization of Expr has the same initial layout with tag and
// location.

typedef struct {
  ExprTag tag;
  Loc* loc;
} Expr;

// FBLC_VAR_EXPR: Variable expressions of the form: <name>
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName name;
} VarExpr;

// FBLC_APP_EXPR: Application expressions of the form: <func>(<argv>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName func;
  int argc;
  Expr** argv;
} AppExpr;

// FBLC_ACCESS_EXPR: Member access expressions of the form: <object>.<field>
typedef struct {
  ExprTag tag;
  Loc* loc;
  const Expr* object;
  LocName field;
} AccessExpr;

// FBLC_UNION_EXPR: Union literals of the form: <type>:<field>(<value>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName type;
  LocName field;
  const Expr* value;
} UnionExpr;

// FBLC_LET_EXPR: let expressions of the form: <type> <name> = <def> ; <body>
typedef struct {
  ExprTag tag;
  Loc* loc;
  LocName type;
  LocName name;
  const Expr* def;
  const Expr* body;
} LetExpr;

// FBLC_COND_EXPR: Conditional expressions of the form: <select>?(<argv>)
typedef struct {
  ExprTag tag;
  Loc* loc;
  const Expr* select;
  int argc;
  Expr** argv;
} CondExpr;

typedef enum { FBLC_KIND_UNION, FBLC_KIND_STRUCT } Kind;

typedef struct {
  LocName type;
  LocName name;
} Field;

typedef struct {
  LocName name;
  Kind kind;
  int fieldc;
  Field* fieldv;
} Type;

typedef struct {
  LocName name;
  LocName return_type;
  Expr* body;
  int argc;
  Field* argv;
} Func;

typedef enum { FBLC_POLARITY_PUT, FBLC_POLARITY_GET } Polarity;

typedef struct {
  LocName type;
  LocName name;
  Polarity polarity;
} Port;

typedef enum {
  FBLC_EVAL_ACTN,
  FBLC_GET_ACTN,
  FBLC_PUT_ACTN,
  FBLC_CALL_ACTN,
  FBLC_LINK_ACTN,
  FBLC_EXEC_ACTN,
  FBLC_COND_ACTN,
} ActnTag;

// Actn is the base structure for all  actions. Each specialization of
// Actn will have the same initial layout with tag and location.

typedef struct {
  ActnTag tag;
  Loc* loc;
} Actn;

// FBLC_EVAL_ACTN: Processes of the form: $(<expr>)
typedef struct {
  ActnTag tag;
  Loc* loc;
  const Expr* expr;
} EvalActn;

// FBLC_GET_ACTN: Processes of the form: <pname>~()
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName port;
} GetActn;

// FBLC_PUT_ACTN: Processes of the form: <pname>~(<expr>)
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName port;
  Expr* expr;
} PutActn;

// FBLC_CALL_ACTN: Processes of the form: <tname>(<port>, ... ; <expr>, ...)
typedef struct {
  ActnTag tag;
  Loc* loc;
  LocName proc;
  int portc;
  LocName* ports;   // Array of portc ports
  int exprc;
  Expr** exprs;      // Array of exprc exprs
} CallActn;

// FBLC_LINK_ACTN: Processes of the form:
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

// FBLC_EXEC_ACTN: Processes of the form:
//    <tname> <vname> = <actn>,  ...  ; <body>
typedef struct {
  ActnTag tag;
  Loc* loc;
  int execc;
  Exec* execv;          // Array of execc execs.
  Actn* body;
} ExecActn;

// FBLC_COND_ACTN: Processes of the form: <expr>?(<proc>, ...)
typedef struct {
  ActnTag tag;
  Loc* loc;
  Expr* select;
  int argc;
  Actn** args;    //  Array of argc args.
} CondActn;

typedef struct {
  LocName name;
  LocName return_type;
  Actn* body;
  int portc;
  Port* portv;              // Array of portc ports.
  int argc;
  Field* argv;              // Array of argv fields.
} Proc;

// An environment contains all the type, function, and process declarations
// for a program. All names used for types, functions, and processes must be
// unique. This is enforced during the construction of the environment.
typedef struct TypeEnv {
  Type* decl;
  struct TypeEnv* next;
} TypeEnv;

typedef struct FuncEnv {
  Func* decl;
  struct FuncEnv* next;
} FuncEnv;

typedef struct ProcEnv {
  Proc* decl;
  struct ProcEnv* next;
} ProcEnv;

typedef struct Env {
  TypeEnv* types;
  FuncEnv* funcs;
  ProcEnv* procs;
} Env;

Env* NewEnv(Allocator* alloc);
Type* LookupType(const Env* env, Name name);
Func* LookupFunc(const Env* env, Name name);
Proc* LookupProc(const Env* env, Name name);
bool AddType(Allocator* alloc, Env* env, Type* type);
bool AddFunc(Allocator* alloc, Env* env, Func* func);
bool AddProc(Allocator* alloc, Env* env, Proc* proc);

// Value

// Value is the base structure for StructValue and UnionValue,
// both of which have the same initial layout.

typedef struct {
  int refcount;
  Type* type;
} Value;

// For struct values 'fieldv' contains the field data in the order the fields
// are declared in the type declaration.

typedef struct {
  int refcount;
  Type* type;
  Value** fieldv;
} StructValue;

// For union values, 'tag' is the index of the active field and 'field'
// is stores the value of the active field.

typedef struct {
  int refcount;
  Type* type;
  int tag;
  Value* field;
} UnionValue;

StructValue* NewStructValue(Type* type);
UnionValue* NewUnionValue(Type* type);
Value* Copy(Value* src);
void Release(Value* value);
void PrintValue(FILE* fout, Value* value);
int TagForField(const Type* type, Name field);

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
Value* ParseValue(Env* env, Type* type, TokenStream* toks);

// Checker
bool CheckProgram(const Env* env);

// BitStream
// Bit streams are represented as sequences of ascii digits '0' and '1'.
// TODO: Support more efficient encodings of bit streams when desired.
typedef struct {
  int fd;
} OutputBitStream;

void OpenBinaryOutputBitStream(OutputBitStream* stream, int fd);
void WriteBits(OutputBitStream* stream, size_t num_bits, uint32_t bits);

// Encoder
void EncodeProgram(OutputBitStream* stream, const Env* env);

#endif  // INTERNAL_H_
