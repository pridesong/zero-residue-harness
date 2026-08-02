# residue-audit

Measures the actual cross-test residue of a standard `fork`/`exec` cold start
— the isolation primitive every agent harness relies on. Real numbers, not
theory.

## Build & run

```bash
make
./audit
```

Requires Linux (POSIX). The tool has two modes, both in one binary:

- `./audit` — parent: sets up a known state (2 blocked signals, 3 non-CLOEXEC
  fds, cwd, umask, rlimit, touched heap pages), forks, execs the probe.
- `./audit probe` — child (exec'd): reports which state survived the exec.

The parent additionally reports the fork-only view (worker-pool reuse model):
touched pages COW-inherited from the parent.

## Output

Two JSON documents, printed in order:

1. **exec-probe** (child, exec'd) — the survivor report: signal mask,
   non-CLOEXEC fds, cwd bytes, umask, rlimits, pid.
2. **audit-report** (parent) — fork/exec outcome + fork-only touched pages
   (worker-pool reuse model).

Architecture-level state (TLB/cache warmth) is not measurable from userland;
the probe flags it and points to `docs/residue-domain.md` for the simulator
coverage.

## Interpreting the result

The child probe exits with *the same signal mask, the same fds, the same cwd,
the same umask, the same rlimits* the parent set — bytes that a harness
claiming "clean start per test" silently carries across every test boundary.

If the sum is nonzero (it will be), the harness's `from-zero` claim is a
POSIX contract, not a property. That is the entire point.
