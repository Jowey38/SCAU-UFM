// G9 cpu_gpu_deterministic_match (M266 fixture matrix, M280 pattern, M284
// implementation).
//
// Runs the CPU reference backend and the deterministic CUDA backend over the
// full M266 fixture matrix and locks:
//   - h and eta bitwise equal (EXPECT_EQ on doubles);
//   - hu/hv bitwise equal except on Manning-friction-active fixtures, where
//     device pow may differ by ulps (locked at 1e-12 absolute);
//   - raw max_cell_cfl, rollback state, per-edge and per-cell diagnostics
//     bitwise equal (identical association order by construction);
//   - accumulated volume diagnostics within 1e-12 (CPU sums sequentially,
//     the CUDA backend uses the fixed-order M280 block tree);
//   - repeated CUDA runs bitwise identical (state vector memcmp);
//   - device snapshot/restore roundtrip bitwise identical.
//
// The golden SKIPS when the CUDA backend is not compiled in or no device is
// present (hosted CI lanes); the governed GPU host is the enforcing
// execution, exactly like the real-engine goldens.

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "mesh/mesh.hpp"
#include "surface2d/backend.hpp"
#include "surface2d/state/state.hpp"

#if defined(SCAU_SURFACE2D_HAS_CUDA)

#include "stcf/case_profiles.hpp"
#include "stcf/io_netcdf.hpp"
#include "surface2d/backends/cuda_backend.hpp"
#include "surface2d/stcf_bridge/load_case.hpp"

