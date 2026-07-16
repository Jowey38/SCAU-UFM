# D-Flow FM spike test cases

This directory will hold `.mdu` files driving the D-Flow FM BMI spike host.
M29 does NOT place real `.mdu` files because Deltares example licensing must
be verified first (deferred to M30+ placement decision).

Required cases per third-party-spike-design §6.1:

- `single_reach.mdu`: single reach, upstream constant Q, downstream constant
  stage. Sanity case for §3.1-§3.3. Repository now includes an author-authored
  skeleton only; it is not yet a validated runtime case.
- `reach_with_weir.mdu`: reach with one fixed weir. Backs §3.3 structure
  state read and G11 `dflowfm_river_steady` physical basis.
