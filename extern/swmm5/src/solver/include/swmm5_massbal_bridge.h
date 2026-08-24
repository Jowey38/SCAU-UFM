#ifndef SWMM5_MASSBAL_BRIDGE_H
#define SWMM5_MASSBAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    double dwInflow;
    double wwInflow;
    double gwInflow;
    double iiInflow;
    double exInflow;
    double apiInflow;
    double flooding;
    double outflow;
    double evapLoss;
    double seepLoss;
    double initStorage;
    double finalStorage;
} SwmmRoutingTotalsSnapshot;

/*
 * Copies the current SWMM routing continuity totals into an ABI-stable DTO.
 * All values use SWMM internal cubic feet and are valid while the project is
 * open. The bridge is read-only and returns 0 on success, non-zero otherwise.
 */
int massbal_getRoutingTotals(SwmmRoutingTotalsSnapshot* totals);

/*
 * Copies one node's cumulative massbal boundary volume registers (ft3,
 * accumulated since swmm_start). For an OUTFALL node the total-outflow
 * register is the emitted boundary volume; stage-driven reverse (backwater)
 * boundary flow appears as NEGATIVE deltas of the same register (M278).
 * Read-only; returns 0 on success, non-zero for an invalid destination or
 * node index.
 */
int massbal_getNodeTotalInflow(int nodeIndex, double* volume);
int massbal_getNodeTotalOutflow(int nodeIndex, double* volume);

#ifdef __cplusplus
}
#endif

#endif