namespace {

namespace s2 = scau::surface2d;

struct Fixture {
    std::string name;
    scau::mesh::Mesh mesh;
    s2::SurfaceState state;
    s2::StepConfig config;
    s2::DpmFields dpm;
    s2::BoundaryConditions boundary;
    s2::SourceTermFields sources;
    bool has_runoff{false};
    bool has_roof{false};
    s2::RunoffStepInputs runoff_inputs;
    s2::RunoffState runoff_state;
    s2::RoofStepInputs roof_inputs;
    // Manning friction is the only stage using pow (ulp-level device libm
    // difference); everything else is locked bitwise.
    bool bitwise_momentum{true};
};

struct RunResult {
    s2::SurfaceState state;
    s2::RunoffState runoff_state;
    s2::StepDiagnostics diagnostics;
};

RunResult run_backend(s2::BackendKind kind, const Fixture& fixture) {
    RunResult result;
    result.state = fixture.state;
    result.runoff_state = fixture.runoff_state;
    const auto geometry = s2::GeometryCache::for_mesh(fixture.mesh);
    if (fixture.has_roof) {
        result.diagnostics = s2::advance_one_step(
            kind, fixture.mesh, result.state, fixture.config, fixture.dpm,
            fixture.boundary, fixture.sources, geometry,
            fixture.runoff_inputs, fixture.roof_inputs, result.runoff_state);
    } else if (fixture.has_runoff) {
        result.diagnostics = s2::advance_one_step(
            kind, fixture.mesh, result.state, fixture.config, fixture.dpm,
            fixture.boundary, fixture.sources, geometry,
            fixture.runoff_inputs, result.runoff_state);
    } else {
        result.diagnostics = s2::advance_one_step(
            kind, fixture.mesh, result.state, fixture.config, fixture.dpm,
            fixture.boundary, fixture.sources, geometry);
    }
    return result;
}

Fixture base_fixture(const std::string& name) {
    Fixture fixture;
    fixture.name = name;
    fixture.mesh = scau::mesh::build_mixed_minimal_mesh();
    fixture.state = s2::SurfaceState::hydrostatic_for_mesh(fixture.mesh, 1.0, 1.0);
    fixture.config.dt = 0.01;
    fixture.config.c_rollback = 100.0;
    fixture.dpm = s2::DpmFields::for_mesh(fixture.mesh);
    fixture.boundary = s2::BoundaryConditions::for_mesh(fixture.mesh);
    fixture.sources = s2::SourceTermFields::for_mesh(fixture.mesh);
    return fixture;
}

void set_moving_state(Fixture& fixture) {
    for (std::size_t c = 0; c < fixture.state.cells.size(); ++c) {
        auto& cell = fixture.state.cells[c];
        cell.conserved.h = 1.0 + 0.05 * static_cast<double>(c % 4);
        cell.eta = cell.conserved.h;  // flat bed z_b = 0
        cell.conserved.hu = 0.3 - 0.1 * static_cast<double>(c % 3);
        cell.conserved.hv = -0.2 + 0.07 * static_cast<double>(c % 5);
    }
}

s2::SoilParams test_soil() {
    s2::SoilParams soil;
    soil.k_s = 1.0e-6;
    soil.psi_f = 0.11;
    soil.theta_s = 0.42;
    soil.theta_i = 0.15;
    return soil;
}

void configure_runoff(Fixture& fixture, double rainfall_rate) {
    fixture.has_runoff = true;
    fixture.runoff_inputs = s2::RunoffStepInputs::for_mesh(fixture.mesh);
    fixture.runoff_state = s2::RunoffState::for_mesh(fixture.mesh);
    fixture.runoff_inputs.lut.entries.clear();
    fixture.runoff_inputs.lut.entries.push_back(test_soil());
    for (std::size_t c = 0; c < fixture.mesh.cells.size(); ++c) {
        fixture.runoff_inputs.rainfall_rate[c] = rainfall_rate;
        fixture.runoff_inputs.fields.pervious_fraction[c] = 0.6;
        fixture.runoff_inputs.fields.impervious_fraction[c] = 0.4;
        fixture.runoff_inputs.fields.roof_fraction[c] = 0.0;
        fixture.runoff_inputs.fields.initial_abstraction_capacity[c] = 2.0e-5;
        fixture.runoff_inputs.fields.depression_storage_capacity[c] = 5.0e-5;
        fixture.runoff_inputs.fields.soil_type[c] = 0;
    }
}

std::vector<Fixture> build_matrix() {
    std::vector<Fixture> matrix;

    // 1. Hydrostatic lake at rest.
    matrix.push_back(base_fixture("lake_at_rest"));

    // 2. Static phi_t jump at rest (WB pairing).
    {
        Fixture fixture = base_fixture("phi_t_jump_static");
        for (std::size_t c = 0; c < fixture.dpm.cells.size(); ++c) {
            fixture.dpm.cells[c].phi_t = (c % 2 == 0) ? 1.0 : 0.4;
        }
        matrix.push_back(std::move(fixture));
    }

    // 3. Phi_c / phi_e_n jump with a moving state.
    {
        Fixture fixture = base_fixture("phi_e_n_jump");
        set_moving_state(fixture);
        for (std::size_t e = 0; e < fixture.dpm.edges.size(); ++e) {
            fixture.dpm.edges[e].phi_e_n = (e % 2 == 0) ? 1.0 : 0.5;
        }
        matrix.push_back(std::move(fixture));
    }

    // 4. Hard/soft blockage.
    {
        Fixture fixture = base_fixture("hard_soft_blockage");
        set_moving_state(fixture);
        for (std::size_t e = 0; e < fixture.dpm.edges.size(); ++e) {
            if (e % 3 == 0) {
                fixture.dpm.edges[e].omega_edge = 0.0;  // hard block
            } else if (e % 3 == 1) {
                fixture.dpm.edges[e].omega_edge = 5.0e-5;  // soft block
            }
        }
        matrix.push_back(std::move(fixture));
    }

    // 5. Wet/dry and near-dry donor.
    {
        Fixture fixture = base_fixture("wet_dry_near_dry");
        for (std::size_t c = 0; c < fixture.state.cells.size(); ++c) {
            auto& cell = fixture.state.cells[c];
            if (c % 3 == 0) {
                cell.conserved.h = 0.0;
            } else if (c % 3 == 1) {
                cell.conserved.h = 5.0e-9;  // below h_min = 1e-8: near-dry donor
            } else {
                cell.conserved.h = 0.5;
                cell.conserved.hu = 0.2;
            }
            cell.eta = cell.conserved.h;  // flat bed
        }
        matrix.push_back(std::move(fixture));
    }

    // 6. Rainfall, infiltration and friction.
    {
        Fixture fixture = base_fixture("rain_infiltration_friction");
        for (auto& cell : fixture.state.cells) {
            cell.conserved.h = 0.05;
            cell.eta = 0.05;
            cell.conserved.hu = 0.01;
        }
        configure_runoff(fixture, 2.0e-5);
        for (std::size_t c = 0; c < fixture.mesh.cells.size(); ++c) {
            fixture.sources.manning_n[c] = 0.03;
        }
        fixture.bitwise_momentum = false;  // Manning pow: 1e-12 lock
        matrix.push_back(std::move(fixture));
    }

    // 7. Coupling exchange and Q_limit-applied volume (drain clamped at -h).
    {
        Fixture fixture = base_fixture("coupling_exchange_applied");
        fixture.sources.exchange_volume[0] = 0.7;
        if (fixture.mesh.cells.size() > 1) {
            fixture.sources.exchange_volume[1] = -1000.0;  // clamp at -h
        }
        matrix.push_back(std::move(fixture));
    }

    // 8. Runoff and roof overflow.
    {
        Fixture fixture = base_fixture("runoff_roof_overflow");
        for (auto& cell : fixture.state.cells) {
            cell.conserved.h = 0.02;
            cell.eta = 0.02;
        }
        configure_runoff(fixture, 5.0e-5);
        for (std::size_t c = 0; c < fixture.mesh.cells.size(); ++c) {
            fixture.runoff_inputs.fields.pervious_fraction[c] = 0.4;
            fixture.runoff_inputs.fields.impervious_fraction[c] = 0.3;
            fixture.runoff_inputs.fields.roof_fraction[c] = 0.3;
            // No roof abstraction and a tiny pending cap so the partial
            // acceptance forces overflow through the mapped surface cell.
            fixture.runoff_inputs.fields.roof_abstraction_capacity[c] = 0.0;
            fixture.runoff_inputs.fields.roof_storage_capacity[c] = 1.0e-8;
        }
        fixture.has_roof = true;
        fixture.roof_inputs = s2::RoofStepInputs::for_mesh(fixture.mesh);
        const std::size_t cell_count = fixture.mesh.cells.size();
        for (std::size_t c = 0; c < cell_count; ++c) {
            fixture.roof_inputs.map.roof_drain_capacity[c] = 1.0e-6;
            fixture.roof_inputs.map.swmm_node_index[c] = static_cast<int>(c);
            fixture.roof_inputs.map.roof_overflow_to_surface_cell_index[c] =
                static_cast<int>((c + 1) % cell_count);
            fixture.roof_inputs.map.roof_to_surface_fraction[c] = 1.0;
        }
        // Deterministic partial acceptance keeps rejected volume pending and
        // forces overflow through the tiny roof storage capacity.
        fixture.roof_inputs.accept = [](const s2::RoofDrainageIntent& intent) {
            s2::RoofDrainageAcceptance acceptance;
            acceptance.requested_volume = intent.requested_volume;
            acceptance.accepted_volume = 0.25 * intent.requested_volume;
            return acceptance;
        };
        matrix.push_back(std::move(fixture));
    }

    // 9. CVC flag off and on (same jump fixture, both flag states).
    for (const bool enable_cvc : {false, true}) {
        Fixture fixture = base_fixture(enable_cvc ? "cvc_on" : "cvc_off");
        set_moving_state(fixture);
        for (std::size_t c = 0; c < fixture.dpm.cells.size(); ++c) {
            fixture.dpm.cells[c].phi_t = (c % 2 == 0) ? 1.0 : 0.8;
        }
        fixture.config.enable_cvc_spatial_phi_t_correction = enable_cvc;
        matrix.push_back(std::move(fixture));
    }

    // 10. Rollback and diagnostics reset (raw max_cell_cfl > C_rollback).
    {
        Fixture fixture = base_fixture("rollback_reset");
        set_moving_state(fixture);
        fixture.config.dt = 1000.0;
        fixture.config.c_rollback = 1.0;
        fixture.config.enable_cvc_spatial_phi_t_correction = true;
        for (std::size_t c = 0; c < fixture.dpm.cells.size(); ++c) {
            fixture.dpm.cells[c].phi_t = (c % 2 == 0) ? 1.0 : 0.8;
        }
        matrix.push_back(std::move(fixture));
    }

    // 11. File-driven mixed tri/quad STCF case (write -> strict load -> step).
    {
        Fixture fixture;
        fixture.name = "stcf_file_mixed_case";
        const auto case_path =
            std::filesystem::path(testing::TempDir()) / "g9_mixed_minimal.stcf.nc";
        scau::stcf::write_stcf_case(case_path, scau::stcf::make_mixed_minimal_case());
        auto loaded = s2::load_surface2d_case(case_path);
        fixture.mesh = std::move(loaded.mesh);
        fixture.dpm = std::move(loaded.dpm_fields);
        fixture.sources = std::move(loaded.source_fields);
        fixture.boundary = s2::BoundaryConditions::for_mesh(fixture.mesh);
        fixture.state = s2::SurfaceState::for_mesh(fixture.mesh);
        for (std::size_t c = 0; c < fixture.state.cells.size(); ++c) {
            const double z_b = loaded.bed_elevations[c];
            const double eta = 1.2;
            auto& cell = fixture.state.cells[c];
            cell.conserved.h = eta > z_b ? eta - z_b : 0.0;
            cell.eta = z_b + cell.conserved.h;
            cell.conserved.hu = 0.02;
        }
        fixture.config.dt = 0.01;
        fixture.config.c_rollback = 100.0;
        matrix.push_back(std::move(fixture));
    }

    // 12. Boundary kinds (DischargeInflow + WaterLevel + Open on top of Wall).
    {
        Fixture fixture = base_fixture("boundary_kinds");
        set_moving_state(fixture);
        const auto geometry = s2::GeometryCache::for_mesh(fixture.mesh);
        fixture.boundary.discharge_per_width.assign(fixture.mesh.edges.size(), 0.0);
        fixture.boundary.water_level.assign(fixture.mesh.edges.size(), 0.0);
        int boundary_seen = 0;
        for (std::size_t e = 0; e < fixture.mesh.edges.size(); ++e) {
            if (geometry.edge_cells[e].is_internal()) {
                continue;
            }
            switch (boundary_seen % 3) {
                case 0:
                    fixture.boundary.edges[e] = s2::BoundaryKind::DischargeInflow;
                    fixture.boundary.discharge_per_width[e] = 0.05;
                    break;
                case 1:
                    fixture.boundary.edges[e] = s2::BoundaryKind::WaterLevel;
                    fixture.boundary.water_level[e] = 1.08;
                    break;
                default:
                    fixture.boundary.edges[e] = s2::BoundaryKind::Open;
                    break;
            }
            ++boundary_seen;
        }
        matrix.push_back(std::move(fixture));
    }

    return matrix;
}

void expect_state_match(const Fixture& fixture, const RunResult& cpu, const RunResult& cuda) {
    ASSERT_EQ(cpu.state.cells.size(), cuda.state.cells.size());
    for (std::size_t c = 0; c < cpu.state.cells.size(); ++c) {
        EXPECT_EQ(cpu.state.cells[c].conserved.h, cuda.state.cells[c].conserved.h)
            << fixture.name << " h mismatch at cell " << c;
        EXPECT_EQ(cpu.state.cells[c].eta, cuda.state.cells[c].eta)
            << fixture.name << " eta mismatch at cell " << c;
        if (fixture.bitwise_momentum) {
            EXPECT_EQ(cpu.state.cells[c].conserved.hu, cuda.state.cells[c].conserved.hu)
                << fixture.name << " hu mismatch at cell " << c;
            EXPECT_EQ(cpu.state.cells[c].conserved.hv, cuda.state.cells[c].conserved.hv)
                << fixture.name << " hv mismatch at cell " << c;
        } else {
            EXPECT_NEAR(cpu.state.cells[c].conserved.hu, cuda.state.cells[c].conserved.hu, 1.0e-12)
                << fixture.name << " hu beyond 1e-12 at cell " << c;
            EXPECT_NEAR(cpu.state.cells[c].conserved.hv, cuda.state.cells[c].conserved.hv, 1.0e-12)
                << fixture.name << " hv beyond 1e-12 at cell " << c;
        }
    }
}

void expect_runoff_state_match(const Fixture& fixture, const RunResult& cpu, const RunResult& cuda) {
    if (!fixture.has_runoff) {
        return;
    }
    const auto& a = cpu.runoff_state;
    const auto& b = cuda.runoff_state;
    for (std::size_t c = 0; c < a.cumulative_infiltration.size(); ++c) {
        EXPECT_EQ(a.cumulative_infiltration[c], b.cumulative_infiltration[c]) << fixture.name << " cell " << c;
        EXPECT_EQ(a.ponding_time[c], b.ponding_time[c]) << fixture.name << " cell " << c;
        EXPECT_EQ(a.abstraction_filled[c], b.abstraction_filled[c]) << fixture.name << " cell " << c;
        EXPECT_EQ(a.depression_storage_filled[c], b.depression_storage_filled[c]) << fixture.name << " cell " << c;
        EXPECT_EQ(a.roof_abstraction_filled[c], b.roof_abstraction_filled[c]) << fixture.name << " cell " << c;
        EXPECT_EQ(a.roof_pending_volume[c], b.roof_pending_volume[c]) << fixture.name << " cell " << c;
    }
}

void expect_diagnostics_match(const Fixture& fixture, const s2::StepDiagnostics& cpu, const s2::StepDiagnostics& cuda) {
    constexpr double kVolumeTolerance = 1.0e-12;  // locked (tolerances.md G9)

    EXPECT_EQ(cpu.cell_count, cuda.cell_count) << fixture.name;
    EXPECT_EQ(cpu.edge_count, cuda.edge_count) << fixture.name;
    EXPECT_EQ(cpu.max_cell_cfl, cuda.max_cell_cfl) << fixture.name << " raw max_cell_cfl";
    EXPECT_EQ(cpu.rollback_required, cuda.rollback_required) << fixture.name;

    EXPECT_NEAR(cpu.rainfall_volume, cuda.rainfall_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.surface_added_volume, cuda.surface_added_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.ponded_infiltration_volume, cuda.ponded_infiltration_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.infiltration_volume, cuda.infiltration_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.abstraction_volume, cuda.abstraction_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.depression_storage_delta_volume, cuda.depression_storage_delta_volume, kVolumeTolerance)
        << fixture.name;
    EXPECT_NEAR(cpu.roof_to_swmm_requested_volume, cuda.roof_to_swmm_requested_volume, kVolumeTolerance)
        << fixture.name;
    EXPECT_NEAR(cpu.roof_to_swmm_accepted_volume, cuda.roof_to_swmm_accepted_volume, kVolumeTolerance)
        << fixture.name;
    EXPECT_NEAR(cpu.roof_to_swmm_rejected_volume, cuda.roof_to_swmm_rejected_volume, kVolumeTolerance)
        << fixture.name;
    EXPECT_NEAR(cpu.roof_pending_delta_volume, cuda.roof_pending_delta_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.roof_overflow_to_surface_volume, cuda.roof_overflow_to_surface_volume, kVolumeTolerance)
        << fixture.name;
    EXPECT_EQ(cpu.missing_roof_overflow_target, cuda.missing_roof_overflow_target) << fixture.name;
    EXPECT_NEAR(cpu.exchange_volume, cuda.exchange_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.boundary_inflow_volume, cuda.boundary_inflow_volume, kVolumeTolerance) << fixture.name;

    EXPECT_EQ(cpu.count_phi_t_jump_events, cuda.count_phi_t_jump_events) << fixture.name;
    EXPECT_NEAR(cpu.cvc_mass_correction_volume, cuda.cvc_mass_correction_volume, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.cvc_momentum_correction_x, cuda.cvc_momentum_correction_x, kVolumeTolerance) << fixture.name;
    EXPECT_NEAR(cpu.cvc_momentum_correction_y, cuda.cvc_momentum_correction_y, kVolumeTolerance) << fixture.name;
    EXPECT_EQ(cpu.max_cvc_storage_residual_after, cuda.max_cvc_storage_residual_after) << fixture.name;

    ASSERT_EQ(cpu.cells.size(), cuda.cells.size()) << fixture.name;
    for (std::size_t c = 0; c < cpu.cells.size(); ++c) {
        EXPECT_EQ(cpu.cells[c].mass_residual, cuda.cells[c].mass_residual)
            << fixture.name << " cell residual " << c;
        EXPECT_EQ(cpu.cells[c].momentum_residual.x, cuda.cells[c].momentum_residual.x)
            << fixture.name << " cell residual x " << c;
        EXPECT_EQ(cpu.cells[c].momentum_residual.y, cuda.cells[c].momentum_residual.y)
            << fixture.name << " cell residual y " << c;
    }
    ASSERT_EQ(cpu.edges.size(), cuda.edges.size()) << fixture.name;
    for (std::size_t e = 0; e < cpu.edges.size(); ++e) {
        EXPECT_EQ(cpu.edges[e].mass_flux, cuda.edges[e].mass_flux) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].momentum_flux_n, cuda.edges[e].momentum_flux_n) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].momentum_x, cuda.edges[e].momentum_x) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].momentum_y, cuda.edges[e].momentum_y) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].pressure_pairing, cuda.edges[e].pressure_pairing) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].s_phi_t, cuda.edges[e].s_phi_t) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].wb_pressure, cuda.edges[e].wb_pressure) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].s_topo, cuda.edges[e].s_topo) << fixture.name << " edge " << e;
        EXPECT_EQ(cpu.edges[e].residual, cuda.edges[e].residual) << fixture.name << " edge " << e;
    }
}

