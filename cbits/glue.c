#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

extern const char *procex_intermediate_exe_contents;
extern const size_t procex_intermediate_exe_contents_len;

static int intermediate_exe_fd = -1;

static int total_write(int fd, const char* contents, size_t len) {
  size_t total = 0;
  while (total < len) {
    ssize_t c = write(fd, contents + total, len - total);
    if (c == -1) {
      return -1;
    }
    total += c;
  }
  return 0;
}

static int init_fd(void) {
  char filename[] = "procex-intermediate-XXXXXX";
  int fd = mkstemp(filename);
  if (fd == -1) {
    return -1;
  }
  if (unlink(filename) == -1) {
    close(fd);
    return -1;
  }

  if (fchmod(fd, S_IRWXU) == -1) {
    close(fd);
    return -1;
  }

  if (total_write(fd, procex_intermediate_exe_contents, procex_intermediate_exe_contents_len) == -1) {
    close(fd);
    return -1;
  }

  if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
    close(fd);
    return -1;
  }

  return fd;
}

// Fork, close file descriptors, then execute.
pid_t vfork_close_execve(
  const char *path, // The path to executable, does not look through PATH
  char *const argv[], // Will be passed verbatim to execve
  // Will be passed verbatim to execve if not NULL, otherwise it will be set to the current environment
  char *const envp[],
  // This is an array that is fd_count long of all file descriptorswe want to share.
  // In the new process, the descriptors will be renamed, fd[i] will be renamed to i using dup2.
  // -1 means it will be closed.
  int fds[],
  size_t fd_count
) {
  int pipe_child_us[2] = { -1, -1 };
  if (pipe(pipe_child_us) == -1) return -1;
  int pipe_us_child[2] = { -1, -1 };
  if (pipe(pipe_us_child) == -1) return -1;

  if (intermediate_exe_fd == -1) {
    // could be subject to races, but probably ok
    int fd = init_fd();
    if (fd == -1) {
      return -1;
    }
    intermediate_exe_fd = fd;
  }

  char pipe_fd_string[20];
  snprintf(pipe_fd_string, sizeof(pipe_fd_string), "%d", pipe_us_child[0]);

  int argc = 0;
  while (argv[argc] != NULL) argc++;
  // one entry for each original argument, one for the fd description, one for the original path, one for the null entry
  const char **new_argv = calloc(argc + 3, sizeof(char *));
  new_argv[0] = pipe_fd_string;
  new_argv[1] = path;
  memcpy(&new_argv[2], argv, (argc + 1) * sizeof(char *));

  pid_t pid = vfork();
  // vfork had an error.
  if (pid == -1) {
    return -1;
  // We are in the child.
  } else if (pid == 0) {
    fexecve(intermediate_exe_fd, (char *const*)new_argv, envp);
    _exit(1);
  } else {
    // first tell child where to respond
    if (total_write(pipe_us_child[1], (char*)&pipe_child_us[1], sizeof(pipe_child_us[1])) == -1) {
      return -1;
    }
    // then send over fd_count
    if (total_write(pipe_us_child[1], (char*)&fd_count, sizeof(fd_count)) == -1) {
      return -1;
    }
    // then send over all the file descriptors we wish to bind
    if (total_write(pipe_us_child[1], (char*)fds, fd_count * sizeof(fds[0])) == -1) {
      return -1;
    }
    // the child should respond now, either with an empty message (EOF) for success,
    // or a 1-byte 'e' for failure.
    char out_byte = '\0';
    if (read(pipe_child_us[0], &out_byte, 1) == 0) {
      // Pipe closed successfully.
      return pid;
    } else {
      return -1;
    }
  }
}
