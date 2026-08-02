#pragma once

#include <string>
#include <vector>

namespace scau::stcf {

enum class ExternalDataKind {
    ugrid_netcdf,
    vector_gis,
    raster_dem,
};

struct ExternalDatasetMetadata {
    ExternalDataKind kind{ExternalDataKind::ugrid_netcdf};
    std::string source_uri;
    std::string source_crs;
    std::string target_crs;
    std::string horizontal_units;
    std::string vertical_units;
    bool authorized_for_project_use{false};
};

struct ExternalFieldMapping {
    std::string source_field;
    std::string target_field;
    std::string source_units;
    std::string target_units;
};

struct ExternalImportContract {
    ExternalDatasetMetadata metadata;
    std::vector<ExternalFieldMapping> field_mappings;
};

// Validates provenance, CRS, units and canonical target-field ownership before
// any external data reaches StcfCase construction. It does not parse a file.
void validate_external_import_contract(const ExternalImportContract& contract);

}  // namespace scau::stcf
