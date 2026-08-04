#include "coupling/driver/surface2d_coupling_map.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace scau::coupling::driver {

void validate_surface2d_coupling_map(
    const Surface2DCouplingMap& map,
    const surface2d::SurfaceState& state,
    const surface2d::DpmFields& dpm,
    const surface2d::GeometryCache& geometry) {
    if (map.surface_cells.empty()) {
        throw std::invalid_argument("surface2d coupling map must not be empty");
    }
    if (dpm.cells.size() != state.cells.size()) {
        throw std::invalid_argument(
            "surface2d coupling map: dpm cell count must match surface state cell count");
    }
    if (geometry.cell_areas.size() != state.cells.size()) {
        throw std::invalid_argument(
            "surface2d coupling map: geometry cell count must match surface state cell count");
    }
    std::unordered_set<std::size_t> seen;
    seen.reserve(map.surface_cells.size());
    for (const std::size_t surface_cell : map.surface_cells) {
        if (surface_cell >= state.cells.size()) {
            throw std::invalid_argument(
                "surface2d coupling map: surface cell index out of range");
        }
        if (!seen.insert(surface_cell).second) {
            throw std::invalid_argument(
                "surface2d coupling map: surface cell mapped more than once");
        }
    }
}

std::vector<core::ExchangeCellState> build_exchange_cells(
    const surface2d::SurfaceState& state,
    const surface2d::DpmFields& dpm,
    const surface2d::GeometryCache& geometry,
    const Surface2DCouplingMap& map,
    const core::CouplingState* previous) {
    validate_surface2d_coupling_map(map, state, dpm, geometry);
    if (previous != nullptr && previous->cells().size() != map.surface_cells.size()) {
        throw std::invalid_argument(
            "surface2d coupling map: previous coupling state cell count must match map");
    }

    std::vector<core::ExchangeCellState> cells;
    cells.reserve(map.surface_cells.size());
    for (std::size_t exchange_index = 0U; exchange_index < map.surface_cells.size();
         ++exchange_index) {
        const std::size_t surface_cell = map.surface_cells[exchange_index];
        const double h = static_cast<double>(state.cells[surface_cell].conserved.h);
        const double phi_t = static_cast<double>(dpm.cells[surface_cell].phi_t);
        const double area = static_cast<double>(geometry.cell_areas[surface_cell]);
        if (!std::isfinite(h) || h < 0.0) {
            throw std::invalid_argument(
                "surface2d coupling map: surface depth must be finite and non-negative");
        }
        if (!std::isfinite(phi_t) || phi_t <= 0.0) {
            throw std::invalid_argument(
                "surface2d coupling map: phi_t must be finite and positive");
        }
        if (!std::isfinite(area) || area <= 0.0) {
            throw std::invalid_argument(
                "surface2d coupling map: cell area must be finite and positive");
        }

        core::ExchangeCellState cell{};
        cell.phi_t = phi_t;
        cell.h = h;
        cell.area = area;
        cell.volume = phi_t * h * area;
        if (previous != nullptr) {
            const core::ExchangeCellState& carried = previous->cells()[exchange_index];
            cell.mass_deficit_account = carried.mass_deficit_account;
            cell.shared_deficit_accounts = carried.shared_deficit_accounts;
        }
        cells.push_back(std::move(cell));
    }
    return cells;
}

ExchangeWriteBackReport apply_exchange_write_back(
    surface2d::SurfaceState& state,
    const surface2d::DpmFields& dpm,
    const surface2d::GeometryCache& geometry,
    const Surface2DCouplingMap& map,
    const std::vector<core::ExchangeCellState>& cells_before,
    const core::CouplingState& coupling_after,
    const std::vector<scau::core::Real>& bed_elevations) {
    validate_surface2d_coupling_map(map, state, dpm, geometry);
    if (cells_before.size() != map.surface_cells.size()) {
        throw std::invalid_argument(
            "surface2d write-back: cells_before count must match map");
    }
    if (coupling_after.cells().size() != map.surface_cells.size()) {
        throw std::invalid_argument(
            "surface2d write-back: coupling state cell count must match map");
    }
    if (bed_elevations.size() != state.cells.size()) {
        throw std::invalid_argument(
            "surface2d write-back: bed elevation count must match surface state cell count");
    }
    if (!coupling_after.snapshot().pending_events().empty()) {
        throw std::logic_error(
            "surface2d write-back: coupling state has pending events; "
            "write-back must run after replay_pending()");
    }

    ExchangeWriteBackReport report{};
    for (std::size_t exchange_index = 0U; exchange_index < map.surface_cells.size();
         ++exchange_index) {
        const std::size_t surface_cell = map.surface_cells[exchange_index];
        const core::ExchangeCellState& after = coupling_after.cells()[exchange_index];
        const double volume_before = cells_before[exchange_index].volume;
        const double volume_after = after.volume;
        if (!std::isfinite(volume_after) || volume_after < 0.0) {
            throw std::invalid_argument(
                "surface2d write-back: post-replay volume must be finite and non-negative");
        }

        const double phi_t = static_cast<double>(dpm.cells[surface_cell].phi_t);
        const double area = static_cast<double>(geometry.cell_areas[surface_cell]);
        const double h_new = volume_after / (phi_t * area);
        if (!std::isfinite(h_new) || h_new < 0.0) {
            throw std::invalid_argument(
                "surface2d write-back: resulting depth must be finite and non-negative");
        }

        surface2d::CellState& surface = state.cells[surface_cell];
        const double h_old = static_cast<double>(surface.conserved.h);
        const double volume_delta = volume_after - volume_before;
        if (volume_delta < 0.0) {
            // Drain: water leaves at the cell velocity, momentum scales with depth.
            const double scale = h_old > 0.0 ? h_new / h_old : 0.0;
            surface.conserved.hu *= scale;
            surface.conserved.hv *= scale;
            report.drained_volume += -volume_delta;
        } else if (volume_delta > 0.0) {
            // Return: returned water carries zero momentum, hu/hv unchanged.
            report.returned_volume += volume_delta;
        }
        surface.conserved.h = h_new;
        surface.eta = static_cast<double>(bed_elevations[surface_cell]) + h_new;
    }
    return report;
}

}  // namespace scau::coupling::driver
