#include "common.h"
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static void usage(const char *a){fprintf(stderr,"Usage: %s <cmd> [args]\n",a); exit(1);}
static double d(struct timespec a, struct timespec b){
 return (b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9;}
int main(int c,char**v){
/* TODO : ADD CODE HERE
*/if (c < 2) usage(v[0]);

  struct timespec t0, t1;
  if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
    perror("clock_gettime");
    return 1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    execvp(v[1], &v[1]);
    perror("execvp");
    _exit(127);
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno == EINTR) continue;
    perror("waitpid");
    return 1;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
    perror("clock_gettime");
    return 1;
  }

  double elapsed = d(t0, t1);

  // IMPORTANT: must contain "exit=0" for /bin/true
  printf("pid=%d ", (int)pid);

  if (WIFEXITED(status)) {
    printf("exit=%d ", WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    printf("signal=%d ", WTERMSIG(status));
  } else {
    // uncommon, but keep some output
    printf("status=%d ", status);
  }

  printf("time=%.6f\n", elapsed);
  return 0;
}

