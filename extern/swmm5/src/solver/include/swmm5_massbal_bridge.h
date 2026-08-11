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

#ifdef __cplusplus
}
#endif

#endif
