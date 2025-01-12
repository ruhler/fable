/**
 * @file sockets.fble.c
 *  Implementation of /Network/Sockets/IO/Builtin% module.
 */

#define _POSIX_C_SOURCE 200112L  // for getaddrinfo

#include <assert.h>       // for assert
#include <stdio.h>        // for perror
#include <string.h>       // for memset
#include <sys/types.h>    // for socket
#include <sys/socket.h>   // for socket
#include <netdb.h>        // for getaddrinfo
#include <unistd.h>       // for read, write, close

#include <fble/fble-alloc.h>      // for FbleFree
#include <fble/fble-function.h>
#include <fble/fble-loc.h>
#include <fble/fble-module-path.h>
#include <fble/fble-name.h>
#include <fble/fble-program.h>
#include <fble/fble-string.h>
#include <fble/fble-value.h>

#include "int.fble.h"         // for FbleNewIntValue, FbleIntValueAccess
#include "string.fble.h"      // for FbleNewStringValue, FbleStringValueAccess

static void OnFree(void* data);
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* ClientImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* AcceptImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);
static FbleValue* ServerImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args);

static FbleValue* IStream(FbleValueHeap* heap, FbleValue* sfd, FbleBlockId module_block_id);
static FbleValue* OStream(FbleValueHeap* heap, FbleValue* sfd, FbleBlockId module_block_id);
static FbleValue* Client(FbleValueHeap* heap, FbleBlockId module_block_id);
static FbleValue* Accept(FbleValueHeap* heap, FbleValue* sfd, FbleBlockId module_block_id);
static FbleValue* Server(FbleValueHeap* heap, FbleBlockId module_block_id);

// -Wpedantic doesn't like our initialization of flexible array members when
// defining static FbleString values.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static FbleString StrFile = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = __FILE__, };
static FbleString StrNetwork = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Network", };
static FbleString StrSockets = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Sockets", };
static FbleString StrIO = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "IO", };
static FbleString StrBuiltin = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "Builtin", };

static FbleString StrModuleBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Network/Sockets/IO/Builtin%", };
static FbleString StrClientBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Network/Sockets/IO/Builtin%.Client", };
static FbleString StrIStreamBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Network/Sockets/IO/Builtin%.IStream", };
static FbleString StrOStreamBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Network/Sockets/IO/Builtin%.OStream", };
static FbleString StrAcceptBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Network/Sockets/IO/Builtin%.Accept", };
static FbleString StrServerBlock = { .refcount = 1, .magic = FBLE_STRING_MAGIC, .str = "/Network/Sockets/IO/Builtin%.Server", };

#pragma GCC diagnostic pop

#define CLIENT_BLOCK_OFFSET 1
#define ISTREAM_BLOCK_OFFSET 2
#define OSTREAM_BLOCK_OFFSET 3
#define ACCEPT_BLOCK_OFFSET 4
#define SERVER_BLOCK_OFFSET 5
#define NUM_PROFILE_BLOCKS 6

static FbleName ProfileBlocks[NUM_PROFILE_BLOCKS] = {
  { .name = &StrModuleBlock, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrClientBlock, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrIStreamBlock, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrOStreamBlock, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrAcceptBlock, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrServerBlock, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
};

static FbleName PathEntries[] = {
  { .name = &StrNetwork, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrSockets, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrIO, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
  { .name = &StrBuiltin, .space = 0, .loc = { .source = &StrFile, .line = __LINE__, .col = 1 }},
};

static FbleModulePath Path = {
  .refcount = 1,
  .magic = FBLE_MODULE_PATH_MAGIC,
  .loc = { .source = &StrFile, .line = __LINE__, .col = 1 },
  .path = { .size = 4, .xs = PathEntries},
};

/**
 * @func[OnFree] OnFree function for int fd native values.
 *  @arg[void*][sfd] int fd to close.
 *  @sideeffects
 *   Closes the @a[sfd] socket.
 */
static void OnFree(void* data)
{
  int sfd = (int)(intptr_t)data;
  close(sfd);
}

/**
 * @func[IStreamImpl] FbleRunFunction to read a byte from a socket.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{IO@<Maybe@<Int@>>}.
 *
 *  @sideeffects
 *   Reads a byte from the socket.
 */
static FbleValue* IStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  int sfd = (int)(intptr_t)FbleNativeValueData(function->statics[0]);

  FbleValue* world = args[0];

  // TODO: Buffer the reads for better performance?
  unsigned char c;
  ssize_t r = read(sfd, &c, 1);

  FbleValue* ms;
  if (r == 0) {
    ms = FbleNewEnumValue(heap, 1);
  } else {
    FbleValue* v = FbleNewIntValue(heap, c);
    ms = FbleNewUnionValue(heap, 0, v);
  }

  return FbleNewStructValue_(heap, 2, world, ms);
}

/**
 * @func[OStreamImpl] FbleRunFunction to write a byte to a file.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Int@, World@) { R@<Unit@>; }}.
 */
static FbleValue* OStreamImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  int sfd = (int)(intptr_t)FbleNativeValueData(function->statics[0]);

  FbleValue* byte = args[0];
  FbleValue* world = args[1];

  // TODO: Buffer writes to improve performance?
  unsigned char c = (unsigned char)FbleIntValueAccess(byte);
  write(sfd, &c, 1);

  FbleValue* unit = FbleNewStructValue_(heap, 0);
  return FbleNewStructValue_(heap, 2, world, unit);
}

