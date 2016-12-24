// Evaluator.c --
//
//   This file implements routines for evaluating expressions.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "fblc.h"

typedef struct Vars Vars;
typedef struct Ports Ports;
typedef struct Cmd Cmd;
typedef struct Thread Thread;

// Threads
//
//   A thread holds the state of a single thread of execution, including local
//   variables, local ports, and a sequence of commands to execute.
//
//   Threads often live on thread lists. A 'next' field is included in the
//   Thread data structure to facilitate putting threads in lists.
//
//   The Threads data structure represents a singly-linked list of threads
//   with head and tail pointer. Threads are added to the tail and removed
//   from the head.

struct Thread {
  Vars* vars;
  Ports* ports;
  Cmd* cmd;
  Thread* next;
};

typedef struct Threads {
  Thread* head;
  Thread* tail;
} Threads;

static Thread* NewThread(FblcArena* arena, Vars* vars, Ports* ports, Cmd* cmd);
static void FreeThread(FblcArena*, Thread* thread);
static void AddThread(Threads* threads, Thread* thread);
static Thread* GetThread(Threads* threads);

// Links
//
// A Link consists of a list of values and a list of threads blocked waiting
// to get values from the link. Values are added to the tail of the values
// list and taken from the head. The empty values list is represented with
// head and tail both set to NULL.

typedef struct Values {
  FblcValue* value;
  struct Values* next;
} Values;

typedef struct Link {
  Values* head;
  Values* tail;
  struct Threads waiting;
} Link;

static Link* NewLink(FblcArena* arena);
static void FreeLink(FblcArena* arena, Link* link);
static void PutValue(FblcArena* arena, Link* link, FblcValue* value);
static FblcValue* GetValue(FblcArena* arena, Link* link);

// The following defines a Vars structure for storing the value of local
// variables. It is possible to extend a local variable scope without
// modifying the original local variable scope, and it is possible to access
// the address of a value stored in the variable scope. Typical usage is to
// extend the variable scope by reserving slots for values that are yet to be
// computed, and to ensure the values are computed and updated by the time the
// scope is used.

struct Vars {
  FblcValue* value;
  struct Vars* next;
};

// List of ports in scope.

struct Ports {
  Link* link;
  struct Ports* next;
};
static FblcValue** LookupRef(Vars* vars, FblcVarId id);
static FblcValue* LookupVal(Vars* vars, FblcVarId id);
static Vars* AddVar(FblcArena* arena, Vars* vars);
static Link* LookupPort(Ports* ports, FblcPortId id);
static Ports* AddPort(FblcArena* arena, Ports* ports, Link* link);

// The evaluator works by breaking down action and expression evaluation into
// a sequence of commands that can be executed in turn. All of the state of
// evaluation, including the stack, is stored explicitly in the command list.
// By storing the stack explicitly instead of piggy-backing off of the C
// runtime stack, we are able to implement the evaluator as a single while
// loop and avoid problems with supporting tail recursive programs.

// The following CmdTag enum identifies the types of commands used to evaluate
// actions and expressions. The Cmd struct represents a command list.
typedef enum {
  CMD_EXPR, CMD_ACTN, CMD_ACCESS, CMD_COND_EXPR, CMD_COND_ACTN,
  CMD_SCOPE, CMD_JOIN, CMD_PUT, CMD_FREE_LINK,
} CmdTag;

// Cmd is the base structure common to all commands.
// Specific commands will have the same initial layout, with additional
// data following.
struct Cmd {
  CmdTag tag;
  struct Cmd* next;
};

// CMD_EXPR: The expr command evaluates expr and stores the resulting value in
// *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcExpr* expr;
  FblcValue** target;
} ExprCmd;

// CMD_ACTN: The actn command executes actn and stores the resulting value in
// *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcActn* actn;
  FblcValue** target;
} ActnCmd;

// CMD_ACCESS: The access command accesses the given field of
// the given value and stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue* value;
  size_t field;
  FblcValue** target;
} AccessCmd;

// CMD_COND_EXPR: The condition expression command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen expression and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue* value;
  FblcExpr** choices;
  FblcValue** target;
} CondExprCmd;

// CMD_COND_ACTN: The conditional action command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen action and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue* value;
  FblcActn** choices;
  FblcValue** target;
} CondActnCmd;

// CMD_SCOPE: The scope command sets the current ports and vars to the given
// ports and vars.
// If is_pop is true, then the old scope is freed before pushing the new one.
// More precisely, the current scope's vars and ports are freed one-by-one
// until they match the given vars and ports or there are no more left. Then
// the scope change is made if necessary.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  Vars* vars;
  Ports* ports;
  bool is_pop;
} ScopeCmd;

