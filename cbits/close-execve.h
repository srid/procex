#pragma once

#include <stdlib.h>

void close_execve(
  const char *path,
  char *const argv[],
  char *const envp[],
  int fds[],
  size_t fd_count,
  int *error_pipe_write
);
