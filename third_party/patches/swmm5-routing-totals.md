# SWMM 5.2.4 routing totals bridge

M272 adds a small governed bridge to the vendored SWMM solver. The patch exports `massbal_getRoutingTotals` with a project-owned DTO, preserving raw `TRoutingTotals` components and a cumulative API lateral-inflow component in internal cubic feet. The C++ adapter converts the values at the ABI firewall to cubic metres and exposes a concrete-only `SwmmExternalNetObservation`.

The bridge is sampled after `SwmmEngine::step()`. The adapter retains the raw routing external net, reports API lateral volume separately, and exposes a deduplicated external net for SimDriver. Removing the API lateral component at this single boundary prevents CouplingLib surface-to-SWMM exchange from being counted as an external source.

The patch is tied to SWMM 5.2.4 (`VERSION 52004`) and must be revalidated if the vendored solver version or routing/mass-balance ABI changes.
