# Residue measurement report

Measured cross-test residue of standard isolation primitives. Every number
below was produced by `tools/residue-audit` on a real kernel — none of it is
estimated.

## Environment

| Item | Value |
|------|-------|
| Kernel | Linux (container) |
| Arch | x86-64 |
| libc | glibc |
| Date | 2026-08-02 |
| Reproduce | `cd tools/residue-audit && make && ./audit` |

## Result 1 — `fork` + `exec` cold start (the standard harness primitive)

The parent sets up a known state: 2 blocked signals, 3 non-CLOEXEC fds,
cwd, umask, rlimits, touched heap pages. The child `exec`s and reports what
survived.

| Item | Measured | POSIX semantics |
|------|----------|-----------------|
| signal mask | 128 B (2 bits set) | preserved by exec — **entire mask survives** |
| non-CLOEXEC fds | 6 | preserved by exec (incl. stdio) |
| cwd | 5 B | preserved by exec |
| umask | 4 B | preserved by exec |
| rlimits (5 measured) | 80 B | preserved by exec |
| pid bookkeeping | changed | observable state delta |

**Subtotal: ~217 B of architectural state carried across every exec boundary.**

## Result 2 — `fork` reuse (the worker-pool / fixture-reuse model)

A 64 MiB anonymous region, touched every 16 KiB by the parent, then `fork`:

| Item | Measured |
|------|----------|
| COW-inherited touched pages | 4096 |
| Inherited bytes | 16 MiB |

## Conclusion

1. **`exec` is state inheritance, not state creation.** The child carries the
   parent's signal mask, fds, cwd, umask, rlimits by POSIX contract — a
   constructive ≥1 B floor.
2. **`fork` is state duplication.** The child inherits every touched page —
   megabytes of warmed state.
3. **Therefore `from-zero` per test is a claim, not a property**, for both the
   standard cold-start model and the reuse model.

This is the problem this project exists to fix — see
[`docs/design.md`](design.md) for the isolation primitive that replaces
`fork/exec`, and [`docs/residue-domain.md`](residue-domain.md) for the exact
0 B contract.

## Reproducibility

The CI workflow (`.github/workflows/ci.yml`) rebuilds and reruns the auditor on
every push and asserts the residue is present — the measurement is a living
check, not a one-off.
