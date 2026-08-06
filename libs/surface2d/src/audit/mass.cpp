#include "surface2d/audit/mass.hpp"

#include <cmath>
#include <stdexcept>

namespace scau::surface2d {

namespace {

class NeumaierSum {
public:
    void add(double value) noexcept {
        const double updated = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - updated) + value;
        } else {
            correction_ += (value - updated) + sum_;
        }
        sum_ = updated;
    }

    [[nodiscard]] double value() const noexcept {
        return sum_ + correction_;
    }

private:
    double sum_{0.0};
    double correction_{0.0};
};

}  // namespace

double total_physical_surface_volume(
    const SurfaceState& state,
    const DpmFields& dpm,
    const GeometryCache& geometry,
    double h_wet) {
    if (!std::isfinite(h_wet) || h_wet < 0.0) {
        throw std::invalid_argument("surface mass audit h_wet must be finite and non-negative");
    }
    if (dpm.cells.size() != state.cells.size()) {
        throw std::invalid_argument(
            "surface mass audit dpm cell count must match surface state cell count");
    }
    if (geometry.cell_areas.size() != state.cells.size()) {
        throw std::invalid_argument(
            "surface mass audit geometry cell count must match surface state cell count");
    }

    NeumaierSum total;
    for (std::size_t cell = 0U; cell < state.cells.size(); ++cell) {
        const double h = static_cast<double>(state.cells[cell].conserved.h);
        const double phi_t = static_cast<double>(dpm.cells[cell].phi_t);
        const double area = static_cast<double>(geometry.cell_areas[cell]);
        if (!std::isfinite(h) || h < 0.0) {
            throw std::invalid_argument(
                "surface mass audit depth must be finite and non-negative");
        }
        if (!std::isfinite(phi_t) || phi_t <= 0.0) {
            throw std::invalid_argument(
                "surface mass audit phi_t must be finite and positive");
        }
        if (!std::isfinite(area) || area <= 0.0) {
            throw std::invalid_argument(
                "surface mass audit cell area must be finite and positive");
        }
        if (h >= h_wet) {
            total.add(phi_t * h * area);
        }
    }
    return total.value();
}

}  // namespace scau::surface2d