// CMD_JOIN: The join command halts the current thread of execution until
// count has reached zero.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  int count;
} JoinCmd;

// CMD_PUT: The put command puts a value onto a link and into the target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue** target;
  Link* link;
  FblcValue* value;
} PutCmd;

// CMD_FREE_LINK: Free resources associated with the given link.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  Link* link;
} FreeLinkCmd;

static Cmd* MkExprCmd(FblcArena* arena, FblcExpr* expr, FblcValue** target, Cmd* next);
static Cmd* MkActnCmd(FblcArena* arena, FblcActn* actn, FblcValue** target, Cmd* next);
static Cmd* MkAccessCmd(FblcArena* arena, FblcValue* value, size_t field, FblcValue** target, Cmd* next);
static Cmd* MkCondExprCmd(FblcArena* arena, FblcValue* value, FblcExpr** choices, FblcValue** target, Cmd* next);
static Cmd* MkCondActnCmd(FblcArena* arena, FblcValue* value, FblcActn** choices, FblcValue** target, Cmd* next);
static Cmd* MkScopeCmd(FblcArena* arena, Vars* vars, Ports* ports, bool is_pop, Cmd* next);
static Cmd* MkPushScopeCmd(FblcArena* arena, Vars* vars, Ports* ports, Cmd* next);
static Cmd* MkPopScopeCmd(FblcArena* arena, Vars* vars, Ports* ports, Cmd* next);
static Cmd* MkJoinCmd(FblcArena* arena, size_t count, Cmd* next);
static Cmd* MkPutCmd(FblcArena* arena, FblcValue** target, Link* link, Cmd* next);
static Cmd* MkFreeLinkCmd(FblcArena* arena, Link* link, Cmd* next);

static void Run(FblcArena* arena, FblcProgram* program, Threads* threads, Thread* thread);

// NewThread --
//
//   Create a new thread.
//
// Inputs:
//   arena - The arena to use for allocation.
//   vars - The thread's local variables.
//   ports - The thread's ports.
//   cmd - The thread's command list.
//
// Results:
//   A newly allocated and initialized thread object.
//
// Side effects:
//   A new thread object is allocated. The thread object should be freed by
//   calling FreeThread when the object is no longer needed.

static Thread* NewThread(FblcArena* arena, Vars* vars, Ports* ports, Cmd* cmd)
{
  Thread* thread = arena->alloc(arena, sizeof(Thread));
  thread->vars = vars;
  thread->ports = ports;
  thread->cmd = cmd;
  thread->next = NULL;
  return thread;
}

// FreeThread --
//
//   Free a thread object that is no longer needed.
//
// Inputs:
//   arena - The arena used to allocate the thread.
//   thread - The thread object to free.
//
// Results:
//   None.
//
// Side effects:
//   The resources for the thead object are released. The thread object should
//   not be used after this call.

static void FreeThread(FblcArena* arena, Thread* thread)
{
  arena->free(arena, thread);
}

// AddThread --
//
//   Add a thread to the thread list.
//
// Inputs:
//   threads - The current thread list.
//   thread - The thread to add to the thread list.
//
// Results:
//   None.
//
// Side effects:
//   The thread is added to the current thread list.

static void AddThread(Threads* threads, Thread* thread)
{
  assert(thread->next == NULL);
  if (threads->head == NULL) {
    assert(threads->tail == NULL);
    threads->head = thread;
    threads->tail = thread;
  } else {
    threads->tail->next = thread;
    threads->tail = thread;
  }
}

// GetThread --
//
//   Get the next thread from the thread list.
//
// Inputs:
//   threads - The current thread list.
//
// Results:
//   The next thread on the thread list, or NULL if there are no more threads
//   on the thread list.
//
// Side Effects:
//   The returned thread is removed from the current thread list.

static Thread* GetThread(Threads* threads)
{
  Thread* thread = threads->head;
  if (thread != NULL) {
    threads->head = thread->next;
    thread->next = NULL;
    if (threads->head == NULL) {
      threads->tail = NULL;
    }
  }
  return thread;
}

// NewLink --
//  
//   Create a new link object.
//
// Inputs:
//   arena - The arena to use for allocations.
//
// Results:
//   A newly created link object with no initial values or waiting threads.
//
// Side effects:
//   Allocates resources for a link object. The resources for the link object
//   should be freed by calling FreeLink.

