/**
 * @file profile.c
 *  Fble profiling and reporting.
 */

#include <assert.h>   // for assert
#include <inttypes.h> // for PRIu64
#include <stdbool.h>  // for bool
#include <string.h>   // for strcmp

#include <fble/fble-alloc.h>
#include <fble/fble-name.h>
#include <fble/fble-profile.h>
#include <fble/fble-vector.h>

// Forward reference for ProfileNode.
typedef struct ProfileNode ProfileNode;

/**
 * @struct[ProfileNodeV] A vector of ProfileNodes.
 *  @field[size_t][size] Number of elements.
 *  @field[ProfileNode**][xs] Elements.
 */
typedef struct {
  size_t size;
  ProfileNode** xs;
} ProfileNodeV;

/**
 * @struct[ProfileNode] A node in the profile graph.
 *  @field[FbleBlockId][id] The block id associated with this node.
 *  @field[uint64_t][count]
 *   Number of times the trace from the root to this node was entered.
 *  @field[uint64_t][time]
 *   Amount of time spent in the trace from the root to this node.
 *  @field[ProfileNode*][parent] The parent of this node.
 *  @field[uint64_t][depth]
 *   The depth of this node in the tree. You can use this to determine which
 *   children are back-edges that should be skipped during traversal.
 *  @field[ProfileNodeV][children]
 *   List of child blocks called from here. Note that some of these may be
 *   back edges. Sorted in id order to facilitate binary search. This node
 *   owns all children with depth greater than this node.
 */
struct ProfileNode {
  FbleBlockId id;
  uint64_t count;
  uint64_t time;

  struct ProfileNode* parent;
  size_t depth;

  ProfileNodeV children;
};

/**
 * @struct[Profile] Internal implementation of FbleProfile.
 *  @field[FbleProfile][_base] The base FbleProfile instance.
 *  @field[ProfileNode*][root] The root of the profiling tree.
 */
typedef struct {
  FbleProfile _base;
  ProfileNode* root;
} Profile;

const FbleBlockId RootBlockId = 0;

/**
 * @struct[Stack] The call stack.
 *  Each element on the call stack is a pointer into the profile graph
 *  representing the current compacted trace.
 *
 *  @field[size_t][capacity]
 *   Number of elements stack space has been allocated for.
 *  @field[size_t][size]
 *   Number of elements actually on the stack.
 *  @field[ProfileNode**][xs] Elements on the stack.
 */
typedef struct {
  size_t capacity;
  size_t size;
  ProfileNode** xs;
} Stack;

/**
 * @struct[FbleProfileThread] Profiling state for a thread of execution.
 *  @field[Stack][stack] The current call stack.
 */
struct FbleProfileThread {
  Stack stack;
};

static void FreeNode(ProfileNode* node);

static void Push(FbleProfileThread* thread, ProfileNode* node, bool replace);
static void Pop(FbleProfileThread* thread);

static ProfileNode* Canonical(ProfileNode* node, FbleBlockId block);
static void EnterBlock(FbleProfileThread* thread, FbleBlockId block, bool replace);

static void QuerySequences(FbleProfile* profile, FbleProfileQuery* query, void* data, FbleBlockIdV* prefix, ProfileNode* node);

/**
 * @func[FreeNode] Frees resources associated with a profile node.
 *  Including any children nodes it has ownership of.
 *
 *  @arg[ProfileNode*][node] The node to free.
 *  @sideeffects
 *   Frees resources associated with the node. The node must not be accessed
 *   after this call.
 */
static void FreeNode(ProfileNode* node)
{
  for (size_t i = 0; i < node->children.size; ++i) {
    if (node->children.xs[i]->depth > node->depth) {
      FreeNode(node->children.xs[i]);
    }
  }
  FbleFreeVector(node->children);
  FbleFree(node);
}

/**
 * @func[Push] Push a node onto the call stack for the thread.
 *  @arg[FbleProfileThread*][thread]
 *   The thread whose call stack to push a value on.
 *  @arg[ProfileNode*][node] The node to push.
 *  @arg[bool][replace] If true, replace the top node instead of pushing.
 *
 *  @sideeffects
 *   Pushes a value on the stack that should be freed using Pop when no longer
 *   needed.
 */
static void Push(FbleProfileThread* thread, ProfileNode* node, bool replace)
{
  if (replace) {
    thread->stack.xs[thread->stack.size - 1] = node;
    return;
  }

  if (thread->stack.size == thread->stack.capacity) {
    thread->stack.capacity *= 2;
    thread->stack.xs = FbleReAllocArray(ProfileNode*, thread->stack.xs, thread->stack.capacity);
  }

  thread->stack.xs[thread->stack.size] = node;
  thread->stack.size++;
}

