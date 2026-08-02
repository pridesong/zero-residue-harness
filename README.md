# Zero-Residue Harness

**Cross-test isolation with a *certified* 0-byte residue — not an assertion, a measured fact.**

Agent harnesses (eval frameworks, benchmark runners, RL environments) promise that every test starts from a clean state. They don't. `fork/exec` boots the child with a warm CPU and inherited OS state — signal mask, open file descriptors, cwd, umask, rlimits, FPU/AVX registers, warm TLB/cache lines, touched heap pages. Every "cold" process carries **≥1 byte** of cross-test residue. `from-zero` is a claim, not a property.

This project exists to make it a property:

1. **`tools/residue-audit`** — measure the actual residue of a standard `fork/exec` cold start. Real numbers, not theory. (Spoiler: it's not zero.) See `docs/residue-measurement.md` for the measured report.
2. **`docs/residue-domain.md`** — the exact contract ℛ: which architectural state counts, which doesn't, and how each byte is verified.
3. **`docs/design.md`** — Cold-Start Island: a per-test hardware-reset isolation fabric where 0 B residue becomes a **machine-verified certificate** instead of a POSIX promise.
4. **`sim/`** — architectural-state diff harness (gem5 full-system) that proves the reset domain is complete.

## Why now

A 2026-04 audit of agent-harness isolation exposed that mainstream harnesses cannot guarantee cross-test state isolation, and the problem is architectural, not a bug: POSIX `exec` semantics *inherit* state by design. The fix is not a cleverer cleanup — it is changing the isolation primitive itself.

## Quick start

```bash
cd tools/residue-audit
make
./audit
```

The tool spawns a standard `fork` + `exec` chain and reports the measured residue, item by item, in bytes. Measured output (Debian container, x86-64, glibc — 2026-08-02):

```
{
  "mode": "exec-probe",
  "signal_mask_bytes": 128,        // POSIX exec preserves the whole mask
  "signal_mask_set_bits": 2,       // parent's blocked signals survived
  "non_cloexec_fds": 6,            // stdio + 3 parent-opened files
  "cwd_bytes": 5,                  // cwd preserved
  "umask_bytes": 4,                // umask preserved
  "rlimit_bytes_measured": 80,     // 5 limits preserved
  "pid": 16                        // pid bookkeeping changed
}
{
  "mode": "audit-report",
  "fork_touched_pages": 4096,      // COW-inherited via fork
  "fork_touched_bytes": 16777216   // 16 MiB of touched heap pages
}
```

**A standard "cold" start carries ~217 B of architectural state plus 16 MiB of inherited pages (fork model).** `from-zero` is a claim, not a property.

If your harness claims "clean start per test", run this and check the number.

## The claim

> **0 B cross-test residue is achievable and verifiable only when the cold state is *fabricated* (architectural RESET + stream-cleared memory) instead of *inherited* (fork/exec).**

The full argument is in `docs/design.md`. The verification protocol (5 acceptance criteria) is in `docs/verification.md`.

## Roadmap

| Project | Status | Why |
|---|---|---|
| **zero-residue-harness** (this repo) | active | The isolation window: a 2026-04 exposure left this space open — it will be taken within months |
| Agentic eval cost architecture | planned | Cache / parallelism / selective replay / cost-budget protocol — pure engineering, useful on day one |
| Cross-protocol eval benchmark (MCP/A2A/ACP) | research | The largest structural gap named by recent surveys |
| Eval governance spec | planned | "Governance is an environment property" — companion document |
| Production eval (ground-truth indefinable) | deferred | A research problem, not an engineering pain |
| Non-reproducibility | deferred | Low differentiation |

## Verification

Five acceptance criteria, all with thresholds — see `docs/verification.md`:

1. State-vector diff == 0 B across 1000 tests
2. Full-zero hardware readback (bank + ECC plane)
3. Timing budget (reset ≤ 2.4 µs, clear ≤ 1.30 ms @ 85 °C)
4. Fault injection: 300/300 corrupted images halt before test entry
5. Determinism: byte-identical cycle traces across runs and slots

## License

MIT — see [LICENSE](LICENSE).
