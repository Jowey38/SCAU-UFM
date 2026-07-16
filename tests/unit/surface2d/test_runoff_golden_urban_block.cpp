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
    double total_ponded_infiltration = 0.0;
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
            total_ponded_infiltration += diag.ponded_infiltration_volume;
            total_accepted += diag.roof_to_swmm_accepted_volume;
        }
    }

    // (2) Cumulative closure across the whole run.
    const double v1 = system_volume(state, dpm, geom);
    // In a closed box the surface storage h is fed by ground surface runoff and
    // roof overflow, and drained by ponded-h Green-Ampt infiltration (M247-F);
    // everything else is loss / leaves to pipe / held. This golden starts from
    // ponded water (lake-at-rest) on the pervious C0 cell, so ponded infiltration
    // is materially nonzero even at phi_t == 1: the subtracted
    // total_ponded_infiltration term is actively exercised, not near-zero.
    // (Ponded infiltration is zero only when surface_depth or pervious area is
    // zero, which is not the case here.)
    EXPECT_NEAR(v1 - v0,
                total_surface_added + total_overflow - total_ponded_infiltration,
                1.0e-8);

    // (3) Behavioral coverage: roof both drained to SWMM and overflowed to surface.
    EXPECT_GT(total_accepted, 0.0);
    EXPECT_GT(total_overflow, 0.0);

    // Post-rain phase (rate 0) contributed no gross rain: total equals the first two phases only.
    const double rained = (1.0e-4 * 5 + 5.0e-4 * 3) * config.dt * total_area;
    EXPECT_NEAR(total_rain, rained, 1.0e-9);
}

// M247-F mini-golden: phi_t < 1 on C0 with ZERO rain but nonzero ponded h. The
// ponded-h Green-Ampt path must drain h by exactly the pervious fraction of the
// ponded depth, and the phi_t-scaled surface storage that leaves equals the
// reported ponded_infiltration_volume diagnostic (the new conservation term).
TEST(GoldenUrbanBlock, LowPorosityInterfaceConservation) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 0.1);
    auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm.cells[0].phi_t = 0.4;
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);

    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.fields.impervious_fraction[i] = 0.0;
        runoff.fields.roof_fraction[i] = 0.0;
        runoff.rainfall_rate[i] = 0.0;
    }
    runoff.fields.pervious_fraction[0] = 0.1;
    runoff.lut.entries[0] = scau::surface2d::SoilParams{.k_s = 1.0, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1};

    const scau::surface2d::StepConfig config{.dt = 1.0, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const double before = dpm.cells[0].phi_t * 0.1 * geom.cell_areas[0];
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, runoff, roof, rs);
    ASSERT_FALSE(diag.rollback_required);
    const double after = dpm.cells[0].phi_t * state.cells[0].conserved.h * geom.cell_areas[0];

    EXPECT_NEAR(before - after, diag.ponded_infiltration_volume, 1.0e-9);
    // fully-infiltrated case: drained depth = h*pervious_fraction => h_after = h*(1 - pervious_fraction)
    EXPECT_NEAR(state.cells[0].conserved.h, 0.09, 1.0e-9);
    EXPECT_GE(state.cells[0].conserved.h, 0.0);
}

// M247-F stage-level coupling check: BOTH nonzero rain AND nonzero ponded h on
// the same pervious C0 cell with phi_t < 1. Asserts the per-cell surface-storage
// invariant phi_t*A*(h_after - h_before) == surface_added - ponded_infiltration,
// reading the totals from the returned diagnostics (only C0 generates runoff, so
// the cell-level and step-level totals coincide here). Rain is kept tiny and
// c_rollback lenient so the closed lake-at-rest step never rolls back.
TEST(GoldenUrbanBlock, MixedRainAndPondedConservation) {
    const auto mesh = scau::mesh::build_mixed_minimal_mesh();
    auto state = scau::surface2d::SurfaceState::hydrostatic_for_mesh(mesh, 1.0, 0.1);
    auto dpm = scau::surface2d::DpmFields::for_mesh(mesh);
    dpm.cells[0].phi_t = 0.4;
    const auto geom = scau::surface2d::GeometryCache::for_mesh(mesh);
    const auto boundary = scau::surface2d::BoundaryConditions::for_mesh(mesh);
    const auto sources = scau::surface2d::SourceTermFields::for_mesh(mesh);
    auto rs = scau::surface2d::RunoffState::for_mesh(mesh);
    auto runoff = scau::surface2d::RunoffStepInputs::for_mesh(mesh);
    auto roof = scau::surface2d::RoofStepInputs::for_mesh(mesh);

    for (std::size_t i = 0; i < mesh.cells.size(); ++i) {
        runoff.fields.pervious_fraction[i] = 0.0;
        runoff.fields.impervious_fraction[i] = 0.0;
        runoff.fields.roof_fraction[i] = 0.0;
        runoff.rainfall_rate[i] = 0.0;
    }
    runoff.fields.pervious_fraction[0] = 0.1;
    runoff.rainfall_rate[0] = 1.0e-4;  // tiny rain so the closed box does not roll back
    runoff.lut.entries[0] = scau::surface2d::SoilParams{.k_s = 1.0, .psi_f = 0.1, .theta_s = 0.4, .theta_i = 0.1};

    const scau::surface2d::StepConfig config{.dt = 1.0, .cfl_safety = 0.45, .c_rollback = 100.0, .h_min = 1.0e-8};
    const double h_before = state.cells[0].conserved.h;
    const auto diag = scau::surface2d::advance_one_step_cpu(mesh, state, config, dpm, boundary, sources, geom, runoff, roof, rs);
    ASSERT_FALSE(diag.rollback_required);
    const double h_after = state.cells[0].conserved.h;

    // Only C0 is a runoff cell, so the step-level surface_added / ponded
    // infiltration diagnostics equal C0's contribution.
    const double storage_delta = dpm.cells[0].phi_t * geom.cell_areas[0] * (h_after - h_before);
    EXPECT_NEAR(storage_delta,
                diag.surface_added_volume - diag.ponded_infiltration_volume,
                1.0e-9);
    EXPECT_GE(h_after, 0.0);
}
