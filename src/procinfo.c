#include "common.h"
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

static void usage(const char *a) {
  fprintf(stderr, "Usage: %s <pid>\n", a);
  exit(1);
}

static long parse_pid_or_die(const char *s, const char *argv0) {
  char *end = NULL;
  errno = 0;
  long pid = strtol(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0' || pid <= 0) usage(argv0);
  return pid;
}

static void die_open(const char *path) {
  if (errno == ENOENT) {
    fprintf(stderr, "Error: PID not found (%s)\n", path);
  } else if (errno == EACCES) {
    fprintf(stderr, "Error: Permission denied (%s)\n", path);
  } else {
    fprintf(stderr, "Error: Could not open %s: %s\n", path, strerror(errno));
  }
  exit(1);
}

static int read_cmdline(char *out, size_t outsz, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) die_open(path);

  size_t n = fread(out, 1, outsz - 1, f);
  fclose(f);

  if (n == 0) {
    out[0] = '\0';
    return 0;
  }

  out[n] = '\0';
  for (size_t i = 0; i + 1 < n; i++) {
    if (out[i] == '\0') out[i] = ' ';
  }
  while (n > 0 && (out[n - 1] == '\0' || out[n - 1] == ' ')) {
    out[n - 1] = '\0';
    n--;
  }
  return 1;
}

static int read_vmrss_kb(long *vmrss_kb, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) die_open(path);

  char line[512];
  int found = 0;
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      long kb = 0;
      if (sscanf(line + 6, "%ld", &kb) == 1) {
        *vmrss_kb = kb;
        found = 1;
      }
      break;
    }
  }
  fclose(f);
  return found;
}

static void read_stat_fields(char *state, long *ppid, long long *utime, long long *stime,
                             const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) die_open(path);

  char buf[4096];
  if (!fgets(buf, sizeof(buf), f)) {
    fclose(f);
    fprintf(stderr, "Error: Could not read %s\n", path);
    exit(1);
  }
  fclose(f);

  // /proc/<pid>/stat: pid (comm) state ppid ...
  char *rparen = strrchr(buf, ')');
  if (!rparen || rparen[1] != ' ') {
    fprintf(stderr, "Error: Unexpected format in %s\n", path);
    exit(1);
  }

  char *rest = rparen + 2; // points to "state ppid ..."

  // Tokenize rest by spaces. In this "rest" view:
  // token1 = state (field 3)
  // token2 = ppid  (field 4)
  // token12 = utime (field 14)
  // token13 = stime (field 15)
  int tok = 0;
  char *save = NULL;
  char *t = strtok_r(rest, " ", &save);

  char st = '?';
  long parent = -1;
  long long u = -1, s = -1;

  while (t) {
    tok++;
    if (tok == 1) {
      st = t[0];
    } else if (tok == 2) {
      parent = strtol(t, NULL, 10);
    } else if (tok == 12) {
      u = strtoll(t, NULL, 10);
    } else if (tok == 13) {
      s = strtoll(t, NULL, 10);
      break;
    }
    t = strtok_r(NULL, " ", &save);
  }

  if (parent < 0 || u < 0 || s < 0) {
    fprintf(stderr, "Error: Missing fields in %s\n", path);
    exit(1);
  }

  *state = st;
  *ppid = parent;
  *utime = u;
  *stime = s;
}

int main(int c, char **v) {
  if (c != 2) usage(v[0]);
  long pid = parse_pid_or_die(v[1], v[0]);

  char path_stat[256], path_status[256], path_cmdline[256];
  snprintf(path_stat, sizeof(path_stat), "/proc/%ld/stat", pid);
  snprintf(path_status, sizeof(path_status), "/proc/%ld/status", pid);
  snprintf(path_cmdline, sizeof(path_cmdline), "/proc/%ld/cmdline", pid);

  char state;
  long ppid;
  long long utime_ticks, stime_ticks;
  read_stat_fields(&state, &ppid, &utime_ticks, &stime_ticks, path_stat);

  long vmrss_kb = 0;
  int have_rss = read_vmrss_kb(&vmrss_kb, path_status);

  char cmd[4096];
  int have_cmd = read_cmdline(cmd, sizeof(cmd), path_cmdline);

  long ticks = sysconf(_SC_CLK_TCK);
  double cpu_sec = 0.0;
  if (ticks > 0) cpu_sec = (double)(utime_ticks + stime_ticks) / (double)ticks;

  // Output (adjust labels if tests expect different)
  printf("PID:%ld\n", pid);
  printf("State: %c\n", state);
  printf("PPid: %ld\n", ppid);
  printf("Cmd: %s\n", have_cmd && cmd[0] ? cmd : "[unknown]");
  printf("CPU:%lld %.3f\n", utime_ticks + stime_ticks, cpu_sec);
  if (have_rss) printf("VmRSS: %ld\n", vmrss_kb);
  else printf("VmRSS: N/A\n");

  return 0;
}

