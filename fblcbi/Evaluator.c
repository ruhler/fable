// Evaluator.c --
//
//   This file implements routines for evaluating expressions.

#include "Internal.h"

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

static Thread* NewThread(Vars* vars, Ports* ports, Cmd* cmd);
static void FreeThread(Thread* thread);
static void AddThread(Threads* threads, Thread* thread);
static Thread* GetThread(Threads* threads);

// Links
//
// A Link consists of a list of values and a list of threads blocked waiting
// to get values from the link. Values are added to the tail of the values
// list and taken from the head. The empty values list is represented with
// head and tail both set to NULL.

typedef struct Values {
  Value* value;
  struct Values* next;
} Values;

typedef struct Link {
  Values* head;
  Values* tail;
  struct Threads waiting;
} Link;

//static Link* NewLink();
static void FreeLink(Link* link);
static void PutValue(Link* link, Value* value);
//static Value* GetValue(Link* link);

// The following defines a Vars structure for storing the value of local
// variables. It is possible to extend a local variable scope without
// modifying the original local variable scope, and it is possible to access
// the address of a value stored in the variable scope. Typical usage is to
// extend the variable scope by reserving slots for values that are yet to be
// computed, and to ensure the values are computed and updated by the time the
// scope is used.

struct Vars {
  Value* value;
  struct Vars* next;
};

// List of ports in scope.

struct Ports {
  Link* link;
  struct Ports* next;
};
static Value** LookupRef(Vars* vars, VarId id);
static Value* LookupVal(Vars* vars, VarId id);
static Vars* AddVar(Vars* vars);
// static Link* LookupPort(Ports* ports, PortId id);
// static Ports* AddPort(Ports* ports, Link* link);

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
  Expr* expr;
  Value** target;
} ExprCmd;

// CMD_ACTN: The actn command executes actn and stores the resulting value in
// *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  Actn* actn;
  Value** target;
} ActnCmd;

// CMD_ACCESS: The access command accesses the given field of
// the given value and stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  Value* value;
  size_t field;
  Value** target;
} AccessCmd;

// CMD_COND_EXPR: The condition expression command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen expression and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  UnionValue* value;
  Expr** choices;
  Value** target;
} CondExprCmd;

// CMD_COND_ACTN: The conditional action command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen action and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  UnionValue* value;
  Actn** choices;
  Value** target;
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
  Value** target;
  Link* link;
  Value* value;
} PutCmd;

// CMD_FREE_LINK: Free resources associated with the given link.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  Link* link;
} FreeLinkCmd;

static Cmd* MkExprCmd(Expr* expr, Value** target, Cmd* next);
static Cmd* MkActnCmd(Actn* actn, Value** target, Cmd* next);
static Cmd* MkAccessCmd(Value* value, size_t field, Value** target, Cmd* next);
static Cmd* MkCondExprCmd(UnionValue* value, Expr** choices, Value** target, Cmd* next);
static Cmd* MkCondActnCmd(UnionValue* value, Actn** choices, Value** target, Cmd* next);
static Cmd* MkScopeCmd(Vars* vars, Ports* ports, bool is_pop, Cmd* next);
static Cmd* MkPushScopeCmd(Vars* vars, Ports* ports, Cmd* next);
static Cmd* MkPopScopeCmd(Vars* vars, Ports* ports, Cmd* next);
static Cmd* MkJoinCmd(size_t count, Cmd* next);
static Cmd* MkPutCmd(Value** target, Link* link, Cmd* next);
static Cmd* MkFreeLinkCmd(Link* link, Cmd* next);

static void Run(Program* program, Threads* threads, Thread* thread);

// NewThread --
//
//   Create a new thread.
//
// Inputs:
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

