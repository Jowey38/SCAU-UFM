# SWMM spike test cases

This directory contains SCAU-UFM-authored `.inp` files for the standalone SWMM
5.2.x spike host. They are written from scratch for this repository and are not
copied from EPA or third-party example sets.

Cases per third-party-spike-design §5.1:

- `single_pipe.inp`: upstream constant inflow into one junction, one conduit,
  and a free outfall. It is the minimal lifecycle/state-IO sanity case for
  §3.1 lifecycle, §3.2 time advance, and §3.3 state reads.
- `manhole_overflow.inp`: shallow downstream manhole `J1` with a restricted
  outlet conduit and sustained upstream inflow. It backs the G8
  `swmm_single_pipe_surcharge` physical evidence path by making real SWMM
  `NODE_HEAD` / `NODE_OVERFLOW` transitions observable through the spike host.

The main repository CMake graph still does not build or gate real SWMM. Use
`spikes/swmm/CMakeLists.txt` with an external `SWMM_SOURCE_DIR` checkout to run
these cases.