bool states_bitwise_equal(const s2::SurfaceState& a, const s2::SurfaceState& b) {
    if (a.cells.size() != b.cells.size()) return false;
    return a.cells.empty()
        || std::memcmp(a.cells.data(), b.cells.data(), a.cells.size() * sizeof(s2::CellState)) == 0;
}

bool cuda_ready() {
    return s2::query_backend_capabilities(s2::BackendKind::cuda_deterministic).available;
}

}  // namespace

TEST(G9CpuGpuDeterministicMatch, RecordsGovernedDevice) {
    if (!cuda_ready()) {
        GTEST_SKIP() << "deterministic CUDA backend unavailable on this host";
    }
    const auto info = s2::cuda_backend_device_info();
    RecordProperty("device_name", info.device_name);
    RecordProperty("compute_capability",
                   std::to_string(info.compute_capability_major) + "." +
                   std::to_string(info.compute_capability_minor));
    RecordProperty("driver_version", info.driver_version);
    RecordProperty("runtime_version", info.runtime_version);
    EXPECT_GE(info.compute_capability_major, 6);
}

TEST(G9CpuGpuDeterministicMatch, FixtureMatrixMatchesCpuReference) {
    if (!cuda_ready()) {
        GTEST_SKIP() << "deterministic CUDA backend unavailable on this host";
    }
    for (const auto& fixture : build_matrix()) {
        SCOPED_TRACE(fixture.name);
        const auto cpu = run_backend(s2::BackendKind::cpu_reference, fixture);
        const auto cuda = run_backend(s2::BackendKind::cuda_deterministic, fixture);
        expect_state_match(fixture, cpu, cuda);
        expect_runoff_state_match(fixture, cpu, cuda);
        expect_diagnostics_match(fixture, cpu.diagnostics, cuda.diagnostics);
        if (fixture.name == "rollback_reset") {
            EXPECT_TRUE(cpu.diagnostics.rollback_required);
            EXPECT_TRUE(cuda.diagnostics.rollback_required);
            // Rollback leaves the state untouched and resets CVC diagnostics.
            EXPECT_TRUE(states_bitwise_equal(fixture.state, cuda.state));
            EXPECT_TRUE(states_bitwise_equal(fixture.state, cpu.state));
            EXPECT_EQ(cuda.diagnostics.count_phi_t_jump_events, 0U);
            EXPECT_EQ(cuda.diagnostics.cvc_mass_correction_volume, 0.0);
            EXPECT_EQ(cuda.diagnostics.boundary_inflow_volume, 0.0);
        }
        if (fixture.name == "cvc_on") {
            EXPECT_GT(cuda.diagnostics.count_phi_t_jump_events, 0U);
        }
        if (fixture.name == "runoff_roof_overflow") {
            EXPECT_GT(cuda.diagnostics.roof_overflow_to_surface_volume, 0.0);
        }
        if (fixture.name == "coupling_exchange_applied") {
            EXPECT_NE(cuda.diagnostics.exchange_volume, 0.0);
        }
    }
}

