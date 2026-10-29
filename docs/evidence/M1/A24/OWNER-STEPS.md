# A24 owner steps

Nothing needs the owner's Mac, headset, ATAK device, Unreal or live access. The only step is review.

1. Open omos PR #556. Read `docs/proposals/2026-10-29-grammar-register/README.md` first. It is the index, ordered by what blocks activation.
2. Tick one ruling box per entry. Rule tier 1 first: ONT-P1, BUS-P2, ONT-P14, ONT-P10, ONT-P7, BUS-P1, CP-4.
3. Re-run the checker on any edited copy:
   `python scripts/check_proposal_subjects.py --self-test`
   Expected output: `self-test: 25 passed, 0 failed` and `result: PASS (0 errors)`.
4. Each approved item becomes its own follow-up PR: a stream CR edit, or a `CLAUDE.md` or doctrine line. Merging #556 changes nothing live.
