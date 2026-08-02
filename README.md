# Zero-Residue Harness

**Cross-test isolation with a *certified* 0-byte residue — not an assertion, a measured fact.**

Agent harnesses promise that every test starts from a clean state. They don't. `fork/exec` boots the child with a warm CPU and inherited OS state — signal mask, open file descriptors, cwd, umask, rlimits, warm TLB/cache lines, touched heap pages. Every "cold" process carries **≥1 byte** of cross-test residue. `from-zero` is a claim, not a property.

Measured: ~217 B of architectural state + 16 MiB of COW-inherited pages per "cold" start. See `docs/residue-measurement.md`.

## Repo layout

- `tools/residue-audit` — measure the actual residue of a `fork/exec` cold start
- `docs/residue-domain.md` — the exact 0 B contract (ℛ)
- `docs/design.md` — Cold-Start Island: a per-test hardware-reset isolation fabric
- `docs/verification.md` — five acceptance criteria

## License

MIT
