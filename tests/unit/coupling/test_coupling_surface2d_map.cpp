#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/driver/surface2d_coupling_map.hpp"

// Unit coverage for the Surface2D <-> CouplingState adapter seam used by the
// SimDriver run loop. The adapter is pure projection/write-back: no mesh is
// needed, only consistent cell-indexed containers.

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ExchangeCellState;
using scau::coupling::core::SharedExchangeEndpointDeficit;
using scau::coupling::core::SharedExchangeEngine;
using scau::coupling::driver::Surface2DCouplingMap;
using scau::coupling::driver::apply_exchange_write_back;
using scau::coupling::driver::build_exchange_cells;
using scau::coupling::driver::validate_surface2d_coupling_map;

// Three surface cells; cells 0 and 2 participate in coupling.
// Cell 0: h=2.0, phi_t=0.4, area=50 -> V=40.  Cell 2: h=1.0, phi_t=1.0, area=10 -> V=10.
struct Fixture {
    scau::surface2d::SurfaceState state{};
    scau::surface2d::DpmFields dpm{};
    scau::surface2d::GeometryCache geometry{};
    std::vector<scau::core::Real> bed_elevations{};
    Surface2DCouplingMap map{};

    Fixture() {
        state.cells.resize(3);
        state.cells[0].conserved.h = 2.0;
        state.cells[0].conserved.hu = 1.0;
        state.cells[0].conserved.hv = -0.5;
        state.cells[0].eta = 12.0;
        state.cells[1].conserved.h = 0.3;
        state.cells[2].conserved.h = 1.0;
        state.cells[2].conserved.hu = 0.2;

        dpm.cells.resize(3);
        dpm.cells[0].phi_t = 0.4;
        dpm.cells[1].phi_t = 1.0;
        dpm.cells[2].phi_t = 1.0;

        geometry.cell_areas = {50.0, 25.0, 10.0};

        bed_elevations = {10.0, 9.0, 8.0};

        map.surface_cells = {0U, 2U};
    }
};

double physical_volume(const Fixture& fx, std::size_t cell) {
    return static_cast<double>(fx.state.cells[cell].conserved.h) *
           static_cast<double>(fx.dpm.cells[cell].phi_t) *
           static_cast<double>(fx.geometry.cell_areas[cell]);
}

}  // namespace

TEST(Surface2DCouplingMap, BuildProjectsPhysicalStorageVolume) {
    const Fixture fx;
    const auto cells = build_exchange_cells(fx.state, fx.dpm, fx.geometry, fx.map, nullptr);

    ASSERT_EQ(cells.size(), 2U);
    EXPECT_DOUBLE_EQ(cells[0].volume, 40.0);
    EXPECT_DOUBLE_EQ(cells[0].phi_t, 0.4);
    EXPECT_DOUBLE_EQ(cells[0].h, 2.0);
    EXPECT_DOUBLE_EQ(cells[0].area, 50.0);
    EXPECT_DOUBLE_EQ(cells[1].volume, 10.0);
    EXPECT_DOUBLE_EQ(cells[1].mass_deficit_account.volume, 0.0);
    EXPECT_TRUE(cells[1].shared_deficit_accounts.empty());
}

TEST(Surface2DCouplingMap, BuildCarriesDeficitAccountsAcrossEpochRebuild) {
    const Fixture fx;
    auto previous_cells = build_exchange_cells(fx.state, fx.dpm, fx.geometry, fx.map, nullptr);
    previous_cells[0].mass_deficit_account.volume = 3.5;
    SharedExchangeEndpointDeficit endpoint_deficit{};
    endpoint_deficit.endpoint.engine = SharedExchangeEngine::river;
    endpoint_deficit.endpoint.node_id = 7U;
    endpoint_deficit.mass_deficit_account.volume = 1.25;
    previous_cells[1].shared_deficit_accounts.push_back(endpoint_deficit);
    const CouplingState previous{previous_cells};

    const auto rebuilt = build_exchange_cells(fx.state, fx.dpm, fx.geometry, fx.map, &previous);

    ASSERT_EQ(rebuilt.size(), 2U);
    EXPECT_DOUBLE_EQ(rebuilt[0].mass_deficit_account.volume, 3.5);
    ASSERT_EQ(rebuilt[1].shared_deficit_accounts.size(), 1U);
    EXPECT_EQ(rebuilt[1].shared_deficit_accounts[0].endpoint.engine, SharedExchangeEngine::river);
    EXPECT_EQ(rebuilt[1].shared_deficit_accounts[0].endpoint.node_id, 7U);
    EXPECT_DOUBLE_EQ(rebuilt[1].shared_deficit_accounts[0].mass_deficit_account.volume, 1.25);
    EXPECT_DOUBLE_EQ(rebuilt[0].volume, 40.0);
}

TEST(Surface2DCouplingMap, BuildAndValidateFailClosed) {
    const Fixture fx;

    Surface2DCouplingMap empty_map{};
    EXPECT_THROW(validate_surface2d_coupling_map(empty_map, fx.state, fx.dpm, fx.geometry),
                 std::invalid_argument);

    Surface2DCouplingMap out_of_range{};
    out_of_range.surface_cells = {0U, 3U};
    EXPECT_THROW(validate_surface2d_coupling_map(out_of_range, fx.state, fx.dpm, fx.geometry),
                 std::invalid_argument);

    Surface2DCouplingMap duplicate{};
    duplicate.surface_cells = {0U, 0U};
    EXPECT_THROW(validate_surface2d_coupling_map(duplicate, fx.state, fx.dpm, fx.geometry),
                 std::invalid_argument);

    Fixture negative_depth;
    negative_depth.state.cells[0].conserved.h = -0.1;
    EXPECT_THROW(
        static_cast<void>(build_exchange_cells(
            negative_depth.state, negative_depth.dpm, negative_depth.geometry,
            negative_depth.map, nullptr)),
        std::invalid_argument);

    const Fixture mismatch;
    const CouplingState wrong_size{{build_exchange_cells(
        mismatch.state, mismatch.dpm, mismatch.geometry, mismatch.map, nullptr)[0]}};
    EXPECT_THROW(
        static_cast<void>(build_exchange_cells(
            mismatch.state, mismatch.dpm, mismatch.geometry, mismatch.map, &wrong_size)),
        std::invalid_argument);
}