/**
 * @func[IStream] Allocates an @l{IStream@} for a socket.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][sfd] The socket to allocate the @l{IStream@} for.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Network/Sockets/IO/Builtin% block.
 *
 *  @returns[FbleValue*] An fble @l{IStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* IStream(FbleValueHeap* heap, FbleValue* sfd, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 1,
    .num_statics = 1,
    .run = &IStreamImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + ISTREAM_BLOCK_OFFSET, &sfd);
}

/**
 * @func[OStream] Allocates an @l{OStream@} for a socket.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][sfd] The socket to allocate the @l{OStream@} for.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Network/Sockets/IO/Builtin% block.
 *
 *  @returns[FbleValue*] An fble @l{OStream@} function value.
 *
 *  @sideeffects
 *   Allocates a value on the heap.
 */
static FbleValue* OStream(FbleValueHeap* heap, FbleValue* sfd, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 2,
    .num_statics = 1,
    .run = &OStreamImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + OSTREAM_BLOCK_OFFSET, &sfd);
}

/**
 * @func[ClientImpl] FbleRunFunction to do a tcp client connection.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, Int@, World@) { R@<Maybe@<IOStream@<IO@>>>; }
 */
static FbleValue* ClientImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* host = FbleStringValueAccess(args[0]);
  int64_t port = FbleIntValueAccess(args[1]);
  FbleValue* world = args[2];

  char portstr[10];
  snprintf(portstr, 10, "%ld", port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  struct addrinfo* result = NULL;
  if (getaddrinfo(host, portstr, &hints, &result) != 0) {
    // For debug. TODO: return the error message instead?
    perror("getaddrinfo");
    assert(result == NULL);
  }
  FbleFree(host);

  int sfd = -1;
  for (struct addrinfo* rp = result; sfd == -1 && rp != NULL; rp = rp->ai_next) {
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
      // For debug. TODO: return the error message instead?
      perror("socket");
      continue;
    }

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == -1) {
      sfd = -1;
      continue;
    }
  }

  freeaddrinfo(result);

  FbleValue* mios;
  if (sfd == -1) {
    mios = FbleNewEnumValue(heap, 1); // Nothing
  } else {
    FbleBlockId module_block_id = function->profile_block_id - CLIENT_BLOCK_OFFSET;
    FbleValue* sfd_value = FbleNewNativeValue(heap, (void*)(intptr_t)sfd, &OnFree);
    FbleValue* istream = IStream(heap, sfd_value, module_block_id);
    FbleValue* ostream = OStream(heap, sfd_value, module_block_id);
    FbleValue* ios = FbleNewStructValue_(heap, 2, istream, ostream);
    mios = FbleNewUnionValue(heap, 0, ios);  // Just(ios);
  }

  return FbleNewStructValue_(heap, 2, world, mios);
}

/**
 * @func[Client] Allocates the Client function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Network/Sockets/IO/Builtin% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Client(FbleValueHeap* heap, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 3,
    .num_statics = 0,
    .run = &ClientImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + CLIENT_BLOCK_OFFSET, NULL);
}

/**
 * @func[AcceptImpl] FbleRunFunction to accept a tcp connection.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (World@) { R@<IOStream@<IO@>>; }
 */
