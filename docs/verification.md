# Verification — five acceptance criteria

All five must pass for the 0 B claim to hold. Each criterion has a method and a
threshold. The claim is **binary**: 1 B in any direction → FAILED. There is no
"close to zero" acceptance interval.

## C1 · State-vector diff == 0 B

- **Method**: `gem5.opt sim/gem5/fs_diff.py --tests tests/ --expect-residue 0`.
  Full-system checkpoints: reset-exit snapshot vs test-exit snapshot, diffed
  over ℛ (domain list from `sv_manifest.yaml`).
- **Threshold**: 1000 tests, all nonzero-delta counts == 0.

## C2 · Full-zero hardware readback

- **Method**: `runner.py --dump` reads back every bank byte + ECC plane + ℛ
  registers via ILA/JTAG after each clear.
- **Threshold**: every byte == 0x00 and every syndrome == 0.
  0 nonzero bytes, 0 nonzero syndromes.

## C3 · Timing budget

- **Method**: ILA timestamps `ts_reset_ready`, `ts_clear_done` at 85 °C.
- **Threshold**: reset ≤ 2.4 µs, clear ≤ 1.30 ms (budget: 2.0 µs / 1.0486 ms
  with margins).

## C4 · Fault injection

- **Method**: `flash_tool.py --corrupt` flips 1 bit at 3 offsets × 100 trials;
  ILA PC trace tracks entry.
- **Threshold**: 300/300 images HALT before test entry —
  `sha_result == MISMATCH`, PC never enters 0x0000_1000 (0 escapes).

## C5 · Determinism / reproducibility

- **Method**: `runner.py --determinism` — same 1000-test set, 2 slots, 2 rounds
  each.
- **Threshold**: all runs report residue == 0 B and cycle-count traces are
  byte-identical across runs and slots.

## What this proves

- C1 proves the reset domain is **complete** (nothing survives reset).
- C2 proves the memory plane is **physically zero** (data + ECC).
- C3 proves the isolation fits the **turnover budget**.
- C4 proves the loader cannot be **bypassed or corrupted**.
- C5 proves the whole thing is **reproducible**, not a flake.

## Software-only precursors (this repo's first milestones)

Before hardware exists, two criteria are already checkable in simulation:

| Criterion | Software proxy |
|-----------|---------------|
| C1 | gem5 full-system diff (no hardware needed) |
| C5 | gem5 determinism across two simulated slots |

C2 (real readback), C3 (real timing) and C4 (real injection) are hardware-bound
and become runnable when the RTL prototype lands on a board.

## Current status

- [ ] C1 — gem5 diff (software, next milestone)
- [ ] C2 — hardware readback (requires RTL prototype)
- [ ] C3 — timing (requires board)
- [ ] C4 — fault injection (requires board)
- [ ] C5 — determinism (software first, then board)
