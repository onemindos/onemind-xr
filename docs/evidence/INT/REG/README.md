# INT/REG: grammar register update (omos #556, branch docs/grammar-proposals)

Commit: 5de40817 (pushed; no force-push; PR stays draft).

Files changed in omos `docs/proposals/2026-10-29-grammar-register/`:
- `cross-reference.md`: PR table and mapping rows for #555, #557-#569, onemind-xr #4/#5, plus 7 new contradictions.
- `integration-resolutions.md` (new): INT-R1 params_hash, INT-R2 capabilities, INT-R3 CLIENT imports, INT-R4 svc.cot.send.
- `README.md`: activation-ordered index rebuilt (tiers 1-5), file list, subjects block.
- `ontology.md`: ONT-P7 correction note from #555 A22-P2.

Evidence:
- `check-register.txt`: `scripts/check_proposal_subjects.py --self-test` on the branch: self-test 25/25, 331 subjects, PASS.
- `check-referenced.txt`: the same checker over the subjects blocks of the D1 (#569), I1 (#567), I2 (#564) and UE (xr #4) files.
  Grammar checks pass; #564's file has 11 listing-completeness misses (wildcard spans not in its block). That is #564's file, not edited.
- `ci-local.txt`: gitleaks over the register folder (no leaks), no YAML changed, `git diff --check` clean.
- `gh-pr-checks.txt`: GitHub CI on #556 after the push (all pass).

Caveat: when this was written, none of the carrying PRs (#554, #552, #562, #563) had the INT-R1..R4 code on its head.
The register records each resolution and names its carrying PR. integration-resolutions.md says so.