static FbleValue* AcceptImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  int sfd = (int)(intptr_t)FbleNativeValueData(function->statics[0]);
  FbleValue* world = args[0];

  int cfd = accept(sfd, NULL, NULL);
  if (cfd < 0) {
    // TODO: What to do here?
    perror("accept");
    assert(false);
  }

  FbleBlockId module_block_id = function->profile_block_id - ACCEPT_BLOCK_OFFSET;
  FbleValue* cfd_value = FbleNewNativeValue(heap, (void*)(intptr_t)cfd, &OnFree);
  FbleValue* istream = IStream(heap, cfd_value, module_block_id);
  FbleValue* ostream = OStream(heap, cfd_value, module_block_id);
  FbleValue* ios = FbleNewStructValue_(heap, 2, istream, ostream);
  return FbleNewStructValue_(heap, 2, world, ios);
}

/**
 * @func[ServerImpl] FbleRunFunction to start a tcp server.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, Int@, World@) { R@<Maybe@<Server@>>; } Server;
 */
static FbleValue* ServerImpl(
    FbleValueHeap* heap, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* host = FbleStringValueAccess(args[0]);
  int64_t port = FbleIntValueAccess(args[1]);
  FbleValue* world = args[2];

  char portstr[10];
  snprintf(portstr, 10, "%ld", port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  struct addrinfo* result = NULL;
  if (getaddrinfo(host, portstr, &hints, &result) != 0) {
    // For debug. TODO: return the error message instead?
    perror("getaddrinfo");
    assert(result == NULL);
  }
  FbleFree(host);

  int sfd = -1;
  for (struct addrinfo* rp = result; sfd == -1 && rp != NULL; rp = rp->ai_next) {
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
      // For debug. TODO: return the error message instead?
      perror("socket");
      continue;
    }

    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1) {
      sfd = -1;
      continue;
    }
  }

  freeaddrinfo(result);

  if (sfd != -1) {
    // TODO: How much backlog to use?
    if (listen(sfd, 10) < 0) {
      // TODO: Return error instead?
      perror("listen");
      close(sfd);
      sfd = -1;
    }
  }

  FbleValue* ms;
  if (sfd == -1) {
    ms = FbleNewEnumValue(heap, 1); // Nothing
  } else {
    FbleBlockId module_block_id = function->profile_block_id - SERVER_BLOCK_OFFSET;
    FbleValue* sfd_value = FbleNewNativeValue(heap, (void*)(intptr_t)sfd, &OnFree);
    FbleValue* accept = Accept(heap, sfd_value, module_block_id);
    ms = FbleNewUnionValue(heap, 0, accept);  // Just(accept);
  }

  return FbleNewStructValue_(heap, 2, world, ms);
}

/**
 * @func[Accept] Allocates the Accept function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleValue*][sfd] The socket to allocate the @l{OStream@} for.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Network/Sockets/IO/Builtin% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Accept(FbleValueHeap* heap, FbleValue* sfd, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 1,
    .num_statics = 1,
    .run = &AcceptImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + ACCEPT_BLOCK_OFFSET, &sfd);
}

/**
 * @func[Server] Allocates the Server function.
 *  @arg[FbleValueHeap*][heap] The value heap.
 *  @arg[FbleBlockId][module_block_id]
 *   The block_id of the /Network/Sockets/IO/Builtin% block.
 *  @returns[FbleValue*] The allocated function.
 *  @sideeffects Allocates an fble value.
 */
static FbleValue* Server(FbleValueHeap* heap, FbleBlockId module_block_id)
{
  FbleExecutable exe = {
    .num_args = 3,
    .num_statics = 0,
    .run = &ServerImpl,
  };
  return FbleNewFuncValue(heap, &exe, module_block_id + SERVER_BLOCK_OFFSET, NULL);
}

static FbleValue* Run(FbleValueHeap* heap, FbleProfileThread* profile, FbleFunction* function, FbleValue** args)
{
  FbleBlockId module_block_id = function->profile_block_id;

  FblePushFrame(heap);
  FbleValue* client = Client(heap, module_block_id);
  FbleValue* server = Server(heap, module_block_id);
  FbleValue* sockets = FbleNewStructValue_(heap, 2, client, server);
  return FblePopFrame(heap, sockets);
}

static FbleExecutable Executable = {
  .num_args = 0, 
  .num_statics = 0,
  .run = &Run,
};

FblePreloadedModule _Fble_2f_Network_2f_Sockets_2f_IO_2f_Builtin_25_ = {
  .path = &Path,
  .deps = { .size = 0, .xs = NULL },
  .executable = &Executable,
  .profile_blocks = { .size = NUM_PROFILE_BLOCKS, .xs = ProfileBlocks },
};
