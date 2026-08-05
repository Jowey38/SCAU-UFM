#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "surface2d/audit/mass.hpp"

namespace {

struct Fixture {
    scau::surface2d::SurfaceState state{};
    scau::surface2d::DpmFields dpm{};
    scau::surface2d::GeometryCache geometry{};

    Fixture() {
        state.cells.resize(3);
        state.cells[0].conserved.h = 2.0;
        state.cells[1].conserved.h = 0.5;
        state.cells[2].conserved.h = 1.0e-7;
        dpm.cells.resize(3);
        dpm.cells[0].phi_t = 0.4;
        dpm.cells[1].phi_t = 1.0;
        dpm.cells[2].phi_t = 0.8;
        geometry.cell_areas = {50.0, 10.0, 100.0};
    }
};

}  // namespace

TEST(Surface2DMassAudit, UsesPhiTStorageAndWetThreshold) {
    const Fixture fixture;
    // Cell 0: 0.4*2*50 = 40; cell 1: 1*0.5*10 = 5;
    // cell 2 is below h_wet and excluded.
    EXPECT_DOUBLE_EQ(scau::surface2d::total_physical_surface_volume(
                         fixture.state, fixture.dpm, fixture.geometry, 1.0e-6),
                     45.0);

    // h == h_wet is included by the canonical rule.
    Fixture boundary;
    boundary.state.cells[2].conserved.h = 1.0e-6;
    EXPECT_NEAR(scau::surface2d::total_physical_surface_volume(
                    boundary.state, boundary.dpm, boundary.geometry, 1.0e-6),
                45.00008, 1.0e-14);
}

TEST(Surface2DMassAudit, CompensatesNumericallyDiverseTerms) {
    Fixture fixture;
    fixture.state.cells[0].conserved.h = 1.0e16;
    fixture.dpm.cells[0].phi_t = 1.0;
    fixture.geometry.cell_areas[0] = 1.0;
    fixture.state.cells[1].conserved.h = 1.0;
    fixture.geometry.cell_areas[1] = 1.0;
    fixture.state.cells[2].conserved.h = 1.0;
    fixture.dpm.cells[2].phi_t = 1.0;
    fixture.geometry.cell_areas[2] = 1.0;
    EXPECT_DOUBLE_EQ(scau::surface2d::total_physical_surface_volume(
                         fixture.state, fixture.dpm, fixture.geometry, 0.0),
                     1.0e16 + 2.0);
}

TEST(Surface2DMassAudit, RejectsInvalidInputs) {
    Fixture fixture;
    fixture.state.cells[0].conserved.h = -0.1;
    EXPECT_THROW(static_cast<void>(scau::surface2d::total_physical_surface_volume(
                     fixture.state, fixture.dpm, fixture.geometry, 0.0)),
                 std::invalid_argument);

    Fixture bad_phi;
    bad_phi.dpm.cells[0].phi_t = 0.0;
    EXPECT_THROW(static_cast<void>(scau::surface2d::total_physical_surface_volume(
                     bad_phi.state, bad_phi.dpm, bad_phi.geometry, 0.0)),
                 std::invalid_argument);

    Fixture bad_area;
    bad_area.geometry.cell_areas[0] =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(static_cast<void>(scau::surface2d::total_physical_surface_volume(
                     bad_area.state, bad_area.dpm, bad_area.geometry, 0.0)),
                 std::invalid_argument);

    const Fixture valid;
    EXPECT_THROW(static_cast<void>(scau::surface2d::total_physical_surface_volume(
                     valid.state, valid.dpm, valid.geometry, -1.0)),
                 std::invalid_argument);

    Fixture size_mismatch;
    size_mismatch.dpm.cells.pop_back();
    EXPECT_THROW(static_cast<void>(scau::surface2d::total_physical_surface_volume(
                     size_mismatch.state, size_mismatch.dpm,
                     size_mismatch.geometry, 0.0)),
                 std::invalid_argument);
}
