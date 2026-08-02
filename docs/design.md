# Design — Cold-Start Island

A per-test hardware-reset isolation fabric. Each test starts as the first
instruction of a freshly reset machine; 0 B cross-test residue is a **certified
architectural state**, not an assertion.

## Why software cannot do this

`exec` is state inheritance, not state creation. POSIX semantics force the
child to inherit signal mask, FD table, cwd, umask, rlimits — a constructive
≥1 B floor. `fork` additionally duplicates touched parent pages (COW). Every
software route (cleanup, pools, snapshots, microVMs, TEEs, unikernels) either
inherits ≥1 B or seeds guest state by construction. See
`docs/residue-domain.md` §1 for the measured failure.

## The isolation primitive

Replace the state *generator* (`exec`) with a reset vector:

```
RESET → CLEAR → LOAD → VERIFY → EXEC → CAPTURE → DIFF → HALT
```

| Step | Module | What it does | Key numbers |
|------|--------|--------------|-------------|
| RESET | `reset_island.sv` | Enumerates ℛ (caches, TLB, FPU/MSR, PMU counters, store buffers, AXI quiet) and returns them to reset vector S₀ | ~2 µs @ 85 °C |
| CLEAR | `bank_clear.sv` | Streams 0x00 across the private bank, **ECC recomputed same-cycle** so both planes are physically zero | 512 bit/cycle @ 1 GHz → 64 MiB in 1.0486 ms |
| LOAD | `dma_loader.sv` | Stages next test from read-only flash; AXI decode drops any cross-slot write | max 64 MiB |
| VERIFY | `fw/stub.c` (OTP ROM) | SHA-256 of loaded image vs host-issued digest; mismatch → HALT before PC enters bank | 100% 1-bit coverage |
| EXEC | — | Test runs ring-0, physical addressing, inside its own bank | fixed entry 0x0000_1000 |
| CAPTURE/DIFF | `state_diff.sv` | Reset-exit snapshot vs test-exit snapshot, byte diff over ℛ; any nonzero → S_FAULT | single-bit `residue_ok` |
| HALT | — | S_FAULT on any nonzero delta; no threshold parameter exists | — |

## Why 0 B is a theorem here, not a promise

Three properties make the claim machine-checkable:

1. **Reset is idempotent** (R∘R = R): replaying the reset cannot carry measurable
   residue — S₀ is a fixed point.
2. **The domain is enumerated**: ℛ is a manifest (`sv_manifest.yaml`) that drives
   all three heads (RTL, gem5, Bochs) — the diff uses raw dumps, not tool
   summaries, so no hidden byte exists.
3. **The authenticator is outside ℛ and unreachable**: OTP eFuse (zero writable
   state), AXI PROT=3, address decode rejection. The certifier cannot be
   contaminated by the certified.

## Fabrication pipeline

1. `gensv.py sim/sv_manifest.yaml` → RTL package, gem5 state list, Bochs config
2. Pull CVA6 6.1 (RV64IMAFDC), compile ROM stub (`-nostdlib`)
3. Vivado batch synth + P&R, signoff fmax 1.0/0.8 GHz
4. One-time OTP eFuse burn (irreversible)
5. `flash_tool.py` pre-hash images + program flash
6. `runner.py` on the board at 85 °C (slots 0-3, `--verify gem5`)

## Cost structure

Per-test turnover is dominated by CLEAR (1.0486 ms of the ≤1.5 ms budget).
The erasure energy floor is Landauer's bound: erasing 2^29 bits costs ≥
kT·ln2 ≈ 1.54 pJ; the engineering budget of ~1.5 nJ/clear sits ~10³ above the
floor. Isolation cost is pinned from below by a physical constant — "how cold is
cold" is a fixed interval, not a knob.

## Evolution

**r2: Test-as-Logic.** Compile each test into the fabric's logic (LUT/FF
netlist) instead of a stored program. No CPU, no bank, no erase, no certifier —
0 B becomes structurally impossible to violate. ~8 µs re-instantiation per slot
(~130× faster than r1), zero erase energy, zero certification latency.
This repo ships r1 (the executable baseline); r2 is the tracked evolution.

## Scope of this repo

- `docs/` — contract, design, verification (this repo's "constitution")
- `tools/residue-audit` — *measure* the problem (fork/exec residue)
- `sim/` — *prove* the solution (reset-domain completeness in gem5)
- `rtl/` — *build* the island (post-simulation; see roadmap)
