// FblcEvaluator.c --
//
//   This file implements routines for evaluating Fblc expressions.

#include "FblcInternal.h"

typedef struct Vars Vars;
typedef struct Ports Ports;
typedef struct Cmd Cmd;
typedef struct Thread Thread;

// Singly-linked list of threads.

struct Thread {
  Vars* vars;
  Ports* ports;
  Cmd* cmd;
  Thread* next;
};

// List of threads with head and tail pointer. Threads are added to the tail
// and removed from the head.

typedef struct Threads {
  Thread* head;
  Thread* tail;
} Threads;

// Singly-linked list of values.

typedef struct Values {
  FblcValue* value;
  struct Values* next;
} Values;

// A Link is a FIFO list of values. Values are put to the tail of the list and
// taken from the head of the list.
// The empty list is represented with head and tail both set to NULL.
// waiting is a list of threads waiting to get values from the link.

typedef struct Link {
  Values* head;
  Values* tail;
  struct Threads waiting;
} Link;

// The following defines a Vars structure for storing the value of local
// variables. It is possible to extend a local variable scope without
// modifying the original local variable scope, and it is possible to access
// the address of a value stored in the variable scope. Typical usage is to
// extend the variable scope by reserving slots for values that are yet to be
// computed, and to ensure the values are computed and updated by the time the
// scope is used.

struct Vars {
  FblcName name;
  FblcValue* value;
  struct Vars* next;
};

// Map from port name to link.

struct Ports {
  FblcName name;
  Link* link;
  struct Ports* next;
};

// The evaluator works by breaking down action and expression evaluation into
// a sequence of commands that can be executed in turn. All of the state of
// evaluation, including the stack, is stored explicitly in the command list.
// By storing the stack explicitly instead of piggy-backing off of the C
// runtime stack, we are able to implement the evaluator as a single while
// loop and avoid problems with supporting tail recursive Fblc programs.

// The following CmdTag enum identifies the types of commands used to evaluate
// actions and expressions. The Cmd struct represents a command list. See the
// comments on the extra data in the Cmd structure for the specification of
// the commands.
// The state of the stack is captured in a command list by storing variables
// in 'vars' commands.

typedef enum {
  CMD_EXPR, CMD_ACTN, CMD_ACCESS, CMD_COND_EXPR, CMD_COND_ACTN,
  CMD_SCOPE, CMD_JOIN, CMD_PUT,
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
  const FblcExpr* expr;
  FblcValue** target;
} ExprCmd;

// CMD_ACTN: The actn command executes actn and stores the resulting value, if
// any, in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcActn* actn;
  FblcValue** target;
} ActnCmd;

// CMD_ACCESS: The access command accesses the given field of the given value
// and stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcValue* value;
  FblcName field;
  FblcValue** target;
} AccessCmd;

// CMD_COND_EXPR: The condition expression command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen expression and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcUnionValue* value;
  FblcExpr* const* choices;
  FblcValue** target;
} CondExprCmd;

// CMD_COND_ACTN: The conditional action command uses the tag of 'value' to
// select the choice to evaluate. It then evaluates the chosen action and
// stores the resulting value in *target.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  FblcUnionValue* value;
  FblcActn** choices;
  FblcValue** target;
} CondActnCmd;

