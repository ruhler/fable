/**
 * @file sockets.fble.c
 *  Implementation of /Network/Sockets/Native% module.
 */

#define _POSIX_C_SOURCE 200112L  // for getaddrinfo

#include <assert.h>       // for assert
#include <inttypes.h>     // for PRIx64
#include <stdio.h>        // for perror
#include <string.h>       // for memset
#include <unistd.h>       // for read, write, close

#ifdef __WIN32
#include <winsock2.h>     // for socket
#include <ws2tcpip.h>     // for getaddrinfo

int Read(SOCKET sfd)
{
  char c;
  int r = recv(sfd, &c, 1, 0);
  if (r > 0) {
    return c;
  }
  return EOF;
}

void Write(SOCKET sfd, char c)
{
  send(sfd, &c, 1, 0);
}
#else
#include <sys/types.h>    // for socket
#include <sys/socket.h>   // for socket
#include <netdb.h>        // for getaddrinfo

// To simplify compatibilities with windows
#define SOCKET int
#define INVALID_SOCKET (-1)
#define closesocket close

int Read(SOCKET sfd)
{
  unsigned char c;
  ssize_t r = read(sfd, &c, 1);
  if (r > 0) {
    return c;
  }
  return EOF;
}

void Write(SOCKET sfd, char c)
{
  ssize_t written = write(sfd, &c, 1);
  (void) written;
}
#endif

#include <fble/fble-alloc.h>      // for FbleFree
#include <fble/fble-runtime.h>    // for FbleValue, etc.

#include "data.fble.h"            // for FbleNewIntValue, etc.


/**
 * @func[GetByte] FbleRunFunction to read a byte from a socket.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *    (Socket@, Unit@) { Maybe@<Int@>; }
 *
 *  @sideeffects
 *   Reads a byte from the socket.
 */
static FbleValue* GetByte(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  SOCKET sfd = (int)(intptr_t)FbleNativeValueData(args[0]);

  // TODO: Buffer the reads for better performance?
  int c = Read(sfd);

  if (c == EOF) {
    return FbleNewMaybeValue(runtime, NULL);
  }

  FbleValue* v = FbleNewIntValue(runtime, c);
  return FbleNewMaybeValue(runtime, v);
}

// /Network/Sockets/Native%.GetByte foreign function.
FbleForeign _Fble_2f_Network_2f_Sockets_2f_Native_25__2e_GetByte = {
  .path = "/Network/Sockets/Native%",
  .name = "GetByte",
  .num_args = 2,
  .max_call_args = 0,
  .run = &GetByte,
};


/**
 * @func[PutByte] FbleRunFunction to write a byte to a socket.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is @l{(Socket@, Int@, Unit@) { Unit@; }}.
 */
static FbleValue* PutByte(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  SOCKET sfd = (int)(intptr_t)FbleNativeValueData(args[0]);
  FbleValue* byte = args[1];

  // TODO: Buffer writes to improve performance?
  int64_t x = FbleIntValueAccess(byte);
  Write(sfd, (char)x);

  return FbleNewStructValue_(runtime, 0);
}

// /Network/Sockets/Native%.PutByte foreign function.
FbleForeign _Fble_2f_Network_2f_Sockets_2f_Native_25__2e_PutByte = {
  .path = "/Network/Sockets/Native%",
  .name = "PutByte",
  .num_args = 3,
  .max_call_args = 0,
  .run = &PutByte,
};

/**
 * @func[Client] FbleRunFunction to do a tcp client connection.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, Int@, Unit@) { Maybe@<Socket@>; }
 */
static FbleValue* Client(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* host = FbleStringValueAccess(args[0]);
  int port = (int)FbleIntValueAccess(args[1]);

  char portstr[10];
  snprintf(portstr, 10, "%d", port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICSERV;
  hints.ai_protocol = 0;

  struct addrinfo* result = NULL;
  int s = getaddrinfo(host, portstr, &hints, &result);
  if (s != 0) {
    // For debug. TODO: return the error message instead?
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(s));
    assert(result == NULL);
  }
  FbleFree(host);

  SOCKET sfd = INVALID_SOCKET;
  for (struct addrinfo* rp = result; sfd == INVALID_SOCKET && rp != NULL; rp = rp->ai_next) {
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == INVALID_SOCKET) {
      // For debug. TODO: return the error message instead?
      continue;
    }

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == -1) {
      sfd = INVALID_SOCKET;
      continue;
    }
  }

  freeaddrinfo(result);

  if (sfd == INVALID_SOCKET) {
    return FbleNewMaybeValue(runtime, NULL);
  }

  FbleValue* sfd_value = FbleNewNativeValue(runtime, (void*)(intptr_t)sfd, NULL);
  return FbleNewMaybeValue(runtime, sfd_value);
}

// /Network/Sockets/Native%.Client foreign function.
FbleForeign _Fble_2f_Network_2f_Sockets_2f_Native_25__2e_Client = {
  .path = "/Network/Sockets/Native%",
  .name = "Client",
  .num_args = 3,
  .max_call_args = 0,
  .run = &Client,
};