static Thread* NewThread(Vars* vars, Ports* ports, Cmd* cmd)
{
  Thread* thread = MALLOC(sizeof(Thread));
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
//   thread - The thread object to free.
//
// Results:
//   None.
//
// Side effects:
//   The resources for the thead object are released. The thread object should
//   not be used after this call.

static void FreeThread(Thread* thread)
{
  FREE(thread);
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
//   None.
//
// Results:
//   A newly created link object with no initial values or waiting threads.
//
// Side effects:
//   Allocates resources for a link object. The resources for the link object
//   should be freed by calling FreeLink.

static Link* NewLink()
{
  Link* link = MALLOC(sizeof(Link));
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
//   link - The link to free.
//
// Results:
//   None.
//
// Side effects:
//   The link object is freed along with any resources associated with it.
//   After this call, the link object should not be accessed again.

void FreeLink(Link* link)
{
  Values* values = link->head;
  while (values != NULL) {
    link->head = values->next;
    Release(values->value);
    FREE(values);
    values = link->head;
  }

  Thread* thread = GetThread(&(link->waiting));
  while (thread != NULL) {
    FreeThread(thread);
    thread = GetThread(&(link->waiting));
  }

  FREE(link);
}

// PutValue --
//  
//   Put a value onto a link.
//
// Inputs:
//   link - The link to put the value on.
//   value - The value to put on the link.
//
// Results:
//   None.
//
// Side effects:
//   Places the given value on the link.

static void PutValue(Link* link, Value* value)
{
  Values* ntail = MALLOC(sizeof(Values));
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
//   link - The link to get the value from.
//
// Results:
//   The next value on the link, or NULL if there are no values available on
//   the link.
//
// Side effects
//   Removes the gotten value from the link.

static Value* GetValue(Link* link)
{
  Value* value = NULL;
  if (link->head != NULL) {
    Values* values = link->head;
    value = values->value;
    link->head = values->next;
    FREE(values);
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

static Value** LookupRef(Vars* vars, VarId id)
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

static Value* LookupVal(Vars* vars, VarId id)
{
  Value** ref = LookupRef(vars, id);
  return *ref;
}

// AddVar --
//   
//   Extend the given scope with a new variable. Use LookupRef to access the
//   newly added variable.
//
// Inputs:
//   vars - The scope to extend.
//
// Returns:
//   A new scope with the new variable and the contents of the given scope.
//
// Side effects:
//   None.

static Vars* AddVar(Vars* vars)
{
  Vars* newvars = MALLOC(sizeof(Vars));
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

static Link* LookupPort(Ports* ports, PortId id)
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
//   ports - The scope to extend.
//   link - The underlying link of the port to add.
//
// Returns:
//   A new scope with the new port and the contents of the given scope.
//
// Side effects:
//   None.

static Ports* AddPort(Ports* ports, Link* link)
{
  Ports* nports = MALLOC(sizeof(Ports));
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
//   expr - The expression for the created command to evaluate. This must not
//          be NULL.
//   target - The target of the result of evaluation. This must not be NULL.
//   next - The command to run after this command.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkExprCmd(Expr* expr, Value** target, Cmd* next)
{
  assert(expr != NULL);
  assert(target != NULL);

  ExprCmd* cmd = MALLOC(sizeof(ExprCmd));
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
//   actn - The action for the created command to evaluate. This must not
//          be NULL.
//   target - The target of the result of evaluation.
//   next - The command to run after this command.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkActnCmd(Actn* actn, Value** target, Cmd* next)
{
  assert(actn != NULL);
  assert(target != NULL);

  ActnCmd* cmd = MALLOC(sizeof(ActnCmd));
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
//   value - The value that will be accessed.
//   field - The id of the field to access.
//   target - The target destination of the accessed field.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkAccessCmd(Value* value, size_t field, Value** target, Cmd* next)
{
  AccessCmd* cmd = MALLOC(sizeof(AccessCmd));
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
//   value - The value to condition on.
//   choices - The list of expressions to choose from based on the value.
//   target - The target destination for the final evaluated value.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkCondExprCmd(UnionValue* value, Expr** choices, Value** target, Cmd* next)
{
  CondExprCmd* cmd = MALLOC(sizeof(CondExprCmd));
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
//   value - The value to condition on.
//   choices - The list of actions to choose from based on the value.
//   target - The target destination for the final evaluated value.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkCondActnCmd(
    UnionValue* value, Actn** choices, Value** target, Cmd* next)
{
  CondActnCmd* cmd = MALLOC(sizeof(CondActnCmd));
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
//   vars - The new value of the local variable scope to use.
//   ports - The new value of the ports scope to use.
//   is_pop - True if this should be a pop scope command.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkScopeCmd(Vars* vars, Ports* ports, bool is_pop, Cmd* next)
{
  ScopeCmd* cmd = MALLOC(sizeof(ScopeCmd));
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
//   vars - The new value of the local variable scope to use.
//   ports - The new value of the ports scope to use.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkPushScopeCmd(Vars* vars, Ports* ports, Cmd* next)
{
  return MkScopeCmd(vars, ports, false, next);
}

// MkPopScopeCmd --
//   
//   Create a command to change the current ports and local variable scope
//   that frees the previous ports and local variable scope.
//
// Inputs:
//   vars - The new value of the local variable scope to use.
//   ports - The new value of the ports scope to use.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkPopScopeCmd(Vars* vars, Ports* ports, Cmd* next)
{
  return MkScopeCmd(vars, ports, true, next);
}

// MkJoinCmd --
//
//   Create a join command.
//
// Inputs:
//   count - The number of threads to join.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkJoinCmd(size_t count, Cmd* next)
{
  JoinCmd* cmd = MALLOC(sizeof(JoinCmd));
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
//   target - The target destination for the put value.
//   link - The link to put the value on.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkPutCmd(Value** target, Link* link, Cmd* next)
{
  assert(target != NULL);
  PutCmd* cmd = MALLOC(sizeof(PutCmd));
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
//   link - The link to free.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkFreeLinkCmd(Link* link, Cmd* next)
{
  FreeLinkCmd* cmd = MALLOC(sizeof(FreeLinkCmd));
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

static void Run(Program* program, Threads* threads, Thread* thread)
{
  for (int i = 0; i < 1024 && thread->cmd != NULL; i++) {
    Cmd* next = thread->cmd->next;
    switch (thread->cmd->tag) {
      case CMD_EXPR: {
        ExprCmd* cmd = (ExprCmd*)thread->cmd;
        Expr* expr = cmd->expr;
        Value** target = cmd->target;
        switch (expr->tag) {
          case VAR_EXPR: {
            VarExpr* var_expr = (VarExpr*)expr;
            Value* value = LookupVal(thread->vars, var_expr->var);
            *target = Copy(value);
            break;
          }

          case APP_EXPR: {
            AppExpr* app_expr = (AppExpr*)expr;
            Decl* decl = program->declv[app_expr->func];
            if (decl->tag == STRUCT_DECL) {
              // Create the struct value now, then add commands to evaluate
              // the arguments to fill in the fields with the proper results.
              TypeDecl* struct_decl = (TypeDecl*)decl;
              size_t fieldc = struct_decl->fieldc;
              StructValue* value = NewStructValue(fieldc);
              *target = (Value*)value;
              for (size_t i = 0; i < fieldc; ++i) {
                next = MkExprCmd(app_expr->argv[i], &(value->fields[i]), next);
              }
              break;
            }

            if (decl->tag == FUNC_DECL) {
              // Add to the top of the command list:
              //  arg -> ... -> arg -> push scope -> body -> pop scope -> next 
              // The results of the arg evaluations will be stored directly in
              // the vars for the first scope command.
              // TODO: Avoid memory leaks in the case of tail calls.

              FuncDecl* func = (FuncDecl*)decl;
              next = MkPopScopeCmd(thread->vars, thread->ports, next);
              next = MkExprCmd(func->body, target, next);
              next = MkPushScopeCmd(NULL, thread->ports, next);

              ScopeCmd* scmd = (ScopeCmd*)next;
              Vars* nvars = NULL;
              for (size_t i = 0; i < func->argc; ++i) {
                nvars = AddVar(nvars);
                next = MkExprCmd(app_expr->argv[i], LookupRef(nvars, 0), next);
              }
              scmd->vars = nvars;
              break;
            }
            assert(false && "No such struct type or function found");
          }

          case ACCESS_EXPR: {
            // Add to the top of the command list:
            // object -> access -> ...
            AccessExpr* access_expr = (AccessExpr*)expr;
            AccessCmd* acmd = (AccessCmd*)MkAccessCmd(
                NULL, access_expr->field, target, next);
            next = MkExprCmd(access_expr->object, &(acmd->value), (Cmd*)acmd);
            break;
          }

          case UNION_EXPR: {
            // Create the union value now, then add a command to evaluate the
            // argument of the union constructor and set the field of the
            // union value.
            UnionExpr* union_expr = (UnionExpr*)expr;
            UnionValue* value = NewUnionValue();
            value->tag = union_expr->field;
            *target = (Value*)value;
            next = MkExprCmd(union_expr->body, &(value->field), next);
            break;
          }

          case LET_EXPR: {
            // Add to the top of the command list:
            // def -> push scope -> body -> pop scope -> ...
            // TODO: Avoid memory leaks for tail calls.
            LetExpr* let_expr = (LetExpr*)expr;

            next = MkPopScopeCmd(thread->vars, thread->ports, next);
            Vars* nvars = AddVar(thread->vars);
            next = MkExprCmd(let_expr->body, target, next);
            next = MkPushScopeCmd(nvars, thread->ports, next);
            next = MkExprCmd(let_expr->def, LookupRef(nvars, 0), next);
            break;
          }

          case COND_EXPR: {
            // Add to the top of the command list:
            // select -> econd -> ...
            CondExpr* cond_expr = (CondExpr*)expr;
            CondExprCmd* ccmd = (CondExprCmd*)MkCondExprCmd(
                NULL, cond_expr->argv, target, next);
            next = MkExprCmd(
                cond_expr->select, (Value**)&(ccmd->value), (Cmd*)ccmd);
            break;
          }
        }
        break;
      }

      case CMD_ACTN: {
        ActnCmd* cmd = (ActnCmd*)thread->cmd;
        Actn* actn = cmd->actn;
        Value** target = cmd->target;
        switch (actn->tag) {
          case EVAL_ACTN: {
            EvalActn* eval_actn = (EvalActn*)actn;
            next = MkExprCmd(eval_actn->expr, target, next);
            break;
          }

          case GET_ACTN: {
            GetActn* get_actn = (GetActn*)actn;
            Link* link = LookupPort(thread->ports, get_actn->port);
            assert(link != NULL && "Get port not in scope");

            *target = GetValue(link);
            if (*target == NULL) {
              AddThread(&link->waiting, thread);
              return;
            }
            break;
          }

          case PUT_ACTN: {
            // expr -> put -> next
            PutActn* put_actn = (PutActn*)actn;
            Link* link = LookupPort(thread->ports, put_actn->port);
            assert(link != NULL && "Put port not in scope");
            PutCmd* pcmd = (PutCmd*)MkPutCmd(target, link, next);
            next = MkExprCmd(put_actn->arg, &(pcmd->value), (Cmd*)pcmd);
            break;
          }

          case CALL_ACTN: {
            CallActn* call_actn = (CallActn*)actn;
            Decl* decl = program->declv[call_actn->proc];
            assert(decl->tag == PROC_DECL);
            ProcDecl* proc = (ProcDecl*)decl;
            assert(proc->portc == call_actn->portc);
            assert(proc->argc == call_actn->argc);

            // Add to the top of the command list:
            //  arg -> ... -> arg -> push scope -> body -> pop scope -> next
            // The results of the arg evaluations will be stored directly in
            // the vars for the scope command.
            // TODO: Avoid memory leaks in the case of tail calls.

            next = MkPopScopeCmd(thread->vars, thread->ports, next);
            next = MkActnCmd(proc->body, target, next);

            Ports* nports = NULL;
            for (size_t i = 0; i < proc->portc; ++i) {
              Link* link = LookupPort(thread->ports, call_actn->portv[i]);
              assert(link != NULL && "port not found in scope");
              nports = AddPort(nports, link);
            }
            next = MkPushScopeCmd(NULL, nports, next);

            ScopeCmd* scmd = (ScopeCmd*)next;
            Vars* nvars = NULL;
            for (size_t i = 0; i < proc->argc; ++i) {
              nvars = AddVar(nvars);
              next = MkExprCmd(call_actn->argv[i], LookupRef(nvars, 0), next);
            }
            scmd->vars = nvars;
            break;
          }

          case LINK_ACTN: {
            // actn -> free link -> pop scope -> next
            // Note: This modifies the scope in place rather than insert a
            // push scope command to do that as separate command.
            // TODO: Avoid memory leaks on tail recursion?
            next = MkPopScopeCmd(thread->vars, thread->ports, next);
            LinkActn* link_actn = (LinkActn*)actn;
            Link* link = NewLink();
            thread->ports = AddPort(thread->ports, link);
            thread->ports = AddPort(thread->ports, link);
            next = MkFreeLinkCmd(link, next);
            next = MkActnCmd(link_actn->body, target, next);
            break;
          }

          case EXEC_ACTN: {
            // Create new threads for each action to execute.
            // actn .>
            // actn ..> join -> push scope -> actn -> pop scope -> next
            // actn .>
            ExecActn* exec_actn = (ExecActn*)actn;
            next = MkPopScopeCmd(thread->vars, thread->ports, next);
            next = MkActnCmd(exec_actn->body, target, next);
            ScopeCmd* scmd = (ScopeCmd*)MkPushScopeCmd(
                NULL, thread->ports, next);
            Cmd* jcmd = MkJoinCmd(exec_actn->execc, (Cmd*)scmd);

            Vars* nvars = thread->vars;
            for (size_t i = 0; i < exec_actn->execc; ++i) {
              nvars = AddVar(nvars);
              Value** target = LookupRef(nvars, 0);
              AddThread(threads, NewThread(thread->vars, thread->ports,
                    MkActnCmd(exec_actn->execv[i], target, jcmd)));
            }
            scmd->vars = nvars;
            next = NULL;
            break;
          }

          case COND_ACTN: {
            // Add to the top of the command list:
            // select -> acond -> ...
            CondActn* cond_actn = (CondActn*)actn;
            CondActnCmd* ccmd = (CondActnCmd*)MkCondActnCmd(
                NULL, cond_actn->argv, target, next);
            next = MkExprCmd(
                cond_actn->select, (Value**)&(ccmd->value), (Cmd*)ccmd);
            break;
          }
            break;
        }
        break;
      }               

      case CMD_ACCESS: {
        AccessCmd* cmd = (AccessCmd*)thread->cmd;
        Value** target = cmd->target;
        if (cmd->value->kind == UNION_KIND) {
          UnionValue* union_value = (UnionValue*)cmd->value;
          if (union_value->tag == cmd->field) {
            *target = Copy(union_value->field);
          } else {
            fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
            abort();
          }
        } else {
          assert(cmd->field < cmd->value->kind);
          Value* value = ((StructValue*)cmd->value)->fields[cmd->field];
          *target = Copy(value);
        }
        Release(cmd->value);
        break;
      }

      case CMD_COND_EXPR: {
        CondExprCmd* cmd = (CondExprCmd*)thread->cmd;
        UnionValue* value = cmd->value;
        Value** target = cmd->target;
        assert(value->kind == UNION_KIND);
        next = MkExprCmd(cmd->choices[value->tag], target, next);
        Release((Value*)value);
        break;
      }

      case CMD_COND_ACTN: {
        CondActnCmd* cmd = (CondActnCmd*)thread->cmd;
        UnionValue* value = cmd->value;
        Value** target = cmd->target;
        assert(value->kind == UNION_KIND);
        next = MkActnCmd(cmd->choices[value->tag], target, next);
        Release((Value*)value);
        break;
      }

      case CMD_SCOPE: {
        ScopeCmd* cmd = (ScopeCmd*)thread->cmd;

        if (cmd->is_pop) {
          while (thread->vars != NULL && thread->vars != cmd->vars) {
            Vars* vars = thread->vars;
            thread->vars = vars->next;
            Release(vars->value);
            FREE(vars);
          }

          while (thread->ports != NULL && thread->ports != cmd->ports) {
            Ports* ports = thread->ports;
            thread->ports = ports->next;
            FREE(ports);
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
          FreeThread(thread);
          return;
        }
        break;
      }

      case CMD_PUT: {
        PutCmd* cmd = (PutCmd*)thread->cmd;
        Link* link = cmd->link;
        Value** target = cmd->target;
        *target = cmd->value;
        PutValue(link, Copy(cmd->value));
        Thread* waiting = GetThread(&link->waiting);
        if (waiting != NULL) {
          AddThread(threads, waiting);
        }
        break;
      }

      case CMD_FREE_LINK: {
        FreeLinkCmd* cmd = (FreeLinkCmd*)thread->cmd;
        FreeLink(cmd->link);
        break;
      }
    }

    FREE(thread->cmd);
    thread->cmd = next;
  }

  if (thread->cmd == NULL) {
    FreeThread(thread);
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

Value* Execute(Program* program, ProcDecl* proc, Value** args)
{
  Vars* vars = NULL;
  for (size_t i = 0; i < proc->argc; ++i) {
    vars = AddVar(vars);
    *LookupRef(vars, 0) = args[i];
  }

  Value* result = NULL;
  Cmd* cmd = MkPopScopeCmd(NULL, NULL, NULL);
  cmd = MkActnCmd(proc->body, &result, cmd);

  Link* links[proc->portc];
  Ports* ports = NULL;
  for (size_t i = 0; i < proc->portc; ++i) {
    links[i] = NewLink();
    ports = AddPort(ports, links[i]);
  }

  Threads threads;
  threads.head = NULL;
  threads.tail = NULL;
  AddThread(&threads, NewThread(vars, ports, cmd));

  Thread* thread = GetThread(&threads);
  while (thread != NULL) {
    // Run the current thread.
    Run(program, &threads, thread);

    // Perform whatever IO is ready. Do this after running the current thread
    // to ensure we get whatever final IO there is before terminating.
    for (size_t i = 0; i < proc->portc; ++i) {
      if (proc->portv[i].polarity == GET_POLARITY) {
        Thread* waiting = GetThread(&(links[i]->waiting));
        if (waiting != NULL) {
          assert(false && "TODO: read from external port");
//          FblcValue* got = portios[i].io(portios[i].user, NULL);
//          if (got == NULL) {
//            AddThread(&(links[i]->waiting), waiting);
//          } else {
//            PutValue(links[i], got);
//            AddThread(&threads, waiting);
//          }
        }
      } else {
        Value* put = GetValue(links[i]);
        if (put != NULL) {
          OutputBitStream stream;
          OpenBinaryOutputBitStream(&stream, 3+i);
          EncodeValue(&stream, program, proc->portv[i].type, put);
          Release(put);
        }
      }
    }

    thread = GetThread(&threads);
  }

  for (int i = 0; i < proc->portc; i++) {
    FreeLink(links[i]);
  }
  return result;
}
