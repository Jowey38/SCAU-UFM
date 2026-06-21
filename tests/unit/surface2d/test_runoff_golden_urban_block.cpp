// M247-E golden case: a closed-box "urban block" of 3 cells (pervious /
// impervious / roof) driven by the roof-aware advance_one_step_cpu overload
// across three rainfall phases (rain -> peak/overflow -> post-rain). The golden
// assertion is the conservation law itself: per substep the audited gross rain
// equals the sum of all sinks, and the cumulative change in surface storage
// equals the only two sources that feed h (ground surface runoff + roof
// overflow). Behavioral coverage: roof drains to SWMM (accepted>0), roof
// overflows to surface (overflow>0), Green-Ampt cumulative infiltration is
// monotonic, and the post-rain phase adds no gross rain.
//
// This intentionally exercises the integrated hot path (HLLC flux + ground
// runoff + roof runoff + exchange + friction) on the minimal mixed mesh; a
// large real-mesh urban grid + the formal GoldenSuite harness remain follow-ups.

#include <vector>

#include <gtest/gtest.h>

#include "mesh/mesh.hpp"
#include "surface2d/boundary/conditions.hpp"
#include "surface2d/dpm/fields.hpp"
#include "surface2d/geometry/cache.hpp"
#include "surface2d/source_terms/fields.hpp"
#include "surface2d/source_terms/runoff/fields.hpp"
#include "surface2d/source_terms/runoff/roof_step.hpp"
#include "surface2d/source_terms/runoff/step_inputs.hpp"
#include "surface2d/state/state.hpp"
#include "surface2d/time_integration/step.hpp"

namespace {

using scau::surface2d::advance_one_step_cpu;
using scau::surface2d::BoundaryConditions;
using scau::surface2d::DpmFields;
using scau::surface2d::GeometryCache;
using scau::surface2d::RoofDrainageAcceptance;
using scau::surface2d::RoofDrainageIntent;
using scau::surface2d::RoofDrainageRejectionReason;
using scau::surface2d::RoofStepInputs;
using scau::surface2d::RunoffState;
using scau::surface2d::RunoffStepInputs;
using scau::surface2d::SourceTermFields;
using scau::surface2d::StepConfig;
using scau::surface2d::StepDiagnostics;
using scau::surface2d::SurfaceState;

double system_volume(const SurfaceState& state, const DpmFields& dpm, const GeometryCache& geom) {
    double v = 0.0;
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        v += state.cells[i].conserved.h * dpm.cells[i].phi_t * geom.cell_areas[i];
    }
    return v;
}

// Sum of all per-step sinks that the gross rain partitions into.
double sink_sum(const StepDiagnostics& d) {
    return d.surface_added_volume + d.infiltration_volume + d.abstraction_volume +
           d.depression_storage_delta_volume + d.roof_to_swmm_accepted_volume +
           d.roof_pending_delta_volume + d.roof_overflow_to_surface_volume;
}

struct Phase {
    double rate;
    int steps;
};

}  // namespace

