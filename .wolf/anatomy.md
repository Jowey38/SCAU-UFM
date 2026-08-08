# anatomy.md

> M272 worktree local anatomy additions, 2026-08-07.

## .wolf/

- `buglog.json` — M272 build/audit bug records (~900 tok)
- `cerebrum.md` — project learnings including SWMM external-net bridge (~7000 tok)
- `memory.md` — chronological action log (~17000 tok)

## docs/superpowers/plans/

- `2026-08-07-m272-swmm-external-net.md` — M272 SWMM external-net implementation plan (~350 tok)

## extern/swmm5/src/solver/include/

- `swmm5_massbal_bridge.h` — ABI-stable SWMM routing totals snapshot (~220 tok)

## superpowers/specs/

- `2026-08-07-m272-swmm-external-net-evidence.md` — M272 implementation and verification evidence (~350 tok)

## tests/golden/swmm_external_net/

- `CMakeLists.txt` — G26 non-gating Golden registration (~100 tok)
- `test_swmm_external_net.cpp` — concrete SWMM external-net evidence (~260 tok)

## tests/unit/coupling/

- `test_coupling_swmm_external_net.cpp` — SWMM routing component and API-lateral unit coverage (~300 tok)

## third_party/patches/

- `swmm5-routing-totals.md` — governed SWMM 5.2.4 bridge patch record (~250 tok)