TEST(G9CpuGpuDeterministicMatch, RepeatedCudaRunsAreBitwiseIdentical) {
    if (!cuda_ready()) {
        GTEST_SKIP() << "deterministic CUDA backend unavailable on this host";
    }
    for (const auto& fixture : build_matrix()) {
        SCOPED_TRACE(fixture.name);
        const auto first = run_backend(s2::BackendKind::cuda_deterministic, fixture);
        for (int repeat = 1; repeat < 3; ++repeat) {
            const auto again = run_backend(s2::BackendKind::cuda_deterministic, fixture);
            EXPECT_TRUE(states_bitwise_equal(first.state, again.state))
                << fixture.name << " repeat " << repeat << " state not bitwise identical";
            EXPECT_EQ(first.diagnostics.max_cell_cfl, again.diagnostics.max_cell_cfl);
            EXPECT_EQ(first.diagnostics.exchange_volume, again.diagnostics.exchange_volume);
            EXPECT_EQ(first.diagnostics.surface_added_volume, again.diagnostics.surface_added_volume);
            EXPECT_EQ(first.diagnostics.cvc_mass_correction_volume, again.diagnostics.cvc_mass_correction_volume);
        }
    }
}

TEST(G9CpuGpuDeterministicMatch, DeviceSnapshotRestoreRoundtripIsBitwise) {
    if (!cuda_ready()) {
        GTEST_SKIP() << "deterministic CUDA backend unavailable on this host";
    }
    auto fixture = base_fixture("snapshot_roundtrip");
    set_moving_state(fixture);
    EXPECT_TRUE(s2::cuda_snapshot_restore_roundtrip(fixture.state));
}

#else  // !SCAU_SURFACE2D_HAS_CUDA

TEST(G9CpuGpuDeterministicMatch, SkipsWithoutCompiledCudaBackend) {
    GTEST_SKIP() << "SCAU_ENABLE_CUDA is OFF: deterministic CUDA backend not compiled";
}

#endif
