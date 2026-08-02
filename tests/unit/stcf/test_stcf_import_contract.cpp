#include <gtest/gtest.h>

#include "stcf/import_contract.hpp"

namespace {

scau::stcf::ExternalImportContract valid_contract() {
    return {
        .metadata = {
            .kind = scau::stcf::ExternalDataKind::ugrid_netcdf,
            .source_uri = "authorized-sample.nc",
            .source_crs = "EPSG:4326",
            .target_crs = "EPSG:32649",
            .horizontal_units = "degree",
            .vertical_units = "m",
            .authorized_for_project_use = true,
        },
        .field_mappings = {
            {"storage_porosity", "phi_t", "1", "1"},
            {"bed_elevation", "z_b", "m", "m"},
        },
    };
}

}  // namespace

TEST(StcfImportContract, AcceptsAuthorizedCanonicalMapping) {
    EXPECT_NO_THROW(scau::stcf::validate_external_import_contract(valid_contract()));
}

TEST(StcfImportContract, RejectsUnauthorizedUnknownAndDuplicateMappings) {
    auto unauthorized = valid_contract();
    unauthorized.metadata.authorized_for_project_use = false;
    EXPECT_THROW(
        scau::stcf::validate_external_import_contract(unauthorized),
        std::invalid_argument);

    auto unknown = valid_contract();
    unknown.field_mappings[0].target_field = "Q_limit";
    EXPECT_THROW(scau::stcf::validate_external_import_contract(unknown), std::invalid_argument);

    auto duplicate = valid_contract();
    duplicate.field_mappings[1].target_field = "phi_t";
    EXPECT_THROW(scau::stcf::validate_external_import_contract(duplicate), std::invalid_argument);
}
