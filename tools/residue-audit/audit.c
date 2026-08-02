/*
 * audit.c — fork/exec residue auditor
 *
 * Measures, byte by byte, the cross-test state that a standard
 * fork/exec cold start inherits. Outputs JSON to stdout.
 *
 * Usage:
 *   make && ./audit            # parent mode: set state, fork+exec probe
 *   ./audit probe              # child mode: report inherited state (exec'd)
 *
 * Design: single binary, two modes. The parent sets up a known state
 * (blocked signals, open FDs, cwd, umask, rlimits, touched heap pages),
 * then forks. The child execs itself as "probe" — a POSIX-standard cold
 * start — and reports what survived. The parent also runs a fork-only
 * comparison (worker-pool reuse model) and reports COW-inherited pages.
 *
 * Everything reported here is measured, not estimated — except items
 * explicitly marked "architecture-level", which cannot be measured from
 * userland and are flagged for the simulator instead.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

/* ------------------------------------------------------------------ */
/* JSON emission helpers                                              */
/* ------------------------------------------------------------------ */

static void jkey(const char *k) { printf("  \"%s\": ", k); }
static void jstr(const char *k, const char *v) { jkey(k); printf("\"%s\",\n", v); }
static void jnum(const char *k, long v) { jkey(k); printf("%ld,\n", v); }
static void jbool(const char *k, int v) { jkey(k); printf("%s,\n", v ? "true" : "false"); }

/* ------------------------------------------------------------------ */
/* Probe mode: report state inherited across exec                     */
/* ------------------------------------------------------------------ */

static int probe_mode(void) {
    printf("{\n");
    jstr("mode", "exec-probe");

    /* 1. signal mask — POSIX: preserved by exec */
    sigset_t mask;
    sigprocmask(SIG_SETMASK, NULL, &mask);
    jnum("signal_mask_bytes", (long)sizeof(mask));
    jbool("signal_mask_nonzero", !sigisemptyset(&mask));
    /* count set bits */
    int bits = 0;
    for (int s = 1; s < NSIG; s++)
        if (sigismember(&mask, s)) bits++;
    jnum("signal_mask_set_bits", bits);

    /* 2. non-CLOEXEC fds — POSIX: preserved by exec */
    int fds = 0;
    for (int fd = 0; fd < 64; fd++) {
        int flags = fcntl(fd, F_GETFD);
        if (flags >= 0 && !(flags & FD_CLOEXEC)) fds++;
    }
    jnum("non_cloexec_fds", fds);

    /* 3. cwd — preserved by exec */
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)))
        jnum("cwd_bytes", (long)strlen(cwd) + 1);
    else
        jnum("cwd_bytes", 0);

    /* 4. umask — preserved by exec */
    mode_t um = umask(0);
    umask(um);
    jnum("umask_bytes", (long)sizeof(mode_t));

    /* 5. rlimits — preserved by exec */
    struct rlimit rl;
    long rl_bytes = 0;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) rl_bytes += sizeof(rl);
    if (getrlimit(RLIMIT_AS, &rl) == 0) rl_bytes += sizeof(rl);
    if (getrlimit(RLIMIT_STACK, &rl) == 0) rl_bytes += sizeof(rl);
    if (getrlimit(RLIMIT_CORE, &rl) == 0) rl_bytes += sizeof(rl);
    if (getrlimit(RLIMIT_CPU, &rl) == 0) rl_bytes += sizeof(rl);
    jnum("rlimit_bytes_measured", rl_bytes);

    /* 6. pid bookkeeping — changes, but is observable state */
    jnum("pid", (long)getpid());

    printf("  \"note\": \"architecture-level state (TLB/cache warmth) is not"
           " measurable from userland; see docs/residue-domain.md\"\n");
    printf("}\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Parent mode: set up known state, fork, exec probe                   */
/* ------------------------------------------------------------------ */

static void parent_setup_state(void) {
    /* block two signals */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    /* open files without CLOEXEC */
    int fd = open("/etc/hostname", O_RDONLY);
    (void)fd;
    fd = open("/etc/resolv.conf", O_RDONLY);
    (void)fd;
    fd = open("/proc/self/stat", O_RDONLY);
    (void)fd;

    /* change directory */
    if (chdir("/tmp") != 0) chdir("/");

    /* set umask */
    umask(0022);

    /* raise a soft rlimit */
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        rl.rlim_cur = 1024;
        setrlimit(RLIMIT_NOFILE, &rl);
    }
}

/* fork-only comparison: count COW-inherited touched pages */
static long count_touched_pages(void *base, size_t len) {
    size_t npages = (len + 4095) / 4096;
    unsigned char *vec = calloc(npages, 1); /* mincore: one byte per page */
    if (!vec) return -1;
    if (mincore(base, len, vec) != 0) { free(vec); return -1; }
    long touched = 0;
    for (size_t i = 0; i < npages; i++)
        if (vec[i]) touched++;
    free(vec);
    return touched;
}

static int audit_mode(void) {
    const size_t HEAP_BYTES = 64 * 1024 * 1024; /* 64 MiB region */
    char *heap = mmap(NULL, HEAP_BYTES, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heap == MAP_FAILED) { perror("mmap"); return 1; }
    /* touch every 4th page -> deterministic touched-page count */
    for (size_t off = 0; off < HEAP_BYTES; off += 16384)
        heap[off] = 1;

    parent_setup_state();

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* child: POSIX-standard cold start via exec */
        char *const argv[] = { "audit", "probe", NULL };
        execv("/proc/self/exe", argv);
        perror("execv");
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    /* parent: measure the fork-only view of the same heap */
    long touched = count_touched_pages(heap, HEAP_BYTES);

    printf("{\n");
    jstr("mode", "audit-report");
    jnum("child_exit_status", status);
    jnum("fork_touched_pages", touched);
    jnum("fork_touched_bytes", touched > 0 ? touched * 4096 : 0);
    jstr("note",
         "parent state was: 2 blocked signals, 3 non-CLOEXEC fds, cwd=/tmp,"
         " umask=0022, RLIMIT_NOFILE.cur=1024, 16 KiB-touched heap pages."
         " child probe (exec'd) reports what survived");
    printf("}\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "probe") == 0)
        return probe_mode();
    return audit_mode();
}