/**
 * @func[Pop] Pops an entry off the stack for the profile thead.
 *  @arg[FbleProfileThread*][thread] The thread whose call stack to pop.
 *  @sideeffects Pops a value on the stack that.
 */
static void Pop(FbleProfileThread* thread)
{
  assert(thread->stack.size > 0);
  thread->stack.size--;
}

/**
 * @func[Canonical] Check for a canonicalized destination.
 *  See if we can compact the sequence formed from the root to @a[node] with
 *  @[block] added to it.
 *
 *  @arg[ProfileNode*][node] The prefix of the sequence to try to compact.
 *  @arg[FbleBlockId][block] The suffix of the sequence to try to compact.
 *  @returns[ProfileNode*]
 *   The compacted destination node, or NULL if the sequence can't be
 *   compacted.
 *  @sideeffects None
 */
static ProfileNode* Canonical(ProfileNode* node, FbleBlockId block)
{
  for (ProfileNode* candidate = node; candidate != NULL; candidate = candidate->parent) {
    if (candidate->id != block) {
      continue;
    }

    size_t length = node->depth - candidate->depth;
    if (length > candidate->depth) {
      return NULL;
    }

    ProfileNode* s = node;
    ProfileNode* t = candidate->parent;
    while (length > 0) {
      if (s->id != t->id) {
        break;
      }

      s = s->parent;
      t = t->parent;
      length--;
    }

    if (length == 0) {
      return candidate;
    }
  }
  return NULL;
}

/**
 * @func[EnterBlock] Enters a block on the given profile thread.
 *  @arg[FbleProfileThread*][thread] The thread to do the call on.
 *  @arg[FbleBlockId][block] The block to call into.
 *  @arg[bool][replace]
 *   If true, replace the current block with the new block being entered
 *   instead of calling into the new block.
 *
 *  @sideeffects
 *   A corresponding call to FbleProfileExitBlock or FbleProfileReplaceBlock
 *   should be made when the call leaves, for proper accounting and resource
 *   management.
 */
static void EnterBlock(FbleProfileThread* thread, FbleBlockId block, bool replace)
{
  ProfileNode* node = thread->stack.xs[thread->stack.size - 1];

  // Binary search to see if the destination node already exists in the
  // profile.
  size_t lo = 0;
  size_t hi = node->children.size;
  ProfileNode** xs = node->children.xs;
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    FbleBlockId here = xs[mid]->id;
    if (here < block) {
      lo = mid + 1;
    } else if (here == block) {
      ProfileNode* dest = xs[mid];
      dest->count++;
      Push(thread, dest, replace);
      return;
    } else {
      hi = mid;
    }
  }

  // We didn't find the node as an immediate child, check to see if we could
  // compact at this point and find the destination up the tree.
  ProfileNode* dest = Canonical(node, block);
  if (dest == NULL) {
    dest = FbleAlloc(ProfileNode);
    dest->id = block;
    dest->count = 0;
    dest->time = 0;
    dest->parent = node;
    dest->depth = node->depth + 1;
    FbleInitVector(dest->children);
  }

  // Insert into the children list, preserving the sort order.
  FbleAppendToVector(node->children, NULL);
  for (size_t i = node->children.size - 1; i > lo; --i) {
    node->children.xs[i] = node->children.xs[i-1];
  }
  node->children.xs[lo] = dest;

  dest->count++;
  Push(thread, dest, replace);
}

/**
 * @func[QuerySequences] Runs a query over call sequences.
 *  @arg[FbleProfile*][profile] The profile being queried.
 *  @arg[FbleProfileQuery*][query] The query to run.
 *  @arg[void*][data] User data.
 *  @arg[FbleBlockIdV*][prefix] Prefix to the sequence in the node.
 *  @arg[ProfileNode*][node] Profile tree node to query sequences for.
 *
 *  @sideeffects
 *   @i Calls query function on sample and children.
 *   @i May temporarily expand prefix, causing it to be resized.
 */
static void QuerySequences(FbleProfile* profile, FbleProfileQuery* query, void* data, FbleBlockIdV* prefix, ProfileNode* node)
{
  FbleAppendToVector(*prefix, node->id);
  query(profile, data, *prefix, node->count, node->time);

  for (size_t i = 0; i < node->children.size; ++i) {
    if (node->children.xs[i]->depth > node->depth) {
      QuerySequences(profile, query, data, prefix, node->children.xs[i]);
    }
  }

  prefix->size--;
}