static Link* NewLink(FblcArena* arena)
{
  Link* link = arena->alloc(arena, sizeof(Link));
  link->head = NULL;
  link->tail = NULL;
  link->waiting.head = NULL;
  link->waiting.tail = NULL;
  return link;
}

// FreeLink --
//
//   Free the given link object and resources associated with it.
//
// Inputs:
//   arena - The arena used to allocate the link.
//   link - The link to free.
//
// Results:
//   None.
//
// Side effects:
//   The link object is freed along with any resources associated with it.
//   After this call, the link object should not be accessed again.

void FreeLink(FblcArena* arena, Link* link)
{
  Values* values = link->head;
  while (values != NULL) {
    link->head = values->next;
    FblcRelease(arena, values->value);
    arena->free(arena, values);
    values = link->head;
  }

  Thread* thread = GetThread(&(link->waiting));
  while (thread != NULL) {
    FreeThread(arena, thread);
    thread = GetThread(&(link->waiting));
  }

  arena->free(arena, link);
}

// PutValue --
//  
//   Put a value onto a link.
//
// Inputs:
//   arena - The arena to use for allocations.
//   link - The link to put the value on.
//   value - The value to put on the link.
//
// Results:
//   None.
//
// Side effects:
//   Places the given value on the link.

static void PutValue(FblcArena* arena, Link* link, FblcValue* value)
{
  Values* ntail = arena->alloc(arena, sizeof(Values));
  ntail->value = value;
  ntail->next = NULL;
  if (link->head == NULL) {
    assert(link->tail == NULL);
    link->head = ntail;
  } else {
    link->tail->next = ntail;
  }
  link->tail = ntail;
}

// GetValue --
//
//   Get the next value from the link.
//
// Inputs:
//   arena - The arena used to put values on the link.
//   link - The link to get the value from.
//
// Results:
//   The next value on the link, or NULL if there are no values available on
//   the link.
//
// Side effects
//   Removes the gotten value from the link.

static FblcValue* GetValue(FblcArena* arena, Link* link)
{
  FblcValue* value = NULL;
  if (link->head != NULL) {
    Values* values = link->head;
    value = values->value;
    link->head = values->next;
    arena->free(arena, values);
    if (link->head == NULL) {
      link->tail = NULL;
    }
  }
  return value;
}

// LookupRef --
//
//   Look up a reference to the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable reference.
//   id - The id of the variable to lookup.
//
// Returns:
//   A pointer to the value of the variable with the given id in vars.
//
// Side effects:
//   The behavior is undefined if the variable is not found in scope.

static FblcValue** LookupRef(Vars* vars, FblcVarId id)
{
  for (size_t i = 0; i < id; ++i) {
    assert(vars != NULL);
    vars = vars->next;
  }
  assert(vars != NULL);
  return &(vars->value);
}

// LookupVal --
//
//   Look up the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable value.
//   id - The id of the variable to lookup.
//
// Returns:
//   The value of the variable with the given id in scope.
//
// Side effects:
//   The behavior is undefined if the variable is not found in scope.

static FblcValue* LookupVal(Vars* vars, FblcVarId id)
{
  FblcValue** ref = LookupRef(vars, id);
  return *ref;
}

// AddVar --
//   
//   Extend the given scope with a new variable. Use LookupRef to access the
//   newly added variable.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vars - The scope to extend.
//
// Returns:
//   A new scope with the new variable and the contents of the given scope.
//
// Side effects:
//   Performs arena allocations.

static Vars* AddVar(FblcArena* arena, Vars* vars)
{
  Vars* newvars = arena->alloc(arena, sizeof(Vars));
  newvars->value = NULL;
  newvars->next = vars;
  return newvars;
}

// LookupPort --
//   
//   Lookup up the link associated with the given port.
//
// Inputs:
//   ports - The set of ports to look at.
//   id - The id of the port to look up.
//
// Returns:
//   The link associated with the given port.
//
// Side effects:
//   The behavior is undefined if the port is not found.

static Link* LookupPort(Ports* ports, FblcPortId id)
{
  for (size_t i = 0; i < id; ++i) {
    assert(ports != NULL);
    ports = ports->next;
  }
  assert(ports != NULL);
  return ports->link;
}

// AddPort --
//   
//   Extend the given port scope with a new port.
//
// Inputs:
//   arena - The arena to use for allocations.
//   ports - The scope to extend.
//   link - The underlying link of the port to add.
//
// Returns:
//   A new scope with the new port and the contents of the given scope.
//
// Side effects:
//   Performs arena allocations.

