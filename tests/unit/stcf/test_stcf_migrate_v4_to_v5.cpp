#include <limits>

#include <gtest/gtest.h>

#include "core/error.hpp"
#include "stcf/migrate_v4_to_v5.hpp"
#include "stcf/validate.hpp"

namespace {

scau::stcf::LegacyStcfV4Dataset legacy_fixture() {
    return scau::stcf::LegacyStcfV4Dataset{
        .schema_version = 4,
        .phi_s = {0.8, 0.6},
        .psi_xx = {0.5, 0.4},
        .psi_xy = {0.1, 0.0},
        .psi_yy = {0.4, 0.3},
        .edge_count = 3U,
    };
}

}  // namespace

TEST(StcfMigrateV4ToV5, MapsEvidenceBackedFieldsWithoutValueDrift) {
    const auto migration = scau::stcf::migrate_stcf_v4_to_v5(legacy_fixture());

    EXPECT_EQ(migration.dataset.schema_version, 5);
    EXPECT_EQ(migration.dataset.cells.phi_t, (std::vector<double>{0.8, 0.6}));
    EXPECT_EQ(migration.dataset.cells.phi_xx, (std::vector<double>{0.5, 0.4}));
    EXPECT_EQ(migration.dataset.cells.phi_xy, (std::vector<double>{0.1, 0.0}));
    EXPECT_EQ(migration.dataset.cells.phi_yy, (std::vector<double>{0.4, 0.3}));
    EXPECT_NO_THROW(scau::stcf::validate_stcf_dataset(migration.dataset, 2U, 3U));
}

TEST(StcfMigrateV4ToV5, RecordsEveryRequiredDefault) {
    const auto migration = scau::stcf::migrate_stcf_v4_to_v5(legacy_fixture());
    const std::string report = scau::stcf::format_stcf_migration_report(migration);

    EXPECT_NE(report.find("renamed:phi_s->phi_t;units=1"), std::string::npos);
    EXPECT_NE(report.find("renamed:psi_tensor.xy->phi_xy;units=1"), std::string::npos);
    EXPECT_NE(report.find("defaulted:<missing>->omega_edge;units=1"), std::string::npos);
    EXPECT_NE(report.find("defaulted:<missing>->soil_params;units=SI"), std::string::npos);
    EXPECT_EQ(migration.dataset.edges.omega_edge, (std::vector<double>{1.0, 1.0, 1.0}));
    EXPECT_EQ(migration.dataset.cells.manning_n, (std::vector<double>{0.0, 0.0}));
}

TEST(StcfMigrateV4ToV5, IsDeterministic) {
    const auto first = scau::stcf::migrate_stcf_v4_to_v5(legacy_fixture());
    const auto second = scau::stcf::migrate_stcf_v4_to_v5(legacy_fixture());
    EXPECT_EQ(
        scau::stcf::format_stcf_migration_report(first),
        scau::stcf::format_stcf_migration_report(second));
    EXPECT_EQ(first.dataset.cells.phi_t, second.dataset.cells.phi_t);
    EXPECT_EQ(first.dataset.edges.phi_e_n, second.dataset.edges.phi_e_n);
}

TEST(StcfMigrateV4ToV5, RejectsWrongVersionAndShape) {
    auto wrong_version = legacy_fixture();
    wrong_version.schema_version = 5;
    EXPECT_THROW(
        static_cast<void>(scau::stcf::migrate_stcf_v4_to_v5(wrong_version)),
        scau::core::ScauError);

    auto wrong_shape = legacy_fixture();
    wrong_shape.psi_xy.pop_back();
    EXPECT_THROW(
        static_cast<void>(scau::stcf::migrate_stcf_v4_to_v5(wrong_shape)),
        scau::core::ScauError);
}

TEST(StcfMigrateV4ToV5, RejectsInvalidPhysicalValuesThroughV5Gate) {
    auto invalid = legacy_fixture();
    invalid.phi_s[0] = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(
        static_cast<void>(scau::stcf::migrate_stcf_v4_to_v5(invalid)),
        scau::core::ScauError);
}
