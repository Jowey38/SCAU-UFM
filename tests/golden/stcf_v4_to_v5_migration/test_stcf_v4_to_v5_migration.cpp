#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "stcf/migrate_v4_to_v5.hpp"
#include "stcf/validate.hpp"

TEST(GoldenStcfV4ToV5Migration, PreservesMappedSemanticsAndReportsDefaults) {
    const scau::stcf::LegacyStcfV4Dataset legacy{
        .schema_version = 4,
        .phi_s = {1.0, 0.75, 0.5},
        .psi_xx = {1.0, 0.6, 0.4},
        .psi_xy = {0.0, 0.1, 0.0},
        .psi_yy = {1.0, 0.5, 0.3},
        .edge_count = 4U,
    };

    const auto first = scau::stcf::migrate_stcf_v4_to_v5(legacy);
    const auto second = scau::stcf::migrate_stcf_v4_to_v5(legacy);
    ASSERT_NO_THROW(scau::stcf::validate_stcf_dataset(first.dataset, 3U, 4U));

    EXPECT_EQ(first.dataset.cells.phi_t, legacy.phi_s);
    EXPECT_EQ(first.dataset.cells.phi_xx, legacy.psi_xx);
    EXPECT_EQ(first.dataset.cells.phi_xy, legacy.psi_xy);
    EXPECT_EQ(first.dataset.cells.phi_yy, legacy.psi_yy);
    EXPECT_EQ(first.dataset.edges.omega_edge, (std::vector<double>{1.0, 1.0, 1.0, 1.0}));

    const std::string report = scau::stcf::format_stcf_migration_report(first);
    EXPECT_EQ(report, scau::stcf::format_stcf_migration_report(second));
    EXPECT_NE(report.find("source_schema=4\ntarget_schema=5"), std::string::npos);
    EXPECT_NE(report.find("renamed:phi_s->phi_t;units=1"), std::string::npos);
    EXPECT_NE(report.find("renamed:psi_tensor.xx->phi_xx;units=1"), std::string::npos);
    EXPECT_NE(report.find("defaulted:<missing>->omega_edge;units=1"), std::string::npos);
}
