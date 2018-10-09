#pragma once

// #include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

struct proc {
   pid_t pid;
   int fds[2];
};

static inline void
close_fd(int *fd)
{
   assert(fd);
   if (*fd >= 0)
      close(*fd);
}

static inline bool
proc_open(const char *file, char *const argv[], struct proc *out_proc)
{
   assert(file && argv && out_proc);
   *out_proc = (struct proc){0};

   int pipes[4];
   pipe(&pipes[0]); /* parent */
   pipe(&pipes[2]); /* child */

#if 0
   // Doesn't work, no idea why
   posix_spawn_file_actions_t fa;
   if (posix_spawn_file_actions_init(&fa) != 0 ||
       posix_spawn_file_actions_addclose(&fa, pipes[0]) != 0 ||
       posix_spawn_file_actions_addclose(&fa, pipes[3]) != 0 ||
       posix_spawn_file_actions_adddup2(&fa, pipes[2], 0) != 0 ||
       posix_spawn_file_actions_adddup2(&fa, pipes[1], 1) != 0 ||
       posix_spawn_file_actions_addclose(&fa, pipes[2]) != 0 ||
       posix_spawn_file_actions_addclose(&fa, pipes[1]) != 0 ||
       posix_spawnp(&out_proc->pid, file, &fa, NULL, argv, NULL) != 0) {
      posix_spawn_file_actions_destroy(&fa);
      for (uint8_t i = 0; i < ARRAY_SIZE(pipes); ++i)
         close(pipes[i]);
      return false;
   }
   posix_spawn_file_actions_destroy(&fa);
#else
   if ((out_proc->pid = fork()) > 0) {
      out_proc->fds[0] = pipes[3];
      out_proc->fds[1] = pipes[0];
      close(pipes[1]);
      close(pipes[2]);
      return true;
   } else {
      close(pipes[0]);
      close(pipes[3]);
      dup2(pipes[2], 0);
      dup2(pipes[1], 1);
      close(pipes[2]);
      close(pipes[1]);
      execvp(file, argv);
      _exit(0);
   }
#endif

   out_proc->fds[0] = pipes[3];
   out_proc->fds[1] = pipes[0];
   close(pipes[1]);
   close(pipes[2]);
   return true;
}

static inline void
proc_close(struct proc *proc)
{
   assert(proc);
   waitpid(proc->pid, NULL, 0);
   close_fd(&proc->fds[0]);
   close_fd(&proc->fds[1]);
   *proc = (struct proc){0};
}
