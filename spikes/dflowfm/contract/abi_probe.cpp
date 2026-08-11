#include <cstddef>
#include <type_traits>

#include "dflowfm_water_balance_v1.h"

static_assert(std::is_standard_layout_v<scau_dflowfm_water_balance_v1>);
static_assert(std::is_trivially_copyable_v<scau_dflowfm_water_balance_v1>);
static_assert(offsetof(scau_dflowfm_water_balance_v1, valid_components) == 8U);
static_assert(offsetof(scau_dflowfm_water_balance_v1, current_time_seconds) == 16U);
static_assert(sizeof(scau_dflowfm_water_balance_v1) == 168U);

int main() {
    return SCAU_DFLOWFM_WATER_BALANCE_ABI_VERSION == 1U ? 0 : 1;
}
