#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
		int cpu, int group_fd, unsigned long flags)
{
  int ret;

  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
		group_fd, flags);
  return ret;
}

static pid_t gettid(void) {
  return syscall(__NR_gettid);
}

static void sigpwr_handler(int sig, siginfo_t *info, void *ucontext)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  printf("%u got signal: %u now: %lu.%lu\n", gettid(), sig, ts.tv_sec, ts.tv_nsec);
}

static void handle_signal(int sig)
{
  struct sigaction sa, old_sa;

  memset(&sa, 0, sizeof(sa));

  sigset_t sigset;

  sigemptyset(&sigset);
  sa.sa_sigaction = sigpwr_handler;
  sa.sa_flags = SA_SIGINFO | SA_RESTART;
  sa.sa_mask = sigset;

  sigaction(SIGPWR, &sa, &old_sa);
}

static int open_desched_event(pid_t pid)
{
  struct perf_event_attr pe;
  int fd;

  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = PERF_TYPE_SOFTWARE;
  pe.size = sizeof(struct perf_event_attr);
  pe.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
  pe.disabled = 1;
  pe.sample_period = 1;
  fd = perf_event_open(&pe, pid, -1, -1, PERF_FLAG_FD_CLOEXEC);
  if (fd == -1) {
    fprintf(stderr, "perf_event_open failed, error: %s\n",
	    strerror(errno));
    exit(1);
  }

  ioctl(fd, PERF_EVENT_IOC_RESET, 0);

  return fd;
}

static int enable_desched_event(int fd)
{
  return ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

static void disable_desched_event(int fd)
{
  ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
}

static void close_desched_event(int fd)
{
  close(fd);
}

int main(int argc, char* argv[])
{
  pid_t pid;
  int fd;

  struct timespec ts = {1, 0}, rem;

  pid = gettid();

  if ((fd = open_desched_event(pid)) < 0) {
    fprintf(stderr, "perf_event_open failed: %s\n",
	    strerror(errno));
    exit(1);
  }

  struct f_owner_ex ex = {
    .type = F_OWNER_PID,
    .pid  = pid,
  };
  
  if (fcntl(fd, F_SETFL, O_ASYNC) < 0) {
    fprintf(stderr, "f_setfl failed: %s\n", strerror(errno));
    exit(1);
  }
  if (fcntl(fd, F_SETOWN_EX, &ex) < 0) {
    fprintf(stderr, "f_setown failed: %s\n", strerror(errno));
    exit(1);
  }

  if (fcntl(fd, F_SETSIG, SIGPWR) < 0) {
    fprintf(stderr, "f_setsig failed: %s\n", strerror(errno));
    exit(1);
  }

  handle_signal(SIGPWR);

  enable_desched_event(fd);

  while (1) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    printf("%u entering sleep, now: %lu.%lu\n", gettid(), now.tv_sec, now.tv_nsec);
    nanosleep(&ts, &rem);
  }

  disable_desched_event(fd);
  close_desched_event(fd);

  return 0;
}
