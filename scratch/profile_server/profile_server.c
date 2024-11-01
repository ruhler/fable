
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Handles an incoming socket connection.
// Takes ownership of fd, closing it when done.
static void handle_connection(int fd)
{
  FILE* fin = fdopen(fd, "r");

  char* line = NULL;
  size_t line_size = 0;
  ssize_t read = 0;
 
  // Get the start line
  do {
    read = getline(&line, &line_size, fin);
  } while (read >= 0 && strcmp(line, "\n") == 0);

  if (read < 0) {
    perror("failed to read start-line");
    free(line);
    fclose(fin);
    return;
  }

  char* start = strdup(line);
  printf("start-line: %s", start);

  // Skip past request headers. We don't care about those.
  do {
    read = getline(&line, &line_size, fin);
    fprintf(stderr, "%s", line);
  } while (read >= 0 && strcmp(line, "\r\n") != 0);

  if (read < 0) {
    perror("failed to parse rest of response");
    free(start);
    free(line);
    fclose(fin);
    return;
  }

  char* method = start;
  char* method_end = strchr(method, ' ');
  char* uri = method_end + 1;
  char* uri_end = strchr(uri, ' ');
  char* version = uri_end + 1;

  *method_end = '\0';
  *uri_end = '\0';

  char* path = uri;
  char* path_end = strchr(uri, '?');
  char* query = NULL;
  if (path_end != NULL) {
    query = path_end + 1;
    *path_end = '\0';
  }

  FILE* fout = fdopen(fd, "w");
  fprintf(fout, "HTTP/1.1 200 OK\n");
  fprintf(fout, "Content-Type: text/html\r\n");
  fprintf(fout, "\r\n");
  fprintf(fout, "<h1>Request Info</h1>\n");
  fprintf(fout, "Method: %s <br/>\n", method);
  fprintf(fout, "Path: %s <br/>\n", path);
  fprintf(fout, "Query: %s <br/>\n", query);
  fprintf(fout, "Version: %s <br/>\n", version);
  fflush(fout);

  free(start);
  free(line);
  fclose(fin);
  fclose(fout);
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
  }

  return 0;
}
