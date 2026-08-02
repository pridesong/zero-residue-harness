# Residue Domain ℛ — the exact contract of "0 B"

`0 B cross-test residue` is meaningless without a precise domain of measurement.
This document is the contract: **what counts as residue, what does not, and how
 each byte is verified.** Every artifact in this repository (auditor, simulator,
RTL) must reference this document. If the contract changes, everything changes
with it.

## Definition

Let ℛ be the set of all architectural state that a test can observe or inherit.
A harness achieves **0 B residue** iff, for every byte in ℛ:

```
state_after(test_N exit) == state_at(cold start of test_N+1)
```

byte-for-byte, provable by a state-vector diff. The comparison baseline is a
**machine-defined cold vector** (e.g. a reset vector), not a POSIX contract.

## The complete enumeration

### Software-visible state inherited by `fork`/`exec` (the current failure mode)

| # | Item | Typical size | POSIX semantics | Measurable in userland? |
|---|------|-------------|-----------------|------------------------|
| 1 | Signal mask | 8 B (sigset_t) | preserved by exec | ✅ `sigprocmask` round-trip |
| 2 | Non-CLOEXEC file descriptors | ~64 B/entry (kernel table) | preserved by exec | ✅ `fcntl(F_GETFD)` |
| 3 | Working directory | path length | preserved by exec | ✅ `getcwd` |
| 4 | umask | 2 B | preserved by exec | ✅ `umask()` |
| 5 | rlimits | ~320 B (struct rlimit × 16) | preserved by exec | ✅ `getrlimit` |
| 6 | Environment + argv | explicit, but present | passed to exec | ✅ |
| 7 | Touched heap/stack pages (via fork) | n × 4096 B | COW-inherited from parent | ✅ `mincore` / RSS accounting |
| 8 | RNG state / ASLR bookkeeping | kernel-side | inherited pattern | ⚠️ indirect only |
| 9 | TLB / cache warmth | architecture-level | inherited on fork, cold on exec | ❌ micro-benchmark proxy |

### Architectural state covered by the Cold-Start Island design (target ℛ)

| # | Item | Reset mechanism | Verifiable? |
|---|------|----------------|-------------|
| A | GPR (32) + FPR (32) + FCSR/FRM | RESET | ✅ state-vector diff |
| B | All CSRs (mcycle, minstret, mhpmcounter\*) | RESET (counters zeroed) | ✅ |
| C | L1 I/D cache tags + data | invalidate port | ✅ gem5 checkpoint |
| D | Full TLB | shootdown | ✅ gem5 checkpoint |
| E | Store buffers | drain + settle ≥64 cycles | ✅ gem5 checkpoint |
| F | Bank (data) + ECC plane | stream-clear to 0x00, ECC recomputed same-cycle | ✅ readback + syndrome check |
| G | DMA descriptor registers | per-test zero on reset | ✅ readback |
| H | Slot control/status registers | per-test zero on reset | ✅ readback |

### Outside ℛ (the honest boundary)

- **Authenticators** (ROM stub, clear engine, reset sequencer): physically
  unreachable from the test (OTP, AXI PROT=3, address decode rejection).
- **Flash / OTP sources**: read-only input streams, not state carriers.

The boundary is documented here so that a future redefinition (e.g. "include
authenticator state in ℛ") is a *deliberate contract change*, not an accident.

## Verifiability principle

Every byte in ℛ must be verifiable by at least one independent mechanism.
If a byte cannot be verified, the 0 B claim does not cover it — say so.

| Mechanism | Coverage | Independent of |
|-----------|----------|----------------|
| State-vector diff (reset-exit vs test-exit) | A, B, C, D, E | simulator (gem5) |
| Hardware readback (ILA/JTAG) | F, G, H | software dumps |
| ECC syndrome check | F | data plane |

## Change control

Any change to this table is a breaking contract change:
bump the version, update `docs/design.md` and `docs/verification.md` together,
and re-run the full acceptance suite.
