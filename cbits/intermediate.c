#include "close-execve.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern char **environ;

static int total_read(int fd, char* out, size_t count) {
  while (count > 0) {
    ssize_t c = read(fd, out, count);
    if (c == -1) {
      return -1;
    }
    count -= c;
    out += c;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  // we strip two entries
  char **new_argv = calloc(argc - 1, sizeof(char *));
  memcpy(new_argv, argv + 2, argc * sizeof(char *));

  int pipe_to_us = atoi(argv[0]);
  char *path = argv[1];

  int pipe_back = -1;
  if (total_read(pipe_to_us, (char*)&pipe_back, sizeof(pipe_back)) == -1) {
    exit(1);
  }

  size_t fd_count = 0;
  if (total_read(pipe_to_us, (char*)&fd_count, sizeof(fd_count)) == -1) {
    exit(1);
  }

  int *fds = calloc(fd_count, sizeof(int));
  if (total_read(pipe_to_us, (char*)fds, fd_count * sizeof(int)) == -1) {
    exit(1);
  }

  close_execve(path, new_argv, environ, fds, fd_count, &pipe_back);

  // The above shouldn't return; if it does, we fail.
  char byte = 'e';

  // Indicates that an error has happened.
  // We assume that the write always succeeds, which it should I think
  // on all systems.
  assert(write(pipe_back, &byte, 1) == 1);

  _exit(1);
}
