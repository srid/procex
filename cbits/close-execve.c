#include "close-execve.h"
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/select.h>

static void reset_signal_state(void) {
  for (int sig = 1; sig < NSIG; sig++) {
    if (sig == SIGKILL || sig == SIGSTOP) continue;

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
  }

  sigset_t set;
  sigemptyset(&set);
  sigprocmask(SIG_SETMASK, &set, NULL);

  for (int sig = 1; sig < NSIG; sig++) {
    if (sig == SIGKILL || sig == SIGSTOP) continue;

    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
  }
}

// This is like vfork_close_execve but replaces the current process.
void close_execve(
  const char *path,
  char *const argv[],
  char *const envp[],
  int fds[],
  size_t fd_count,
  int *error_pipe_write
) {
  *error_pipe_write = dup(*error_pipe_write);
  if (*error_pipe_write == -1) return;

  // We make sure the file desciptors in the array point to what they're
  // supposed to point to, since if e.g. one pointed to stdin (fd 0),
  // we want it to mean the old stdin, not the new stdin.
  for (size_t i = 0; i < fd_count; i++) {
    if (fds[i] != -1) {
      int fd = dup(fds[i]);
      if (fd == -1) return;
      fds[i] = fd;
    }
  }

  // Rename the file descriptors as specified,
  // closing the ones we don't want.
  for (int i = 0; i < fd_count; i++) {
    if (fds[i] == -1) {
      if (close(i) == -1) return;
    } else {
      if (dup2(fds[i], i) == -1) return;
    }
  }

  if (dup2(*error_pipe_write, fd_count) == -1) return;
  fcntl(fd_count, F_SETFD, FD_CLOEXEC);
  *error_pipe_write = fd_count;

  closefrom(fd_count + 1);

  // Reset fd limit for compatibility with select(), see http://0pointer.net/blog/file-descriptor-limits.html.
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) < 0) return;
  rl.rlim_cur = FD_SETSIZE;
  if (setrlimit(RLIMIT_NOFILE, &rl) < 0) return;

  reset_signal_state();

  execve(path, argv, envp);
}