// See documentation in fble-profile.h.
FbleProfile* FbleNewProfile(bool enabled)
{
  Profile* profile = FbleAlloc(Profile);

  profile->root = FbleAlloc(ProfileNode);
  profile->root->id = RootBlockId;
  profile->root->count = 0;
  profile->root->time = 0;
  profile->root->parent = NULL;
  profile->root->depth = 0;
  FbleInitVector(profile->root->children);

  FbleInitVector(profile->_base.blocks);
  profile->_base.enabled = enabled;

  FbleName root = {
    .name = FbleNewString("[root]"),
    .space = FBLE_NORMAL_NAME_SPACE,
    .loc = { .source = FbleNewString(__FILE__), .line = __LINE__, .col = 0 }
  };
  FbleBlockId root_id = FbleAddBlockToProfile(&profile->_base, root);
  assert(root_id == RootBlockId);

  return &profile->_base;
}

// See documentation in fble-profile.h
void FbleEnableProfiling(FbleProfile* profile)
{
  profile->enabled = true;
}

// See documentation in fble-profile.h
void FbleDisableProfiling(FbleProfile* profile)
{
  profile->enabled = false;
}

// See documentation in fble-profile.h.
FbleBlockId FbleAddBlockToProfile(FbleProfile* profile, FbleName name)
{
  FbleBlockId id = profile->blocks.size;
  FbleAppendToVector(profile->blocks, name);
  return id;
}
// See documentation in fble-profile.h.
FbleBlockId FbleAddBlocksToProfile(FbleProfile* profile, FbleNameV names)
{
  size_t id = profile->blocks.size;
  for (size_t i = 0; i < names.size; ++i) {
    FbleAddBlockToProfile(profile, FbleCopyName(names.xs[i]));
  }
  return id;
}

// See documentation in fble-profile.h.
void FbleFreeProfile(FbleProfile* profile_)
{
  Profile* profile = (Profile*)profile_;

  FreeNode(profile->root);
  for (size_t i = 0; i < profile->_base.blocks.size; ++i) {
    FbleFreeName(profile->_base.blocks.xs[i]);
  }
  FbleFreeVector(profile->_base.blocks);
  FbleFree(profile);
}

// See documentation in fble-profile.h.
FbleProfileThread* FbleNewProfileThread(FbleProfile* profile_)
{
  if (!profile_->enabled) {
    return NULL;
  }

  FbleProfileThread* thread = FbleAlloc(FbleProfileThread);
  
  thread->stack.capacity = 8;
  thread->stack.size = 0;
  thread->stack.xs = FbleAllocArray(ProfileNode*, thread->stack.capacity);

  Profile* profile = (Profile*)profile_;

  Push(thread, profile->root, false);
  profile->root->count++;
  return thread;
}

// See documentation in fble-profile.h.
void FbleFreeProfileThread(FbleProfileThread* thread)
{
  if (thread == NULL) {
    return;
  }

  FbleFree(thread->stack.xs);
  FbleFree(thread);
}

// See documentation in fble-profile.h.
void FbleProfileSample(FbleProfileThread* thread, uint64_t time)
{
  if (thread == NULL) {
    return;
  }

  thread->stack.xs[thread->stack.size - 1]->time += time;
}

// See documentation in fble-profile.h.
void FbleProfileEnterBlock(FbleProfileThread* thread, FbleBlockId block)
{
  if (thread == NULL) {
    return;
  }
  EnterBlock(thread, block, false);
}

// See documentation in fble-profile.h.
void FbleProfileReplaceBlock(FbleProfileThread* thread, FbleBlockId block)
{
  if (thread == NULL) {
    return;
  }
  EnterBlock(thread, block, true);
}

// See documentation in fble-profile.h.
void FbleProfileExitBlock(FbleProfileThread* thread)
{
  if (thread == NULL) {
    return;
  }

  Pop(thread);
}

// See documentation in fble-profile.h
FbleName* FbleProfileBlockName(FbleProfile* profile, FbleBlockId id)
{
  if (id < profile->blocks.size) {
    return profile->blocks.xs + id;
  }
  return NULL;
}

// See documentation in fble-profile.h
FbleBlockId FbleLookupProfileBlockId(FbleProfile* profile, const char* name)
{
  for (size_t i = 0; i < profile->blocks.size; ++i) {
    if (strcmp(name, profile->blocks.xs[i].name->str) == 0) {
      return i;
    }
  }
  return 0;
}

// See documentation in fble-profile.h
void FbleQueryProfile(FbleProfile* profile_, FbleProfileQuery* query, void* userdata)
{
  if (!profile_->enabled) {
    return;
  }

  Profile* profile = (Profile*)profile_;

  FbleBlockIdV prefix;
  FbleInitVector(prefix);
  QuerySequences(profile_, query, userdata, &prefix, profile->root);
  FbleFreeVector(prefix);
}