TEST(Surface2DCouplingMap, WriteBackDrainScalesMomentumPreservingVelocity) {
    Fixture fx;
    const auto before = build_exchange_cells(fx.state, fx.dpm, fx.geometry, fx.map, nullptr);
    const double u_before = fx.state.cells[0].u();
    const double v_before = fx.state.cells[0].v();

    auto after_cells = before;
    after_cells[0].volume = 36.0;  // drained 4 m3 -> h 2.0 -> 1.8
    after_cells[0].h = 1.8;
    const CouplingState after{after_cells};

    const auto report = apply_exchange_write_back(
        fx.state, fx.dpm, fx.geometry, fx.map, before, after, fx.bed_elevations);

    EXPECT_NEAR(fx.state.cells[0].conserved.h, 1.8, 1.0e-12);
    EXPECT_NEAR(fx.state.cells[0].eta, 11.8, 1.0e-12);
    EXPECT_NEAR(fx.state.cells[0].u(), u_before, 1.0e-12);
    EXPECT_NEAR(fx.state.cells[0].v(), v_before, 1.0e-12);
    EXPECT_NEAR(fx.state.cells[0].conserved.hu, 1.0 * (1.8 / 2.0), 1.0e-12);
    EXPECT_NEAR(report.drained_volume, 4.0, 1.0e-12);
    EXPECT_NEAR(report.returned_volume, 0.0, 1.0e-12);
    // Non-coupled cell untouched.
    EXPECT_DOUBLE_EQ(fx.state.cells[1].conserved.h, 0.3);
}

TEST(Surface2DCouplingMap, WriteBackReturnAddsZeroMomentumVolume) {
    Fixture fx;
    const auto before = build_exchange_cells(fx.state, fx.dpm, fx.geometry, fx.map, nullptr);
    const double hu_before = fx.state.cells[2].conserved.hu;

    auto after_cells = before;
    after_cells[1].volume = 12.0;  // returned 2 m3 -> h 1.0 -> 1.2 (phi_t=1, area=10)
    after_cells[1].h = 1.2;
    const CouplingState after{after_cells};

    const auto report = apply_exchange_write_back(
        fx.state, fx.dpm, fx.geometry, fx.map, before, after, fx.bed_elevations);

    EXPECT_NEAR(fx.state.cells[2].conserved.h, 1.2, 1.0e-12);
    EXPECT_NEAR(fx.state.cells[2].eta, 9.2, 1.0e-12);
    EXPECT_DOUBLE_EQ(fx.state.cells[2].conserved.hu, hu_before);
    EXPECT_LT(fx.state.cells[2].u(), hu_before / 1.0 + 1.0e-15);
    EXPECT_NEAR(report.returned_volume, 2.0, 1.0e-12);
    EXPECT_NEAR(report.drained_volume, 0.0, 1.0e-12);
}

TEST(Surface2DCouplingMap, WriteBackRoundTripConservesPhysicalStorage) {
    Fixture fx;
    const auto before = build_exchange_cells(fx.state, fx.dpm, fx.geometry, fx.map, nullptr);

    auto after_cells = before;
    after_cells[0].volume = 33.0;
    after_cells[0].h = 33.0 / (0.4 * 50.0);
    after_cells[1].volume = 10.5;
    after_cells[1].h = 1.05;
    const CouplingState after{after_cells};

    const auto report = apply_exchange_write_back(
        fx.state, fx.dpm, fx.geometry, fx.map, before, after, fx.bed_elevations);

    double surface_total = 0.0;
    for (const std::size_t cell : fx.map.surface_cells) {
        surface_total += physical_volume(fx, cell);
    }
    EXPECT_NEAR(surface_total, 33.0 + 10.5, 1.0e-12);
    EXPECT_NEAR(report.drained_volume - report.returned_volume, (40.0 - 33.0) - 0.5, 1.0e-12);
}

TEST(Surface2DCouplingMap, WriteBackFailClosed) {
    Fixture fx;
    const auto before = build_exchange_cells(fx.state, fx.dpm, fx.geometry, fx.map, nullptr);

    // Pending events present: write-back must run post-replay.
    CouplingState pending{before};
    scau::coupling::core::CouplingEvent event{};
    event.exchange_cell_index = 0U;
    event.volume_delta = 1.0;
    pending.enqueue_event(event);
    EXPECT_THROW(
        static_cast<void>(apply_exchange_write_back(
            fx.state, fx.dpm, fx.geometry, fx.map, before, pending, fx.bed_elevations)),
        std::logic_error);

    // cells_before size mismatch.
    const CouplingState after{before};
    const std::vector<ExchangeCellState> short_before{before[0]};
    EXPECT_THROW(
        static_cast<void>(apply_exchange_write_back(
            fx.state, fx.dpm, fx.geometry, fx.map, short_before, after, fx.bed_elevations)),
        std::invalid_argument);

    // Bed elevation size mismatch.
    const std::vector<scau::core::Real> short_bed{10.0};
    EXPECT_THROW(
        static_cast<void>(apply_exchange_write_back(
            fx.state, fx.dpm, fx.geometry, fx.map, before, after, short_bed)),
        std::invalid_argument);
}