static Ports* AddPort(FblcArena* arena, Ports* ports, Link* link)
{
  Ports* nports = arena->alloc(arena, sizeof(Ports));
  nports->link = link;
  nports->next = ports;
  return nports;
}

// MkExprCmd --
//
//   Creates a command to evaluate the given expression, storing the resulting
//   value at the given target location.
//
// Inputs:
//   alloc - The arena to use for allocations.
//   expr - The expression for the created command to evaluate. This must not
//          be NULL.
//   target - The target of the result of evaluation. This must not be NULL.
//   next - The command to run after this command.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkExprCmd(FblcArena* arena, FblcExpr* expr, FblcValue** target, Cmd* next)
{
  assert(expr != NULL);
  assert(target != NULL);

  ExprCmd* cmd = arena->alloc(arena, sizeof(ExprCmd));
  cmd->tag = CMD_EXPR;
  cmd->next = next;
  cmd->expr = expr;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkActnCmd --
//
//   Creates a command to evaluate the given action, storing the resulting
//   value at the given target location.
//
// Inputs:
//   arena - The arena to use for allocations.
//   actn - The action for the created command to evaluate. This must not
//          be NULL.
//   target - The target of the result of evaluation.
//   next - The command to run after this command.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkActnCmd(FblcArena* arena, FblcActn* actn, FblcValue** target, Cmd* next)
{
  assert(actn != NULL);
  assert(target != NULL);

  ActnCmd* cmd = arena->alloc(arena, sizeof(ActnCmd));
  cmd->tag = CMD_ACTN;
  cmd->next = next;
  cmd->actn = actn;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkAccessCmd --
//   
//   Create a command to access a field from a value, storing the result at
//   the given target location.
//
// Inputs:
//   arena - The arena to use for allocations.
//   value - The value that will be accessed.
//   field - The id of the field to access.
//   target - The target destination of the accessed field.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkAccessCmd(FblcArena* arena, FblcValue* value, size_t field, FblcValue** target, Cmd* next)
{
  AccessCmd* cmd = arena->alloc(arena, sizeof(AccessCmd));
  cmd->tag = CMD_ACCESS;
  cmd->next = next;
  cmd->value = value;
  cmd->field = field;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkCondExprCmd --
//   
//   Create a command to select and evaluate an expression based on the tag of
//   the given value.
//
// Inputs:
//   arena - The arena to use for allocations.
//   value - The value to condition on.
//   choices - The list of expressions to choose from based on the value.
//   target - The target destination for the final evaluated value.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkCondExprCmd(FblcArena* arena, FblcValue* value, FblcExpr** choices, FblcValue** target, Cmd* next)
{
  CondExprCmd* cmd = arena->alloc(arena, sizeof(CondExprCmd));
  cmd->tag = CMD_COND_EXPR;
  cmd->next = next;
  cmd->value = value;
  cmd->choices = choices;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkCondActnCmd --
//   
//   Create a command to select and evaluate an action based on the tag of
//   the given value.
//
// Inputs:
//   arena - The arena to use for allocations.
//   value - The value to condition on.
//   choices - The list of actions to choose from based on the value.
//   target - The target destination for the final evaluated value.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkCondActnCmd(FblcArena* arena, FblcValue* value, FblcActn** choices, FblcValue** target, Cmd* next)
{
  CondActnCmd* cmd = arena->alloc(arena, sizeof(CondActnCmd));
  cmd->tag = CMD_COND_ACTN;
  cmd->next = next;
  cmd->value = value;
  cmd->choices = choices;
  cmd->target = target;
  return (Cmd*)cmd;
}

// MkScopeCmd --
//   
//   Create a command to change the current ports and local variable scope.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vars - The new value of the local variable scope to use.
//   ports - The new value of the ports scope to use.
//   is_pop - True if this should be a pop scope command.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkScopeCmd(FblcArena* arena, Vars* vars, Ports* ports, bool is_pop, Cmd* next)
{
  ScopeCmd* cmd = arena->alloc(arena, sizeof(ScopeCmd));
  cmd->tag = CMD_SCOPE;
  cmd->next = next;
  cmd->vars = vars;
  cmd->ports = ports;
  cmd->is_pop = is_pop;
  return (Cmd*)cmd;
}

// MkPushScopeCmd --
//   
//   Create a command to change the current ports and local variable scope
//   without freeing the previous ports and local variable scope.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vars - The new value of the local variable scope to use.
//   ports - The new value of the ports scope to use.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkPushScopeCmd(FblcArena* arena, Vars* vars, Ports* ports, Cmd* next)
{
  return MkScopeCmd(arena, vars, ports, false, next);
}

// MkPopScopeCmd --
//   
//   Create a command to change the current ports and local variable scope
//   that frees the previous ports and local variable scope.
//
// Inputs:
//   arena - The arena to use for allocations.
//   vars - The new value of the local variable scope to use.
//   ports - The new value of the ports scope to use.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkPopScopeCmd(FblcArena* arena, Vars* vars, Ports* ports, Cmd* next)
{
  return MkScopeCmd(arena, vars, ports, true, next);
}

// MkJoinCmd --
//
//   Create a join command.
//
// Inputs:
//   arena - The arena to use for allocations.
//   count - The number of threads to join.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkJoinCmd(FblcArena* arena, size_t count, Cmd* next)
{
  JoinCmd* cmd = arena->alloc(arena, sizeof(JoinCmd));
  cmd->tag = CMD_JOIN;
  cmd->next = next;
  cmd->count = count;
  return (Cmd*)cmd;
}

// MkPutCmd --
//
//   Create a put command.
//
// Inputs:
//   arena - The arena to use for allocations.
//   target - The target destination for the put value.
//   link - The link to put the value on.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkPutCmd(FblcArena* arena, FblcValue** target, Link* link, Cmd* next)
{
  assert(target != NULL);
  PutCmd* cmd = arena->alloc(arena, sizeof(PutCmd));
  cmd->tag = CMD_PUT;
  cmd->next = next;
  cmd->target = target;
  cmd->link = link;
  cmd->value = NULL;
  return (Cmd*)cmd;
}

// MkFreeLinkCmd --
//
//   Create a free link command.
//
// Inputs:
//   arena - The arena to use for allocations.
//   link - The link to free.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   Performs arena allocations.

static Cmd* MkFreeLinkCmd(FblcArena* arena, Link* link, Cmd* next)
{
  FreeLinkCmd* cmd = arena->alloc(arena, sizeof(FreeLinkCmd));
  cmd->tag = CMD_FREE_LINK;
  cmd->next = next;
  cmd->link = link;
  return (Cmd*)cmd;
}

// Run --
//
//   Spend a finite amount of time executing commands for a thread.
//
// Inputs:
//   arena - The arena to use for allocations.
//   env - The environment in which to run the thread.
//   threads - The list of currently active threads.
//   thread - The thread to run.
//
// Result:
//   None.
//
// Side effects:
//   Threads are added to the threads list based on the commands executed for
//   the thread. If the thread is executed to completion, it will be freed.
//   Otherwise it will be added back to the threads list representing the
//   continuation of this thread.
//   Performs arena allocations.

static void Run(FblcArena* arena, FblcProgram* program, Threads* threads, Thread* thread)
{
  for (int i = 0; i < 1024 && thread->cmd != NULL; i++) {
    Cmd* next = thread->cmd->next;
    switch (thread->cmd->tag) {
      case CMD_EXPR: {
        ExprCmd* cmd = (ExprCmd*)thread->cmd;
        FblcExpr* expr = cmd->expr;
        FblcValue** target = cmd->target;
        switch (expr->tag) {
          case FBLC_VAR_EXPR: {
            FblcVarExpr* var_expr = (FblcVarExpr*)expr;
            FblcValue* value = LookupVal(thread->vars, var_expr->var);
            *target = FblcCopy(arena, value);
            break;
          }

          case FBLC_APP_EXPR: {
            FblcAppExpr* app_expr = (FblcAppExpr*)expr;
            FblcDecl* decl = program->declv[app_expr->func];
            if (decl->tag == FBLC_STRUCT_DECL) {
              // Create the struct value now, then add commands to evaluate
              // the arguments to fill in the fields with the proper results.
              FblcTypeDecl* struct_decl = (FblcTypeDecl*)decl;
              size_t fieldc = struct_decl->fieldc;
              FblcValue* value = FblcNewStruct(arena, fieldc);
              *target = value;
              for (size_t i = 0; i < fieldc; ++i) {
                next = MkExprCmd(arena, app_expr->argv[i], value->fields + i, next);
              }
              break;
            }

            if (decl->tag == FBLC_FUNC_DECL) {
              // Add to the top of the command list:
              //  arg -> ... -> arg -> push scope -> body -> pop scope -> next 
              // The results of the arg evaluations will be stored directly in
              // the vars for the first scope command.
              // TODO: Avoid memory leaks in the case of tail calls.

              FblcFuncDecl* func = (FblcFuncDecl*)decl;
              next = MkPopScopeCmd(arena, thread->vars, thread->ports, next);
              next = MkExprCmd(arena, func->body, target, next);
              next = MkPushScopeCmd(arena, NULL, thread->ports, next);

              ScopeCmd* scmd = (ScopeCmd*)next;
              Vars* nvars = NULL;
              for (size_t i = 0; i < func->argc; ++i) {
                nvars = AddVar(arena, nvars);
                next = MkExprCmd(arena, app_expr->argv[i], LookupRef(nvars, 0), next);
              }
              scmd->vars = nvars;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case FBLC_ACCESS_EXPR: {
            // Add to the top of the command list:
            // object -> access -> ...
            FblcAccessExpr* access_expr = (FblcAccessExpr*)expr;
            AccessCmd* acmd = (AccessCmd*)MkAccessCmd(arena, NULL, access_expr->field, target, next);
            next = MkExprCmd(arena, access_expr->object, &(acmd->value), (Cmd*)acmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            // Create the union value now, then add a command to evaluate the
            // argument of the union constructor and set the field of the
            // union value.
            FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
            FblcDecl* decl = program->declv[union_expr->type];
            assert(decl->tag == FBLC_UNION_DECL);
            FblcTypeDecl* union_decl = (FblcTypeDecl*)decl;
            *target = FblcNewUnion(arena, union_decl->fieldc, union_expr->field, NULL);
            next = MkExprCmd(arena, union_expr->body, (*target)->fields, next);
            break;
          }

          case FBLC_LET_EXPR: {
            // Add to the top of the command list:
            // def -> push scope -> body -> pop scope -> ...
            // TODO: Avoid memory leaks for tail calls.
            FblcLetExpr* let_expr = (FblcLetExpr*)expr;

            next = MkPopScopeCmd(arena, thread->vars, thread->ports, next);
            Vars* nvars = AddVar(arena, thread->vars);
            next = MkExprCmd(arena, let_expr->body, target, next);
            next = MkPushScopeCmd(arena, nvars, thread->ports, next);
            next = MkExprCmd(arena, let_expr->def, LookupRef(nvars, 0), next);
            break;
          }

          case FBLC_COND_EXPR: {
            // Add to the top of the command list:
            // select -> econd -> ...
            FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
            CondExprCmd* ccmd = (CondExprCmd*)MkCondExprCmd(arena, NULL, cond_expr->argv, target, next);
            next = MkExprCmd(arena, cond_expr->select, &(ccmd->value), (Cmd*)ccmd);
            break;
          }
        }
        break;
      }

      case CMD_ACTN: {
        ActnCmd* cmd = (ActnCmd*)thread->cmd;
        FblcActn* actn = cmd->actn;
        FblcValue** target = cmd->target;
        switch (actn->tag) {
          case FBLC_EVAL_ACTN: {
            FblcEvalActn* eval_actn = (FblcEvalActn*)actn;
            next = MkExprCmd(arena, eval_actn->expr, target, next);
            break;
          }

          case FBLC_GET_ACTN: {
            FblcGetActn* get_actn = (FblcGetActn*)actn;
            Link* link = LookupPort(thread->ports, get_actn->port);
            assert(link != NULL && "Get port not in scope");

            *target = GetValue(arena, link);
            if (*target == NULL) {
              AddThread(&link->waiting, thread);
              return;
            }
            break;
          }

          case FBLC_PUT_ACTN: {
            // expr -> put -> next
            FblcPutActn* put_actn = (FblcPutActn*)actn;
            Link* link = LookupPort(thread->ports, put_actn->port);
            assert(link != NULL && "Put port not in scope");
            PutCmd* pcmd = (PutCmd*)MkPutCmd(arena, target, link, next);
            next = MkExprCmd(arena, put_actn->arg, &(pcmd->value), (Cmd*)pcmd);
            break;
          }

          case FBLC_CALL_ACTN: {
            FblcCallActn* call_actn = (FblcCallActn*)actn;
            FblcDecl* decl = program->declv[call_actn->proc];
            assert(decl->tag == FBLC_PROC_DECL);
            FblcProcDecl* proc = (FblcProcDecl*)decl;
            assert(proc->portc == call_actn->portc);
            assert(proc->argc == call_actn->argc);

            // Add to the top of the command list:
            //  arg -> ... -> arg -> push scope -> body -> pop scope -> next
            // The results of the arg evaluations will be stored directly in
            // the vars for the scope command.
            // TODO: Avoid memory leaks in the case of tail calls.

            next = MkPopScopeCmd(arena, thread->vars, thread->ports, next);
            next = MkActnCmd(arena, proc->body, target, next);

            Ports* nports = NULL;
            for (size_t i = 0; i < proc->portc; ++i) {
              Link* link = LookupPort(thread->ports, call_actn->portv[i]);
              assert(link != NULL && "port not found in scope");
              nports = AddPort(arena, nports, link);
            }
            next = MkPushScopeCmd(arena, NULL, nports, next);

            ScopeCmd* scmd = (ScopeCmd*)next;
            Vars* nvars = NULL;
            for (size_t i = 0; i < proc->argc; ++i) {
              nvars = AddVar(arena, nvars);
              next = MkExprCmd(arena, call_actn->argv[i], LookupRef(nvars, 0), next);
            }
            scmd->vars = nvars;
            break;
          }

          case FBLC_LINK_ACTN: {
            // actn -> free link -> pop scope -> next
            // Note: This modifies the scope in place rather than insert a
            // push scope command to do that as separate command.
            // TODO: Avoid memory leaks on tail recursion?
            next = MkPopScopeCmd(arena, thread->vars, thread->ports, next);
            FblcLinkActn* link_actn = (FblcLinkActn*)actn;
            Link* link = NewLink(arena);
            thread->ports = AddPort(arena, thread->ports, link);
            thread->ports = AddPort(arena, thread->ports, link);
            next = MkFreeLinkCmd(arena, link, next);
            next = MkActnCmd(arena, link_actn->body, target, next);
            break;
          }

          case FBLC_EXEC_ACTN: {
            // Create new threads for each action to execute.
            // actn .>
            // actn ..> join -> push scope -> actn -> pop scope -> next
            // actn .>
            FblcExecActn* exec_actn = (FblcExecActn*)actn;
            next = MkPopScopeCmd(arena, thread->vars, thread->ports, next);
            next = MkActnCmd(arena, exec_actn->body, target, next);
            ScopeCmd* scmd = (ScopeCmd*)MkPushScopeCmd(arena, NULL, thread->ports, next);
            Cmd* jcmd = MkJoinCmd(arena, exec_actn->execc, (Cmd*)scmd);

            Vars* nvars = thread->vars;
            for (size_t i = 0; i < exec_actn->execc; ++i) {
              nvars = AddVar(arena, nvars);
              FblcValue** target = LookupRef(nvars, 0);
              AddThread(threads, NewThread(arena, thread->vars, thread->ports,
                    MkActnCmd(arena, exec_actn->execv[i], target, jcmd)));
            }
            scmd->vars = nvars;
            next = NULL;
            break;
          }

          case FBLC_COND_ACTN: {
            // Add to the top of the command list:
            // select -> acond -> ...
            FblcCondActn* cond_actn = (FblcCondActn*)actn;
            CondActnCmd* ccmd = (CondActnCmd*)MkCondActnCmd(arena, NULL, cond_actn->argv, target, next);
            next = MkExprCmd(arena, cond_actn->select, &(ccmd->value), (Cmd*)ccmd);
            break;
          }
            break;
        }
        break;
      }               

      case CMD_ACCESS: {
        AccessCmd* cmd = (AccessCmd*)thread->cmd;
        FblcValue** target = cmd->target;
        FblcValue* value = cmd->value;
        switch (value->kind) {
          case FBLC_STRUCT_KIND:
            *target = FblcCopy(arena, value->fields[cmd->field]);
            break;

          case FBLC_UNION_KIND:
            if (value->tag == cmd->field) {
              *target = FblcCopy(arena, *value->fields);
            } else {
              fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
              abort();
            }
            break;

          default:
            assert(false && "Unknown value kind");
        }
        FblcRelease(arena, value);
        break;
      }

      case CMD_COND_EXPR: {
        CondExprCmd* cmd = (CondExprCmd*)thread->cmd;
        FblcValue* value = cmd->value;
        FblcValue** target = cmd->target;
        assert(value->kind == FBLC_UNION_KIND);
        next = MkExprCmd(arena, cmd->choices[value->tag], target, next);
        FblcRelease(arena, value);
        break;
      }

      case CMD_COND_ACTN: {
        CondActnCmd* cmd = (CondActnCmd*)thread->cmd;
        FblcValue* value = cmd->value;
        FblcValue** target = cmd->target;
        assert(value->kind == FBLC_UNION_KIND);
        next = MkActnCmd(arena, cmd->choices[value->tag], target, next);
        FblcRelease(arena, value);
        break;
      }

      case CMD_SCOPE: {
        ScopeCmd* cmd = (ScopeCmd*)thread->cmd;

        if (cmd->is_pop) {
          while (thread->vars != NULL && thread->vars != cmd->vars) {
            Vars* vars = thread->vars;
            thread->vars = vars->next;
            FblcRelease(arena, vars->value);
            arena->free(arena, vars);
          }

          while (thread->ports != NULL && thread->ports != cmd->ports) {
            Ports* ports = thread->ports;
            thread->ports = ports->next;
            arena->free(arena, ports);
          }
        }

        thread->vars = cmd->vars;
        thread->ports = cmd->ports;
        break;
      }

      case CMD_JOIN: {
        JoinCmd* cmd = (JoinCmd*)thread->cmd;
        assert(cmd->count > 0);
        cmd->count--;
        if (cmd->count != 0) {
          FreeThread(arena, thread);
          return;
        }
        break;
      }

      case CMD_PUT: {
        PutCmd* cmd = (PutCmd*)thread->cmd;
        Link* link = cmd->link;
        FblcValue** target = cmd->target;
        *target = cmd->value;
        PutValue(arena, link, FblcCopy(arena, cmd->value));
        Thread* waiting = GetThread(&link->waiting);
        if (waiting != NULL) {
          AddThread(threads, waiting);
        }
        break;
      }

      case CMD_FREE_LINK: {
        FreeLinkCmd* cmd = (FreeLinkCmd*)thread->cmd;
        FreeLink(arena, cmd->link);
        break;
      }
    }

    arena->free(arena, thread->cmd);
    thread->cmd = next;
  }

  if (thread->cmd == NULL) {
    FreeThread(arena, thread);
  } else {
    AddThread(threads, thread);
  }
}

// Execute --
//
//   Execute an process under the given program environment. The program
//   and process must be well formed.
//
// Inputs:
//   arena - The arena to use for allocations.
//   program - The program environment.
//   proc - The process to execute.
//   args - Arguments to the function to execute.
//
// Returns:
//   The result of executing the given procedure in the program environment
//   with the given arguments and ports on open file descriptors 3 and on.
//
// Side effects:
//   Releases the args values.
//   Reads and writes to the open file descriptors for the ports as specified
//   by the program.
//   Performs arena allocations.

FblcValue* Execute(FblcArena* arena, FblcProgram* program, FblcProcDecl* proc, FblcValue** args)
{
  Vars* vars = NULL;
  for (size_t i = 0; i < proc->argc; ++i) {
    vars = AddVar(arena, vars);
    *LookupRef(vars, 0) = args[i];
  }

  FblcValue* result = NULL;
  Cmd* cmd = MkPopScopeCmd(arena, NULL, NULL, NULL);
  cmd = MkActnCmd(arena, proc->body, &result, cmd);

  Link* links[proc->portc];
  Ports* ports = NULL;
  for (size_t i = 0; i < proc->portc; ++i) {
    links[i] = NewLink(arena);
    ports = AddPort(arena, ports, links[i]);
  }

  Threads threads;
  threads.head = NULL;
  threads.tail = NULL;
  AddThread(&threads, NewThread(arena, vars, ports, cmd));

  Thread* thread = GetThread(&threads);
  while (thread != NULL) {
    // Run the current thread.
    Run(arena, program, &threads, thread);

    // Perform whatever IO is ready. Do this after running the current thread
    // to ensure we get whatever final IO there is before terminating.
    for (size_t i = 0; i < proc->portc; ++i) {
      if (proc->portv[i].polarity == FBLC_GET_POLARITY) {
        Thread* waiting = GetThread(&(links[i]->waiting));
        if (waiting != NULL) {
          // TODO: Don't block if there isn't anything available to read.
          // TODO: Flush the input stream to ensure alignment is met.
          InputBitStream stream;
          OpenBinaryFdInputBitStream(&stream, 3+i);
          FblcValue* got = DecodeValue(arena, &stream, program, proc->portv[i].type);
          PutValue(arena, links[i], got);
          AddThread(&threads, waiting);
        }
      } else {
        assert(proc->portv[i].polarity == FBLC_PUT_POLARITY);
        FblcValue* put = GetValue(arena, links[i]);
        if (put != NULL) {
          OutputBitStream stream;
          OpenBinaryOutputBitStream(&stream, 3+i);
          EncodeValue(&stream, put);
          FlushWriteBits(&stream);
          FblcRelease(arena, put);
        }
      }
    }

    thread = GetThread(&threads);
  }

  for (int i = 0; i < proc->portc; i++) {
    FreeLink(arena, links[i]);
  }
  return result;
}