// CMD_SCOPE: The scope command sets the current ports and vars to the given
// ports and vars.
typedef struct {
  CmdTag tag;
  struct Cmd* next;
  Vars* vars;
  Ports* ports;
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

static FblcValue** LookupRef(Vars* vars, FblcName name);
static FblcValue* LookupVal(Vars* vars, FblcName name);
static Vars* AddVar(Vars* vars, FblcName name);

static Link* LookupPort(Ports* ports, FblcName name);
static Ports* AddPort(Ports* ports, FblcName name, Link* link);

static Thread* NewThread(Vars* vars, Ports* ports, Cmd* cmd);
static void FreeThread(Thread* thread);
static void AddThread(Threads* threads, Thread* thread);
static Thread* GetThread(Threads* threads);

static Cmd* MkExprCmd(const FblcExpr* expr, FblcValue** target, Cmd* next);
static Cmd* MkActnCmd(FblcActn* actn, FblcValue** target, Cmd* next);
static Cmd* MkAccessCmd(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next);
static Cmd* MkCondExprCmd(
    FblcUnionValue* value, FblcExpr* const* choices, FblcValue** target,
    Cmd* next);
static Cmd* MkCondActnCmd(
    FblcUnionValue* value, FblcActn** choices, FblcValue** target, Cmd* next);
static Cmd* MkScopeCmd(Vars* vars, Ports* ports, Cmd* next);
static Cmd* MkJoinCmd(int count, Cmd* next);
static int TagForField(const FblcType* type, FblcName field);
static void Run(const FblcEnv* env, Threads* threads, Thread* thread);

// LookupRef --
//
//   Look up a reference to the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable reference.
//   name - The name of the variable to lookup.
//
// Returns:
//   A pointer to the value of the variable with the given name in scope, or
//   NULL if no such variable or value was found in scope.
//
// Side effects:
//   None.

static FblcValue** LookupRef(Vars* vars, FblcName name)
{
  for ( ; vars != NULL; vars = vars->next) {
    if (FblcNamesEqual(vars->name, name)) {
      return &(vars->value);
    }
  }
  return NULL;
}

// LookupVal --
//
//   Look up the value of a variable in the given scope.
//
// Inputs:
//   vars - The scope to look in for the variable value.
//   name - The name of the variable to lookup.
//
// Returns:
//   The value of the variable with the given name in scope, or NULL if no
//   such variable or value was found in scope.
//
// Side effects:
//   None.

static FblcValue* LookupVal(Vars* vars, FblcName name)
{
  FblcValue** ref = LookupRef(vars, name);
  return ref == NULL ? NULL : *ref;
}

// AddVar --
//   
//   Extend the given scope with a new variable. Use LookupRef to access the
//   newly added variable.
//
// Inputs:
//   vars - The scope to extend.
//   name - The name of the variable to add.
//
// Returns:
//   A new scope with the new variable and the contents of the given scope.
//
// Side effects:
//   None.

static Vars* AddVar(Vars* vars, FblcName name)
{
  Vars* newvars = GC_MALLOC(sizeof(Vars));
  newvars->name = name;
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
//   name - The name of the port to look up.
//
// Returns:
//   The link associated with the given port, or NULL if no such port was
//   found.
//
// Side effects:
//   None.

static Link* LookupPort(Ports* ports, FblcName name)
{
  for ( ; ports != NULL; ports = ports->next) {
    if (FblcNamesEqual(ports->name, name)) {
      return ports->link;
    }
  }
  return NULL;
}

// AddPort --
//   
//   Extend the given port scope with a new port.
//
// Inputs:
//   ports - The scope to extend.
//   name - The name of the port to add.
//   link - The underlying link of the port to add.
//
// Returns:
//   A new scope with the new port and the contents of the given scope.
//
// Side effects:
//   None.

static Ports* AddPort(Ports* ports, FblcName name, Link* link)
{
  Ports* nports = GC_MALLOC(sizeof(Ports));
  nports->name = name;
  nports->link = link;
  nports->next = ports;
  return nports;
}

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
  Thread* thread = GC_MALLOC(sizeof(Thread));
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
  GC_FREE(thread);
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

static Cmd* MkExprCmd(const FblcExpr* expr, FblcValue** target, Cmd* next)
{
  assert(expr != NULL);
  assert(target != NULL);

  ExprCmd* cmd = GC_MALLOC(sizeof(ExprCmd));
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

static Cmd* MkActnCmd(FblcActn* actn, FblcValue** target, Cmd* next)
{
  assert(actn != NULL);
  assert(target != NULL);

  ActnCmd* cmd = GC_MALLOC(sizeof(ActnCmd));
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
//   field - The name of the field to access.
//   target - The target destination of the accessed field.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkAccessCmd(
    FblcValue* value, FblcName field, FblcValue** target, Cmd* next)
{
  AccessCmd* cmd = GC_MALLOC(sizeof(AccessCmd));
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

static Cmd* MkCondExprCmd(
    FblcUnionValue* value, FblcExpr* const* choices, FblcValue** target,
    Cmd* next)
{
  CondExprCmd* cmd = GC_MALLOC(sizeof(CondExprCmd));
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
    FblcUnionValue* value, FblcActn** choices, FblcValue** target, Cmd* next)
{
  CondActnCmd* cmd = GC_MALLOC(sizeof(CondActnCmd));
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
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkScopeCmd(Vars* vars, Ports* ports, Cmd* next)
{
  ScopeCmd* cmd = GC_MALLOC(sizeof(ScopeCmd));
  cmd->tag = CMD_SCOPE;
  cmd->next = next;
  cmd->vars = vars;
  cmd->ports = ports;
  return (Cmd*)cmd;
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

static Cmd* MkJoinCmd(int count, Cmd* next)
{
  JoinCmd* cmd = GC_MALLOC(sizeof(JoinCmd));
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
//   link - The link to put the value on.
//   next - The command to run after this one.
//
// Returns:
//   The newly created command.
//
// Side effects:
//   None.

static Cmd* MkPutCmd(FblcValue** target, Link* link, Cmd* next)
{
  assert(target != NULL);
  PutCmd* cmd = GC_MALLOC(sizeof(PutCmd));
  cmd->tag = CMD_PUT;
  cmd->next = next;
  cmd->target = target;
  cmd->link = link;
  cmd->value = NULL;
  return (Cmd*)cmd;
}

// TagForField --
//
//   Return the tag corresponding to the named field for the given type.
//   It is a program error if the named field does not exist for the given
//   type.
//
// Inputs:
//   type - The type in question.
//   field - The field to get the tag for.
//
// Result:
//   The index in the list of fields for the type containing that field name.
//
// Side effects:
//   None.

static int TagForField(const FblcType* type, FblcName field)
{
  for (int i = 0; i < type->fieldc; i++) {
    if (FblcNamesEqual(field, type->fieldv[i].name.name)) {
      return i;
    }
  }
  assert(false && "No such field.");
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
// Side Effects:
//   Threads are added to the threads list based on the commands executed for
//   the thread. If the thread is not executed to completion, it will be added
//   back to the threads list representing the continuation of this thread.

static void Run(const FblcEnv* env, Threads* threads, Thread* thread)
{
  for (int i = 0; i < 1024 && thread->cmd != NULL; i++) {
    Cmd* next = thread->cmd->next;
    switch (thread->cmd->tag) {
      case CMD_EXPR: {
        ExprCmd* cmd = (ExprCmd*)thread->cmd;
        const FblcExpr* expr = cmd->expr;
        FblcValue** target = cmd->target;
        switch (expr->tag) {
          case FBLC_VAR_EXPR: {
            FblcVarExpr* var_expr = (FblcVarExpr*)expr;
            FblcName var_name = var_expr->name.name;
            *target = FblcCopy(LookupVal(thread->vars, var_name));
            assert(*target != NULL && "Var not in scope");
            break;
          }

          case FBLC_APP_EXPR: {
            FblcAppExpr* app_expr = (FblcAppExpr*)expr;
            FblcType* type = FblcLookupType(env, app_expr->func.name);
            if (type != NULL) {
              if (type->kind == FBLC_KIND_STRUCT) {
                // Create the struct value now, then add commands to evaluate
                // the arguments to fill in the fields with the proper results.
                int fieldc = type->fieldc;
                FblcStructValue* value = GC_MALLOC(sizeof(FblcStructValue));
                value->refcount = 1;
                value->fieldv = GC_MALLOC(fieldc * sizeof(FblcValue*));
                value->type = type;
                *target = (FblcValue*)value;
                for (int i = 0; i < fieldc; i++) {
                  next = MkExprCmd(
                      app_expr->argv[i], &(value->fieldv[i]), next);
                }
              } else {
                assert(false && "Invalid kind of type for application");
              }
              break;
            }

            FblcFunc* func = FblcLookupFunc(env, app_expr->func.name);
            if (func != NULL) {
              // Add to the top of the command list:
              // arg -> ... -> arg -> scope -> body -> (scope) -> ...
              // The results of the arg evaluations will be stored directly in
              // the vars for the scope command.

              // Don't put a scope change if we will immediately 
              // change it back to a different scope. This is important to
              // avoid memory leaks for tail calls.
              if (next != NULL && next->tag != CMD_SCOPE) {
                next = MkScopeCmd(thread->vars, thread->ports, next);
              }

              next = MkExprCmd(func->body, target, next);
              next = MkScopeCmd(NULL, thread->ports, next);

              ScopeCmd* scmd = (ScopeCmd*)next;
              Vars* nvars = NULL;
              for (int i = 0; i < func->argc; i++) {
                FblcName var_name = func->argv[i].name.name;
                nvars = AddVar(nvars, var_name);
                next = MkExprCmd(
                    app_expr->argv[i], LookupRef(nvars, var_name), next);
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
            AccessCmd* acmd = (AccessCmd*)MkAccessCmd(
                NULL, access_expr->field.name, target, next);
            next = MkExprCmd(access_expr->object, &(acmd->value), (Cmd*)acmd);
            break;
          }

          case FBLC_UNION_EXPR: {
            // Create the union value now, then add a command to evaluate the
            // argument of the union constructor and set the field of the
            // union value.
            FblcUnionExpr* union_expr = (FblcUnionExpr*)expr;
            FblcType* type = FblcLookupType(env, union_expr->type.name);
            assert(type != NULL);
            FblcUnionValue* value = GC_MALLOC(sizeof(FblcUnionValue));
            value->refcount = 1;
            value->type = type;
            value->tag = TagForField(type, union_expr->field.name);
            *target = (FblcValue*)value;
            next = MkExprCmd(union_expr->value, &(value->field), next);
            break;
          }

          case FBLC_LET_EXPR: {
            // Add to the top of the command list:
            // def -> var -> body -> (vars) -> ...
            FblcLetExpr* let_expr = (FblcLetExpr*)expr;

            // No need to reset the scope if we are going to switch to
            // different scope immediately after anyway.
            if (next != NULL && next->tag != CMD_SCOPE) {
              next = MkScopeCmd(thread->vars, thread->ports, next);
            }
            FblcName var_name = let_expr->name.name;
            Vars* nvars = AddVar(thread->vars, var_name);
            next = MkExprCmd(let_expr->body, target, next);
            next = MkScopeCmd(nvars, thread->ports, next);
            next = MkExprCmd(let_expr->def, LookupRef(nvars, var_name), next);
            break;
          }

          case FBLC_COND_EXPR: {
            // Add to the top of the command list:
            // select -> econd -> ...
            FblcCondExpr* cond_expr = (FblcCondExpr*)expr;
            CondExprCmd* ccmd = (CondExprCmd*)MkCondExprCmd(
                NULL, cond_expr->argv, target, next);
            next = MkExprCmd(
                cond_expr->select, (FblcValue**)&(ccmd->value), (Cmd*)ccmd);
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
            next = MkExprCmd(eval_actn->expr, target, next);
            break;
          }

          case FBLC_GET_ACTN: {
            FblcGetActn* get_actn = (FblcGetActn*)actn;
            Link* link = LookupPort(thread->ports, get_actn->port.name);
            assert(link != NULL && "Get port not in scope");

            if (link->head == NULL) {
              AddThread(&link->waiting, thread);
              return;
            } else {
              *target = link->head->value;
              link->head = link->head->next;
              if (link->head == NULL) {
                link->tail = NULL;
              }
            }
            break;
          }

          case FBLC_PUT_ACTN: {
            // expr -> put -> next
            FblcPutActn* put_actn = (FblcPutActn*)actn;
            Link* link = LookupPort(thread->ports, put_actn->port.name);
            assert(link != NULL && "Put port not in scope");
            PutCmd* pcmd = (PutCmd*)MkPutCmd(target, link, next);
            next = MkExprCmd(put_actn->expr, &(pcmd->value), (Cmd*)pcmd);
            break;
          }

          case FBLC_CALL_ACTN: {
            FblcCallActn* call_actn = (FblcCallActn*)actn;
            FblcProc* proc = FblcLookupProc(env, call_actn->proc.name);
            assert(proc != NULL && "No such proc found");
            assert(proc->portc == call_actn->portc);
            assert(proc->argc == call_actn->exprc);

            // Add to the top of the command list:
            // arg -> ... -> arg -> scope -> body -> (scope) -> ...
            // The results of the arg evaluations will be stored directly in
            // the vars for the scope command.

            // Don't put a scope change if we will immediately 
            // change it back to a different scope. This is important to
            // avoid memory leaks for tail calls.
            if (next != NULL && next->tag != CMD_SCOPE) {
              next = MkScopeCmd(thread->vars, thread->ports, next);
            }

            next = MkActnCmd(proc->body, target, next);

            Ports* nports = NULL;
            for (int i = 0; i < proc->portc; i++) {
              Link* link = LookupPort(thread->ports, call_actn->ports[i].name);
              assert(link != NULL && "port not found in scope");
              nports = AddPort(nports, proc->portv[i].name.name, link);
            }
            next = MkScopeCmd(NULL, nports, next);

            ScopeCmd* scmd = (ScopeCmd*)next;
            Vars* nvars = NULL;
            for (int i = 0; i < proc->argc; i++) {
              FblcName var_name = proc->argv[i].name.name;
              nvars = AddVar(nvars, var_name);
              next = MkExprCmd(
                  call_actn->exprs[i], LookupRef(nvars, var_name), next);
            }
            scmd->vars = nvars;
            break;
          }

          case FBLC_LINK_ACTN: {
            FblcLinkActn* link_actn = (FblcLinkActn*)actn;
            Link* link = GC_MALLOC(sizeof(Link));
            link->head = NULL;
            link->tail = NULL;
            link->waiting.head = NULL;
            link->waiting.tail = NULL;
            thread->ports = AddPort(
                thread->ports, link_actn->getname.name, link);
            thread->ports = AddPort(
                thread->ports, link_actn->putname.name, link);
            next = MkActnCmd(link_actn->body, target, next);
            break;
          }

          case FBLC_EXEC_ACTN: {
            // Create new threads for each action to execute.
            // actn .>
            // actn ..> join -> scope -> actn -> scope -> next
            // actn .>
            FblcExecActn* exec_actn = (FblcExecActn*)actn;
            next = MkScopeCmd(thread->vars, thread->ports, next);
            next = MkActnCmd(exec_actn->body, target, next);
            ScopeCmd* scmd = (ScopeCmd*)MkScopeCmd(NULL, thread->ports, next);
            Cmd* jcmd = MkJoinCmd(exec_actn->execc, (Cmd*)scmd);

            Vars* nvars = thread->vars;
            for (int i = 0; i < exec_actn->execc; i++) {
              FblcExec* exec = &(exec_actn->execv[i]);
              nvars = AddVar(nvars, exec->var.name.name);
              FblcValue** target = LookupRef(nvars, exec->var.name.name);
              AddThread(threads, NewThread(thread->vars, thread->ports,
                    MkActnCmd(exec->actn, target, jcmd)));
            }
            scmd->vars = nvars;
            next = NULL;
            break;
          }

          case FBLC_COND_ACTN: {
            // Add to the top of the command list:
            // select -> acond -> ...
            FblcCondActn* cond_actn = (FblcCondActn*)actn;
            CondActnCmd* ccmd = (CondActnCmd*)MkCondActnCmd(
                NULL, cond_actn->args, target, next);
            next = MkExprCmd(
                cond_actn->select, (FblcValue**)&(ccmd->value), (Cmd*)ccmd);
            break;
          }
            break;
        }
        break;
      }               

      case CMD_ACCESS: {
        AccessCmd* cmd = (AccessCmd*)thread->cmd;
        FblcType* type = cmd->value->type;
        int target_tag = TagForField(type, cmd->field);
        FblcValue** target = cmd->target;
        if (type->kind == FBLC_KIND_STRUCT) {
          *target = ((FblcStructValue*)cmd->value)->fieldv[target_tag];
        } else {
          assert(type->kind == FBLC_KIND_UNION);
          FblcUnionValue* union_value = (FblcUnionValue*)cmd->value;
          if (union_value->tag == target_tag) {
            *target = union_value->field;
          } else {
            fprintf(stderr, "MEMBER ACCESS UNDEFINED\n");
            abort();
          }
        }
        // FblcRelease(cmd->value);
        break;
      }

      case CMD_COND_EXPR: {
        CondExprCmd* cmd = (CondExprCmd*)thread->cmd;
        FblcUnionValue* value = cmd->value;
        FblcValue** target = cmd->target;
        assert(value->type->kind == FBLC_KIND_UNION);
        next = MkExprCmd(cmd->choices[value->tag], target, next);
        break;
      }

      case CMD_COND_ACTN: {
        CondActnCmd* cmd = (CondActnCmd*)thread->cmd;
        FblcUnionValue* value = cmd->value;
        FblcValue** target = cmd->target;
        assert(value->type->kind == FBLC_KIND_UNION);
        next = MkActnCmd(cmd->choices[value->tag], target, next);
        break;
      }

      case CMD_SCOPE: {
        ScopeCmd* cmd = (ScopeCmd*)thread->cmd;
        thread->vars = cmd->vars;
        thread->ports = cmd->ports;
        break;
      }

      case CMD_JOIN: {
        JoinCmd* cmd = (JoinCmd*)thread->cmd;
        assert(cmd->count > 0);
        cmd->count--;
        if (cmd->count != 0) {
          return;
        }
        break;
      }

      case CMD_PUT: {
        PutCmd* cmd = (PutCmd*)thread->cmd;
        Link* link = cmd->link;
        FblcValue** target = cmd->target;
        *target = cmd->value;
        assert(cmd->value != NULL);
        Values* ntail = GC_MALLOC(sizeof(Values));
        ntail->value = cmd->value;
        ntail->next = NULL;
        if (link->head == NULL) {
          assert(link->tail == NULL);
          link->head = ntail;
        } else {
          link->tail->next = ntail;
        }
        link->tail = ntail;

        Thread* waiting = GetThread(&link->waiting);
        if (waiting != NULL) {
          AddThread(threads, waiting);
        }
        break;
      }
    }

    GC_FREE(thread->cmd);
    thread->cmd = next;
  }

  if (thread->cmd == NULL) {
    FreeThread(thread);
  } else {
    AddThread(threads, thread);
  }
}

// FblcEvaluate --
//
//   Evaluate an expression under the given program environment. The program
//   and expression must be well formed.
//
// Inputs:
//   env - The program environment.
//   expr - The expression to evaluate.
//
// Returns:
//   The result of evaluating the given expression in the program environment
//   in a scope with no local variables.
//
// Side effects:
//   None.

FblcValue* FblcEvaluate(const FblcEnv* env, const FblcExpr* expr)
{
  FblcEvalActn actn;
  actn.tag = FBLC_EVAL_ACTN;
  actn.loc = expr->loc;
  actn.expr = expr;
  return FblcExecute(env, (FblcActn*)&actn);
}

// FblcExecute --
//
//   Execute an action under the given program environment. The program
//   and action must be well formed.
//
// Inputs:
//   env - The program environment.
//   actn - The action to execute.
//
// Returns:
//   The result of executing the given action in the program environment
//   in a scope with no local variables and no ports.
//
// Side effects:
//   None.

FblcValue* FblcExecute(const FblcEnv* env, FblcActn* actn)
{
  FblcValue* result = NULL;
  Threads threads;
  threads.head = NULL;
  threads.tail = NULL;

  AddThread(&threads, NewThread(NULL, NULL, MkActnCmd(actn, &result, NULL)));

  Thread* thread = GetThread(&threads);
  while (thread != NULL) {
    Run(env, &threads, thread);
    thread = GetThread(&threads);
  }
  return result;
}