/**
 * @func[Accept] FbleRunFunction to accept a tcp connection.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Socket@, Unit@) { Socket@; }
 */
static FbleValue* Accept(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  SOCKET sfd = (SOCKET)(intptr_t)FbleNativeValueData(args[0]);

  SOCKET cfd = accept(sfd, NULL, NULL);
  if (cfd == INVALID_SOCKET) {
    // TODO: What to do here?
    perror("accept");
    assert(false);
  }

  return FbleNewNativeValue(runtime, (void*)(intptr_t)cfd, NULL);
}

// /Network/Sockets/Native%.Accept foreign function.
FbleForeign _Fble_2f_Network_2f_Sockets_2f_Native_25__2e_Accept = {
  .path = "/Network/Sockets/Native%",
  .name = "Accept",
  .num_args = 2,
  .max_call_args = 0,
  .run = &Accept,
};

/**
 * @func[Close] FbleRunFunction to close a tcp connection.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Socket@, Unit@) { Unit@; }
 */
static FbleValue* Close(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  SOCKET sfd = (SOCKET)(intptr_t)FbleNativeValueData(args[0]);
  closesocket(sfd);
  return FbleNewStructValue_(runtime, 0);
}

// /Network/Sockets/Native%.Close foreign function.
FbleForeign _Fble_2f_Network_2f_Sockets_2f_Native_25__2e_Close = {
  .path = "/Network/Sockets/Native%",
  .name = "Close",
  .num_args = 2,
  .max_call_args = 0,
  .run = &Close,
};


/**
 * @func[Server] FbleRunFunction to start a tcp server.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (String@, Int@, Unit@) { Maybe@<*(Int@ port, Socket@ socket)>; }
 */
static FbleValue* Server(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
  (void)profile;

  char* host = FbleStringValueAccess(args[0]);
  int port = (int)FbleIntValueAccess(args[1]);

  char portstr[10];
  snprintf(portstr, 10, "%d", port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;    // TODO: AF_INET?
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
  hints.ai_protocol = 0;

  struct addrinfo* result = NULL;

  int s = getaddrinfo(host, portstr, &hints, &result);
  if (s != 0) {
    // For debug. TODO: return the error message instead?
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(s));
    assert(result == NULL);
  }
  FbleFree(host);

  SOCKET sfd = INVALID_SOCKET;
  for (struct addrinfo* rp = result; sfd == INVALID_SOCKET && rp != NULL; rp = rp->ai_next) {
    // TODO: rp->ai_family rather than AF_INET?
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == INVALID_SOCKET) {
      // For debug. TODO: return the error message instead?
      perror("socket");
      continue;
    }

    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1) {
      sfd = INVALID_SOCKET;
      continue;
    }
  }

  freeaddrinfo(result);

  if (sfd != INVALID_SOCKET) {
    // TODO: How much backlog to use?
    if (listen(sfd, 10) != 0) {
      // TODO: Return error instead?
      perror("listen");
      closesocket(sfd);
      sfd = INVALID_SOCKET;
    }
  }

  if (sfd == INVALID_SOCKET) {
    return FbleNewMaybeValue(runtime, NULL); // Nothing
  }

  // TODO: Don't assume AF_INET here?
  struct sockaddr_in addr;
  socklen_t addr_size = sizeof(addr);
  if (getsockname(sfd, (struct sockaddr*)&addr, &addr_size) < 0) {
    perror("getsockname");
  } else {
    port = ntohs(addr.sin_port);
  }

  FbleValue* port_value = FbleNewIntValue(runtime, port);
  FbleValue* sfd_value = FbleNewNativeValue(runtime, (void*)(intptr_t)sfd, NULL);
  FbleValue* server_value = FbleNewStructValue_(runtime, 2, port_value, sfd_value);
  return FbleNewMaybeValue(runtime, server_value);
}

// /Network/Sockets/Native%.Server foreign function.
FbleForeign _Fble_2f_Network_2f_Sockets_2f_Native_25__2e_Server = {
  .path = "/Network/Sockets/Native%",
  .name = "Server",
  .num_args = 3,
  .max_call_args = 0,
  .run = &Server,
};

/**
 * @func[Init] FbleRunFunction to initialize sockets infra.
 *  See documentation of FbleRunFunction in fble-function.h
 *
 *  The fble type of the function is:
 *
 *  @code[fble] @
 *   (Unit@) { Unit@; }
 */
static FbleValue* Init(
    FbleRuntime* runtime, FbleProfileThread* profile,
    FbleFunction* function, FbleValue** args)
{
#ifdef __WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    // Uh... what to do here?
    assert(false && "couldn't initialize windows sockets?");
  }
#endif // __WIN32

  return FbleNewStructValue_(runtime, 0);
}

// /Network/Sockets/Native%.Init foreign function.
FbleForeign _Fble_2f_Network_2f_Sockets_2f_Native_25__2e_Init = {
  .path = "/Network/Sockets/Native%",
  .name = "Init",
  .num_args = 1,
  .max_call_args = 0,
  .run = &Init,
};
