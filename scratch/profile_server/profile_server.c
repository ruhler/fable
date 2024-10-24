
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void handle_connection(int fd)
{
  char buf[1025];
  ssize_t len = 0;
  do {
    for (size_t i = 0; i < len; ++i) {
      printf("%c", toupper(buf[i]));
    }
    len = read(fd, buf, 1024);
    if (len < 0) {
      perror("read");
      return;
    }
  } while (len > 0);
}

int main(int argc, char *argv[])
{
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd < 0) {
    perror("socket");
    return 1;
  }

	struct sockaddr_in s_addr;
	memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sin_family = AF_INET;
  s_addr.sin_port = htons(8888);
  s_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(sfd, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
		perror("bind");
    return 1;
  }

	if (listen(sfd, 10) < 0) {
		perror("listen");
    return 1;
  }

  while (true) {
    int cfd = accept(sfd, NULL, NULL);
    if (cfd < 0) {
      perror("accept");
      return 1;
    }
    handle_connection(cfd);
    close(cfd);
  }

  return 0;
}