TEST(GoldenUrbanBlock, ConservesAcrossRainPeakAndPostRainPhases) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    ASSERT_EQ(mesh.cells.size(), 3U);  // C0 quad, C1 tri, C2 tri

    auto state = SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 1.0);  // flat lake at rest
    const auto dpm = DpmFields::for_mesh(mesh);                        // phi_t = 1.0
    const auto geom = GeometryCache::for_mesh(mesh);
    const auto boundary = BoundaryConditions::for_mesh(mesh);          // closed (wall) box
    const auto sources = SourceTermFields::for_mesh(mesh);             // no manning, no exchange
    auto rs = RunoffState::for_mesh(mesh);

    auto runoff = RunoffStepInputs::for_mesh(mesh);
    auto roof = RoofStepInputs::for_mesh(mesh);
    // C0 = pervious ground (Green-Ampt), C1 = impervious ground, C2 = roof.
    runoff.fields.pervious_fraction = {1.0, 0.0, 0.0};
    runoff.fields.impervious_fraction = {0.0, 1.0, 0.0};
    runoff.fields.roof_fraction = {0.0, 0.0, 1.0};
    runoff.fields.initial_abstraction_capacity = {2.0e-5, 2.0e-5, 0.0};
    runoff.fields.depression_storage_capacity = {2.0e-5, 2.0e-5, 0.0};
    runoff.fields.roof_storage_capacity = {0.0, 0.0, 0.0};             // no roof buffer: leftover overflows
    // Roof C2 drains a tiny bit to SWMM node 0; the rest overflows to cell C0.
    roof.map.roof_drain_capacity = {0.0, 0.0, 1.0e-7};                 // m^3/s (tiny -> accepted>0, most overflows)
    roof.map.swmm_node_index = {-1, -1, 0};
    roof.map.roof_overflow_to_surface_cell_index = {-1, -1, 0};        // roof -> C0 surface
    roof.accept = [](const RoofDrainageIntent& in) {
        return RoofDrainageAcceptance{.requested_volume = in.requested_volume,
                                      .accepted_volume = in.requested_volume,
                                      .rejected_volume = 0.0,
                                      .rejection_reason = RoofDrainageRejectionReason::None};
    };

    // Tiny rain keeps the closed box near lake-at-rest (trivial CFL) so the run
    // never rolls back -- this golden tests runoff conservation, not CFL stability.
    const StepConfig config{.dt = 0.1, .cfl_safety = 0.45, .c_rollback = 1.0e4, .h_min = 1.0e-8};
    const std::vector<Phase> phases{{1.0e-4, 5}, {5.0e-4, 3}, {0.0, 5}};

    double total_rain = 0.0;
    double total_surface_added = 0.0;
    double total_overflow = 0.0;
    double total_accepted = 0.0;
    double last_f_inf = rs.cumulative_infiltration[0];
    const double v0 = system_volume(state, dpm, geom);

    double total_area = 0.0;
    for (const double a : geom.cell_areas) total_area += a;

    for (const auto& phase : phases) {
        for (auto& r : runoff.rainfall_rate) r = phase.rate;
        for (int s = 0; s < phase.steps; ++s) {
            const auto diag =
                advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, runoff, roof, rs);
            ASSERT_FALSE(diag.rollback_required);

            // (1) Per-substep mass closure: gross rain == sum of all sinks.
            EXPECT_NEAR(diag.rainfall_volume, sink_sum(diag), 1.0e-9);
            // Gross rain this step is exactly rate*dt*total_area (every cell receives rain).
            EXPECT_NEAR(diag.rainfall_volume, phase.rate * config.dt * total_area, 1.0e-9);

            // (4) Green-Ampt cumulative infiltration is monotonic non-decreasing.
            EXPECT_GE(rs.cumulative_infiltration[0] + 1.0e-15, last_f_inf);
            last_f_inf = rs.cumulative_infiltration[0];

            total_rain += diag.rainfall_volume;
            total_surface_added += diag.surface_added_volume;
            total_overflow += diag.roof_overflow_to_surface_volume;
            total_accepted += diag.roof_to_swmm_accepted_volume;
        }
    }

    // (2) Cumulative closure across the whole run.
    const double v1 = system_volume(state, dpm, geom);
    // In a closed box the only sources that change surface storage h are ground
    // surface runoff and roof overflow; everything else is loss / leaves to pipe / held.
    EXPECT_NEAR(v1 - v0, total_surface_added + total_overflow, 1.0e-8);

    // (3) Behavioral coverage: roof both drained to SWMM and overflowed to surface.
    EXPECT_GT(total_accepted, 0.0);
    EXPECT_GT(total_overflow, 0.0);

    // Post-rain phase (rate 0) contributed no gross rain: total equals the first two phases only.
    const double rained = (1.0e-4 * 5 + 5.0e-4 * 3) * config.dt * total_area;
    EXPECT_NEAR(total_rain, rained, 1.0e-9);
}
