#ifndef SCAU_DFLOWFM_WATER_BALANCE_V1_H
#define SCAU_DFLOWFM_WATER_BALANCE_V1_H

/*
 * SCAU-UFM project-authored native water-balance ABI contract snapshot.
 *
 * The exporting bridge (spikes/dflowfm/bridge/dflowfm_water_balance_bridge.F90)
 * is compiled INTO the governed external dflowfm.dll build (M274/M275). This
 * header is the C-side contract snapshot, mirroring extern/dflowfm/include's
 * role for bmi.h: the production adapter does NOT include it; it re-declares
 * the layout privately and resolves `dflowfm_get_water_balance_v1` by name at
 * runtime. If this contract is revised, bump the ABI version and re-verify the
 * adapter mirror in libs/coupling/river/src/dflowfm_adapter/dflowfm_engine.cpp.
 *
 * Cumulative semantics (M274 contract evidence):
 * - All *_m3 components are engine-native cumulative volumes since the LAST
 *   `initialize` call. They reset to zero on every initialize, including a
 *   restart reload. Consumers MUST re-baseline across an initialize boundary.
 * - current_time_seconds is the engine-native time (time1).
 * - storage_m3 is whole-domain storage (vol1tot).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCAU_DFLOWFM_WATER_BALANCE_ABI_VERSION 1u

#define SCAU_DFLOWFM_WB_BOUNDARY 0x0000000000000001ULL
#define SCAU_DFLOWFM_WB_LATERAL 0x0000000000000002ULL
#define SCAU_DFLOWFM_WB_SOURCE 0x0000000000000004ULL
#define SCAU_DFLOWFM_WB_QEXT 0x0000000000000008ULL
#define SCAU_DFLOWFM_WB_RAIN_EVAP 0x0000000000000010ULL
#define SCAU_DFLOWFM_WB_GROUNDWATER 0x0000000000000020ULL
#define SCAU_DFLOWFM_WB_STORAGE 0x0000000000000040ULL
#define SCAU_DFLOWFM_WB_VOLUME_ERROR 0x0000000000000080ULL

struct scau_dflowfm_water_balance_v1 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t valid_components;
    double current_time_seconds;
    double storage_m3;
    double volume_error_cumulative_m3;
    double boundary_in_m3;
    double boundary_out_m3;
    double lateral_1d_in_m3;
    double lateral_1d_out_m3;
    double lateral_2d_in_m3;
    double lateral_2d_out_m3;
    double source_in_m3;
    double source_out_m3;
    double qext_1d_in_m3;
    double qext_1d_out_m3;
    double qext_2d_in_m3;
    double qext_2d_out_m3;
    double rain_in_m3;
    double evaporation_out_m3;
    double groundwater_in_m3;
    double groundwater_out_m3;
};

int dflowfm_get_water_balance_v1(
    struct scau_dflowfm_water_balance_v1* out,
    uint32_t out_size);

#ifdef __cplusplus
}
#endif

#endif
