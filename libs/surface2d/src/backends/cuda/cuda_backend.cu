// Deterministic CUDA backend for the Surface2D step (M284/G9).
//
// Determinism contract (M280 pattern, proven bitwise on the governed host):
//   - one thread per EDGE computes the shared SCAU_HD numerics exactly once
//     and writes per-side contribution slots (no atomics anywhere);
//   - one thread per CELL gathers its owned edges in ascending global edge
//     index, which is bitwise-identical association to the CPU reference
//     edge loop (a cell's contributions arrive in edge-index order there);
//   - whole-domain scalar diagnostics use a fixed-order 256-wide sequential
//     pairwise block tree with block partials combined sequentially on the
//     host (deterministic; agreement with the CPU sequential sums is locked
//     at 1e-12 by G9, exact for max/count);
//   - double precision throughout; compiled with --fmad=false so device
//     contraction matches the /fp:precise MSVC host. Device sqrt/fabs are
//     IEEE correctly rounded (bitwise-identical to the host); device pow may
//     differ by ulps and only Manning friction uses it (G9 locks hu/hv at
//     1e-12 on friction-active fixtures; h and eta are bitwise).
//
// Mutation model: the caller's state/runoff_state are copied, advanced on the
// device, and committed only after every stage succeeded, so any failure
// leaves the caller's state untouched (the CPU reference can throw
// mid-mutation; the CUDA backend is strictly atomic per step -- recorded in
// the M284 plan). A device error flag re-creates the checked-kernel failure
// surface for values that only exist on the device.

#include "surface2d/backends/cuda_backend.hpp"

#include <cuda_runtime.h>

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "surface2d/dpm/cvc_augmented_flux.hpp"
#include "surface2d/dpm/edge_classification.hpp"
#include "surface2d/reconstruction/hydrostatic.hpp"
#include "surface2d/riemann/hllc.hpp"
#include "surface2d/source_terms/coupling_exchange.hpp"
#include "surface2d/source_terms/friction.hpp"
#include "surface2d/source_terms/phi_t.hpp"
#include "surface2d/source_terms/runoff/runoff_generation.hpp"
#include "surface2d/source_terms/well_balanced.hpp"
#include "surface2d/time_integration/boundary_ghosts.hpp"
#include "surface2d/wetting_drying/limits.hpp"

namespace scau::surface2d {
namespace {

constexpr int kBlock = 256;

constexpr int kBoundaryNone = -1;  // internal edge
constexpr int kBoundaryWall = 0;
constexpr int kBoundaryOpen = 1;
constexpr int kBoundaryDischargeInflow = 2;
constexpr int kBoundaryWaterLevel = 3;

void cuda_check(cudaError_t status, const char* what) {
    if (status != cudaSuccess) {
        throw std::runtime_error(
            std::string("surface2d CUDA backend: ") + what + " failed: " + cudaGetErrorString(status));
    }
}

// RAII device buffer.
template <typename T>
class DeviceArray {
public:
    DeviceArray() = default;
    explicit DeviceArray(std::size_t count) { allocate(count); }
    ~DeviceArray() { release(); }
    DeviceArray(const DeviceArray&) = delete;
    DeviceArray& operator=(const DeviceArray&) = delete;

    void allocate(std::size_t count) {
        release();
        count_ = count;
        if (count_ > 0) {
            cuda_check(cudaMalloc(&ptr_, count_ * sizeof(T)), "cudaMalloc");
        }
    }
    void upload(const T* host, std::size_t count) {
        if (count > 0) {
            cuda_check(cudaMemcpy(ptr_, host, count * sizeof(T), cudaMemcpyHostToDevice), "cudaMemcpy H2D");
        }
    }
    void upload(const std::vector<T>& host) { upload(host.data(), host.size()); }
    void download(T* host, std::size_t count) const {
        if (count > 0) {
            cuda_check(cudaMemcpy(host, ptr_, count * sizeof(T), cudaMemcpyDeviceToHost), "cudaMemcpy D2H");
        }
    }
    void copy_from(const DeviceArray<T>& other, std::size_t count) {
        if (count > 0) {
            cuda_check(cudaMemcpy(ptr_, other.ptr_, count * sizeof(T), cudaMemcpyDeviceToDevice), "cudaMemcpy D2D");
        }
    }
    void zero(std::size_t count) {
        if (count > 0) {
            cuda_check(cudaMemset(ptr_, 0, count * sizeof(T)), "cudaMemset");
        }
    }
    [[nodiscard]] T* get() const noexcept { return ptr_; }

private:
    void release() noexcept {
        if (ptr_ != nullptr) {
            (void)cudaFree(ptr_);
            ptr_ = nullptr;
        }
    }
    T* ptr_{nullptr};
    std::size_t count_{0};
};

inline int grid_for(int n) { return (n + kBlock - 1) / kBlock; }

// ---------------------------------------------------------------------------
// Fixed-order reductions (M280 pattern).
// ---------------------------------------------------------------------------

__global__ void k_reduce_sum(const double* in, double* partials, int n) {
    __shared__ double shared[kBlock];
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    shared[threadIdx.x] = (i < n) ? in[i] : 0.0;
    __syncthreads();
    for (int stride = kBlock / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared[threadIdx.x] += shared[threadIdx.x + stride];
        }
        __syncthreads();
    }
    if (threadIdx.x == 0) partials[blockIdx.x] = shared[0];
}

__global__ void k_reduce_max(const double* in, double* partials, int n) {
    __shared__ double shared[kBlock];
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    shared[threadIdx.x] = (i < n) ? in[i] : 0.0;
    __syncthreads();
    for (int stride = kBlock / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            const double other = shared[threadIdx.x + stride];
            if (shared[threadIdx.x] < other) shared[threadIdx.x] = other;
        }
        __syncthreads();
    }
    if (threadIdx.x == 0) partials[blockIdx.x] = shared[0];
}

__global__ void k_reduce_count(const int* in, unsigned long long* partials, int n) {
    __shared__ unsigned long long shared[kBlock];
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    shared[threadIdx.x] = (i < n) ? static_cast<unsigned long long>(in[i]) : 0ULL;
    __syncthreads();
    for (int stride = kBlock / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared[threadIdx.x] += shared[threadIdx.x + stride];
        }
        __syncthreads();
    }
    if (threadIdx.x == 0) partials[blockIdx.x] = shared[0];
}

double reduce_sum(const DeviceArray<double>& values, int n) {
    if (n == 0) return 0.0;
    const int grid = grid_for(n);
    DeviceArray<double> partials(static_cast<std::size_t>(grid));
    k_reduce_sum<<<grid, kBlock>>>(values.get(), partials.get(), n);
    cuda_check(cudaGetLastError(), "k_reduce_sum launch");
    std::vector<double> host(static_cast<std::size_t>(grid));
    partials.download(host.data(), host.size());
    double total = 0.0;
    for (int b = 0; b < grid; ++b) total += host[static_cast<std::size_t>(b)];
    return total;
}

double reduce_max(const DeviceArray<double>& values, int n) {
    if (n == 0) return 0.0;
    const int grid = grid_for(n);
    DeviceArray<double> partials(static_cast<std::size_t>(grid));
    k_reduce_max<<<grid, kBlock>>>(values.get(), partials.get(), n);
    cuda_check(cudaGetLastError(), "k_reduce_max launch");
    std::vector<double> host(static_cast<std::size_t>(grid));
    partials.download(host.data(), host.size());
    double result = 0.0;
    for (int b = 0; b < grid; ++b) {
        if (result < host[static_cast<std::size_t>(b)]) result = host[static_cast<std::size_t>(b)];
    }
    return result;
}

std::size_t reduce_count(const DeviceArray<int>& values, int n) {
    if (n == 0) return 0U;
    const int grid = grid_for(n);
    DeviceArray<unsigned long long> partials(static_cast<std::size_t>(grid));
    k_reduce_count<<<grid, kBlock>>>(values.get(), partials.get(), n);
    cuda_check(cudaGetLastError(), "k_reduce_count launch");
    std::vector<unsigned long long> host(static_cast<std::size_t>(grid));
    partials.download(host.data(), host.size());
    unsigned long long total = 0ULL;
    for (int b = 0; b < grid; ++b) total += host[static_cast<std::size_t>(b)];
    return static_cast<std::size_t>(total);
}

// ---------------------------------------------------------------------------
// Views passed by value into kernels.
// ---------------------------------------------------------------------------

struct CellsView {
    double* h{nullptr};
    double* hu{nullptr};
    double* hv{nullptr};
    const double* eta{nullptr};
    const double* phi_t{nullptr};
    const double* area{nullptr};
    int cell_count{0};
};

struct EdgesView {
    const double* nx{nullptr};
    const double* ny{nullptr};
    const double* len{nullptr};
    const int* left{nullptr};   // -1 when absent
    const int* right{nullptr};  // -1 when absent
    const int* bkind{nullptr};  // kBoundaryNone for internal edges
    const double* phi_e_n{nullptr};
    const double* omega{nullptr};
    const double* q_inflow{nullptr};   // DischargeInflow q per unit length
    const double* eta_bc{nullptr};     // WaterLevel prescribed stage
    int edge_count{0};
};

// Per-edge, per-side contribution slots, indexed 2*e + slot (slot 0 = left
// adjacency, slot 1 = right adjacency). The gather adds, per owned edge:
// mass, then momentum-x flux then momentum-x WB, then the same for y --
// exactly the CPU accumulation sequence.
struct ContribView {
    double* mass{nullptr};
    double* fx{nullptr};
    double* fy{nullptr};
    double* wx{nullptr};
    double* wy{nullptr};
};

struct CvcOutView {
    int* flag{nullptr};
    double* mass{nullptr};
    double* mx{nullptr};
    double* my{nullptr};
    double* resid{nullptr};
};

struct CsrView {
    const int* offsets{nullptr};    // cell_count + 1
    const int* edge_index{nullptr};
    const int* slot{nullptr};
};

// ---------------------------------------------------------------------------
// CFL (mirrors cfl/diagnostics.cpp raw_cell_cfl, including its fixed default
// gravity of 9.81 for the wave-speed estimate).
// ---------------------------------------------------------------------------

__global__ void k_edge_cfl(CellsView cells, EdgesView edges, double dt, double* te) {
    const int e = blockIdx.x * blockDim.x + threadIdx.x;
    if (e >= edges.edge_count) return;

    const int l = edges.left[e];
    const int r = edges.right[e];
    const bool internal = l >= 0 && r >= 0;
    if (!internal
        && (edges.bkind[e] == kBoundaryWall || edges.bkind[e] == kBoundaryDischargeInflow)) {
        te[e] = 0.0;
        return;
    }
    const int left_index = l >= 0 ? l : r;
    const int right_index = r >= 0 ? r : l;
    const CellState left{
        .conserved = {.h = cells.h[left_index], .hu = cells.hu[left_index], .hv = cells.hv[left_index]},
        .eta = cells.eta[left_index]};
    const CellState right{
        .conserved = {.h = cells.h[right_index], .hu = cells.hu[right_index], .hv = cells.hv[right_index]},
        .eta = cells.eta[right_index]};
    const auto speeds = estimate_hllc_wave_speeds(left, right, Normal2{.x = edges.nx[e], .y = edges.ny[e]});
    const core::Real spectral_radius = hd::max_(hd::abs_(speeds.s_l), hd::abs_(speeds.s_r));
    te[e] = dt * edges.len[e] * spectral_radius;
}

__global__ void k_cfl_gather(CsrView csr, const double* te, const double* area, double* cell_cfl, int cell_count) {
    const int c = blockIdx.x * blockDim.x + threadIdx.x;
    if (c >= cell_count) return;
    double value = 0.0;
    if (area[c] > 0.0) {
        for (int k = csr.offsets[c]; k < csr.offsets[c + 1]; ++k) {
            value += te[csr.edge_index[k]] / area[c];
        }
    }
    cell_cfl[c] = value;
}

// ---------------------------------------------------------------------------
// Edge flux kernel (mirrors run_flux_core's per-edge branches exactly).
// ---------------------------------------------------------------------------

struct FluxConfig {
    double h_min{0.0};
    double gravity{0.0};
    double dt{0.0};
    int enable_cvc{0};
};

__global__ void k_edge_flux(
    CellsView cells,
    EdgesView edges,
    FluxConfig config,
    ContribView contrib,
    CvcOutView cvc,
    double* binflow,
    EdgeStepDiagnostics* ediag) {
    const int e = blockIdx.x * blockDim.x + threadIdx.x;
    if (e >= edges.edge_count) return;

    const double nx = edges.nx[e];
    const double ny = edges.ny[e];
    const double len = edges.len[e];
    const Normal2 normal{.x = nx, .y = ny};
    const EdgeDpmFields edge_fields{.phi_e_n = edges.phi_e_n[e], .omega_edge = edges.omega[e]};

    const int base = 2 * e;
    contrib.mass[base] = 0.0;
    contrib.mass[base + 1] = 0.0;
    contrib.fx[base] = 0.0;
    contrib.fx[base + 1] = 0.0;
    contrib.fy[base] = 0.0;
    contrib.fy[base + 1] = 0.0;
    contrib.wx[base] = 0.0;
    contrib.wx[base + 1] = 0.0;
    contrib.wy[base] = 0.0;
    contrib.wy[base + 1] = 0.0;
    cvc.flag[e] = 0;
    cvc.mass[e] = 0.0;
    cvc.mx[e] = 0.0;
    cvc.my[e] = 0.0;
    cvc.resid[e] = 0.0;
    binflow[e] = 0.0;

    const int l = edges.left[e];
    const int r = edges.right[e];

    auto load_cell = [&](int index) {
        return CellState{
            .conserved = {.h = cells.h[index], .hu = cells.hu[index], .hv = cells.hv[index]},
            .eta = cells.eta[index]};
    };

    if (l >= 0 && r >= 0) {
        const CellState left = load_cell(l);
        const CellState right = load_cell(r);
        const auto flux = hllc_normal_flux(left, right, edge_fields, normal, config.h_min);
        const bool assemble_wb_pairing =
            classify_edge(edge_fields.omega_edge, edge_fields.phi_e_n).wb_pairing_assembled;

        EdgeStepDiagnostics diagnostics{
            .mass_flux = flux.mass,
            .momentum_flux_n = flux.momentum_n,
            .momentum_x = flux.momentum_x,
            .momentum_y = flux.momentum_y,
        };
        if (assemble_wb_pairing) {
            const core::Real h_avg = 0.5 * (left.conserved.h + right.conserved.h);
            diagnostics.pressure_pairing = pressure_pairing_phi_t(cells.phi_t[l], cells.phi_t[r], h_avg);
            diagnostics.s_phi_t = s_phi_t_centered(cells.phi_t[l], cells.phi_t[r], h_avg);
            diagnostics.residual = diagnostics.pressure_pairing + diagnostics.s_phi_t;
        }

        CvcSideFluxes side_fluxes{
            .left_mass = flux.mass,
            .right_mass = flux.mass,
            .left_momentum_x = flux.momentum_x,
            .right_momentum_x = flux.momentum_x,
            .left_momentum_y = flux.momentum_y,
            .right_momentum_y = flux.momentum_y,
        };
        const core::Real phi_t_left = cells.phi_t[l];
        const core::Real phi_t_right = cells.phi_t[r];
        if (config.enable_cvc != 0 && assemble_wb_pairing && phi_t_left != phi_t_right) {
            side_fluxes = cvc_side_fluxes_unchecked(flux, phi_t_left, phi_t_right);
            if (side_fluxes.applied) {
                cvc.flag[e] = 1;
                cvc.mass[e] = config.dt * len * (
                    phi_t_left * (flux.mass - side_fluxes.left_mass)
                    + phi_t_right * (side_fluxes.right_mass - flux.mass));
                cvc.mx[e] = config.dt * len * (
                    phi_t_left * (flux.momentum_x - side_fluxes.left_momentum_x)
                    + phi_t_right * (side_fluxes.right_momentum_x - flux.momentum_x));
                cvc.my[e] = config.dt * len * (
                    phi_t_left * (flux.momentum_y - side_fluxes.left_momentum_y)
                    + phi_t_right * (side_fluxes.right_momentum_y - flux.momentum_y));
                cvc.resid[e] = hd::abs_(side_fluxes.storage_residual_after);
            }
        }

        contrib.mass[base] = -side_fluxes.left_mass * len;
        contrib.mass[base + 1] = side_fluxes.right_mass * len;
        contrib.fx[base] = -side_fluxes.left_momentum_x * len;
        contrib.fy[base] = -side_fluxes.left_momentum_y * len;
        contrib.fx[base + 1] = side_fluxes.right_momentum_x * len;
        contrib.fy[base + 1] = side_fluxes.right_momentum_y * len;
        if (assemble_wb_pairing) {
            const auto pair = reconstruct_hydrostatic_pair(left, right);
            const auto wb = well_balanced_edge_pairing(
                phi_t_left,
                phi_t_right,
                left.conserved.h,
                right.conserved.h,
                pair.left.conserved.h,
                pair.right.conserved.h,
                config.gravity);
            diagnostics.wb_pressure = wb.pressure_flux;
            diagnostics.s_topo = wb.s_topo_left;
            contrib.wx[base] = wb.left_normal * nx * len;
            contrib.wy[base] = wb.left_normal * ny * len;
            contrib.wx[base + 1] = -wb.right_normal * nx * len;
            contrib.wy[base + 1] = -wb.right_normal * ny * len;
        }
        ediag[e] = diagnostics;
        return;
    }

    const int inside_index = l >= 0 ? l : r;
    const bool inside_is_left = l >= 0;
    const int slot = inside_is_left ? 0 : 1;
    const int boundary_kind = edges.bkind[e];

    if (boundary_kind == kBoundaryWall || boundary_kind == kBoundaryDischargeInflow) {
        const core::Real h_inside = cells.h[inside_index];
        const core::Real wall_pressure = well_balanced_boundary_pressure(
            cells.phi_t[inside_index], h_inside, config.gravity);
        const core::Real q_inflow = boundary_kind == kBoundaryDischargeInflow
            ? edges.q_inflow[e]
            : 0.0;
        ediag[e] = EdgeStepDiagnostics{
            .mass_flux = inside_is_left ? -q_inflow : q_inflow,
            .momentum_flux_n = wall_pressure,
            .momentum_x = wall_pressure * nx,
            .momentum_y = wall_pressure * ny,
        };
        if (q_inflow != 0.0) {
            contrib.mass[base + slot] = q_inflow * len;
            binflow[e] = q_inflow * len;
        }
        const core::Real momentum_x = wall_pressure * nx * len;
        const core::Real momentum_y = wall_pressure * ny * len;
        contrib.fx[base + slot] = inside_is_left ? -momentum_x : momentum_x;
        contrib.fy[base + slot] = inside_is_left ? -momentum_y : momentum_y;
        return;
    }

    const CellState inside = load_cell(inside_index);
    const CellState outside = boundary_kind == kBoundaryWaterLevel
        ? water_level_outside_state(inside, edges.eta_bc[e])
        : open_boundary_outside_state(inside);
    const auto flux = hllc_normal_flux(
        inside_is_left ? inside : outside,
        inside_is_left ? outside : inside,
        edge_fields,
        normal,
        config.h_min);

    EdgeStepDiagnostics edge_diagnostics{
        .mass_flux = flux.mass,
        .momentum_flux_n = flux.momentum_n,
        .momentum_x = flux.momentum_x,
        .momentum_y = flux.momentum_y,
    };

    const core::Real integrated_flux = edge_diagnostics.mass_flux * len;
    contrib.mass[base + slot] = inside_is_left ? -integrated_flux : integrated_flux;
    contrib.fx[base + slot] = inside_is_left ? -flux.momentum_x * len : flux.momentum_x * len;
    contrib.fy[base + slot] = inside_is_left ? -flux.momentum_y * len : flux.momentum_y * len;

    if (boundary_kind == kBoundaryWaterLevel) {
        const bool assemble_wb_pairing =
            classify_edge(edge_fields.omega_edge, edge_fields.phi_e_n).wb_pairing_assembled;
        if (assemble_wb_pairing) {
            const CellState& left_state = inside_is_left ? inside : outside;
            const CellState& right_state = inside_is_left ? outside : inside;
            const auto pair = reconstruct_hydrostatic_pair(left_state, right_state);
            const core::Real phi_t_inside = cells.phi_t[inside_index];
            const auto wb = well_balanced_edge_pairing(
                phi_t_inside,
                phi_t_inside,
                left_state.conserved.h,
                right_state.conserved.h,
                pair.left.conserved.h,
                pair.right.conserved.h,
                config.gravity);
            edge_diagnostics.wb_pressure = wb.pressure_flux;
            edge_diagnostics.s_topo = wb.s_topo_left;
            if (inside_is_left) {
                contrib.wx[base + slot] = wb.left_normal * nx * len;
                contrib.wy[base + slot] = wb.left_normal * ny * len;
            } else {
                contrib.wx[base + slot] = -wb.right_normal * nx * len;
                contrib.wy[base + slot] = -wb.right_normal * ny * len;
            }
        }
    }
    ediag[e] = edge_diagnostics;
}

// ---------------------------------------------------------------------------
// Cell gather + state update + sources.
// ---------------------------------------------------------------------------

__global__ void k_residual_gather(CsrView csr, ContribView contrib, CellStepDiagnostics* cdiag, int cell_count) {
    const int c = blockIdx.x * blockDim.x + threadIdx.x;
    if (c >= cell_count) return;
    CellStepDiagnostics d{};
    for (int k = csr.offsets[c]; k < csr.offsets[c + 1]; ++k) {
        const int idx = 2 * csr.edge_index[k] + csr.slot[k];
        d.mass_residual += contrib.mass[idx];
        d.momentum_residual.x += contrib.fx[idx];
        d.momentum_residual.x += contrib.wx[idx];
        d.momentum_residual.y += contrib.fy[idx];
        d.momentum_residual.y += contrib.wy[idx];
    }
    cdiag[c] = d;
}

__global__ void k_state_update(
    CellsView cells,
    const CellStepDiagnostics* cdiag,
    double dt,
    double h_min,
    int* error_flag) {
    const int c = blockIdx.x * blockDim.x + threadIdx.x;
    if (c >= cells.cell_count) return;
    const double area = cells.area[c];
    // Depth update (mirrors apply_depth_update; checked wrapper conditions
    // re-created via the error flag).
    if (area > 0.0) {
        const core::Real h_before = cells.h[c];
        const core::Real dh = dt * cdiag[c].mass_residual / area;
        if (!hd::isfinite_(h_before) || h_before < 0.0 || !hd::isfinite_(dh)) {
            *error_flag = 1;
            return;
        }
        cells.h[c] = nonnegative_depth_after_increment_unchecked(h_before, dh);
    }
    // Momentum update (mirrors apply_momentum_update).
    if (area <= 0.0) {
        return;
    }
    if (!hd::isfinite_(cells.h[c]) || !hd::isfinite_(cells.hu[c]) || !hd::isfinite_(cells.hv[c])) {
        *error_flag = 1;
        return;
    }
    const ConservedState limited = apply_dry_cell_momentum_limit_unchecked(
        ConservedState{.h = cells.h[c], .hu = cells.hu[c], .hv = cells.hv[c]}, h_min);
    cells.h[c] = limited.h;
    cells.hu[c] = limited.hu;
    cells.hv[c] = limited.hv;
    if (limited.h <= h_min) {
        return;
    }
    cells.hu[c] += dt * cdiag[c].momentum_residual.x / area;
    cells.hv[c] += dt * cdiag[c].momentum_residual.y / area;
}

__global__ void k_sources(
    CellsView cells,
    const double* manning_n,
    const double* exchange_volume,
    double* applied_exchange,
    double dt,
    double h_min,
    double gravity,
    int* error_flag) {
    const int c = blockIdx.x * blockDim.x + threadIdx.x;
    if (c >= cells.cell_count) return;
    applied_exchange[c] = 0.0;
    const double area = cells.area[c];
    if (area <= 0.0) return;

    const core::Real phi_t = cells.phi_t[c];
    if (exchange_volume[c] != 0.0) {
        if (!hd::isfinite_(cells.h[c]) || cells.h[c] < 0.0) {
            *error_flag = 1;
            return;
        }
        const auto exchange = exchange_depth_increment_unchecked(
            exchange_volume[c], cells.h[c], phi_t, area);
        cells.h[c] += exchange.depth_increment;
        applied_exchange[c] = exchange.applied_volume;
    }
    if (!hd::isfinite_(cells.h[c]) || cells.h[c] < 0.0
        || !hd::isfinite_(cells.hu[c]) || !hd::isfinite_(cells.hv[c])) {
        *error_flag = 1;
        return;
    }
    const ConservedState result = apply_manning_friction_unchecked(
        apply_dry_cell_momentum_limit_unchecked(
            ConservedState{.h = cells.h[c], .hu = cells.hu[c], .hv = cells.hv[c]}, h_min),
        manning_n[c], dt, h_min, gravity);
    cells.h[c] = result.h;
    cells.hu[c] = result.hu;
    cells.hv[c] = result.hv;
}

// ---------------------------------------------------------------------------
// Ground runoff kernel (mirrors apply_ground_runoff_stage).
// ---------------------------------------------------------------------------

struct GroundView {
    const double* rainfall_rate{nullptr};
    const double* pervious_fraction{nullptr};
    const double* impervious_fraction{nullptr};
    const double* roof_fraction{nullptr};
    const double* initial_abstraction_capacity{nullptr};
    const double* depression_storage_capacity{nullptr};
    const double* roof_abstraction_capacity{nullptr};
    const double* roof_storage_capacity{nullptr};
    const SoilParams* soil{nullptr};  // pre-resolved per cell on the host
    double* cumulative_infiltration{nullptr};
    double* ponding_time{nullptr};
    double* abstraction_filled{nullptr};
    double* depression_storage_filled{nullptr};
    const double* roof_abstraction_filled{nullptr};
    const double* roof_pending_volume{nullptr};
    double* diag_rainfall{nullptr};
    double* diag_surface_added{nullptr};
    double* diag_ponded_infiltration{nullptr};
    double* diag_infiltration{nullptr};
    double* diag_abstraction{nullptr};
    double* diag_depression_delta{nullptr};
    double f_inf_floor{0.0};
    double dt{0.0};
};

__global__ void k_ground_runoff(CellsView cells, GroundView ground, int* error_flag) {
    const int c = blockIdx.x * blockDim.x + threadIdx.x;
    if (c >= cells.cell_count) return;
    ground.diag_rainfall[c] = 0.0;
    ground.diag_surface_added[c] = 0.0;
    ground.diag_ponded_infiltration[c] = 0.0;
    ground.diag_infiltration[c] = 0.0;
    ground.diag_abstraction[c] = 0.0;
    ground.diag_depression_delta[c] = 0.0;
    const double area = cells.area[c];
    if (area <= 0.0) return;
    const core::Real phi_t = cells.phi_t[c];

    if (!hd::isfinite_(cells.h[c]) || cells.h[c] < 0.0) {
        *error_flag = 1;
        return;
    }

    RunoffCellInputs cell_inputs;
    cell_inputs.rainfall_rate = ground.rainfall_rate[c];
    cell_inputs.phi_t = phi_t;
    cell_inputs.cell_area = area;
    cell_inputs.surface_depth = cells.h[c];

    RunoffCellParams params;
    params.pervious_fraction = ground.pervious_fraction[c];
    params.impervious_fraction = ground.impervious_fraction[c];
    params.roof_fraction = ground.roof_fraction[c];
    params.initial_abstraction_capacity = ground.initial_abstraction_capacity[c];
    params.depression_storage_capacity = ground.depression_storage_capacity[c];
    params.roof_abstraction_capacity = ground.roof_abstraction_capacity[c];
    params.roof_storage_capacity = ground.roof_storage_capacity[c];
    params.roof_drain_capacity = 0.0;
    params.soil = ground.soil[c];

    RunoffCellState cell_state;
    cell_state.cumulative_infiltration = ground.cumulative_infiltration[c];
    cell_state.ponding_time = ground.ponding_time[c];
    cell_state.abstraction_filled = ground.abstraction_filled[c];
    cell_state.depression_storage_filled = ground.depression_storage_filled[c];
    cell_state.roof_abstraction_filled = ground.roof_abstraction_filled[c];
    cell_state.roof_pending_volume = ground.roof_pending_volume[c];

    const GroundRunoffResult g = evaluate_ground_runoff_unchecked(
        cell_inputs, params, cell_state, ground.dt, ground.f_inf_floor);

    cells.h[c] += (g.surface_added_volume - g.ponded_infiltration_volume) / (phi_t * area);

    ground.cumulative_infiltration[c] = cell_state.cumulative_infiltration;
    ground.ponding_time[c] = cell_state.ponding_time;
    ground.abstraction_filled[c] = cell_state.abstraction_filled;
    ground.depression_storage_filled[c] = cell_state.depression_storage_filled;

    const core::Real area_ground =
        (params.pervious_fraction + params.impervious_fraction) * area;
    ground.diag_rainfall[c] = cell_inputs.rainfall_rate * ground.dt * area_ground;
    ground.diag_surface_added[c] = g.surface_added_volume;
    ground.diag_ponded_infiltration[c] = g.ponded_infiltration_volume;
    ground.diag_infiltration[c] = g.infiltration_volume;
    ground.diag_abstraction[c] = g.abstraction_volume;
    ground.diag_depression_delta[c] = g.depression_storage_delta_volume;
}

__global__ void k_perturb(double* values, int n) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    values[i] += 1.0;
}

// ---------------------------------------------------------------------------
// Host-side packing and validation helpers.
// ---------------------------------------------------------------------------

int boundary_kind_code(BoundaryKind kind) {
    switch (kind) {
        case BoundaryKind::Wall: return kBoundaryWall;
        case BoundaryKind::Open: return kBoundaryOpen;
        case BoundaryKind::DischargeInflow: return kBoundaryDischargeInflow;
        case BoundaryKind::WaterLevel: return kBoundaryWaterLevel;
    }
    return kBoundaryWall;
}

void validate_step_inputs_like_cpu(const mesh::Mesh& mesh, const SurfaceState& state, const StepConfig& config) {
    if (!std::isfinite(config.dt) || config.dt <= 0.0) {
        throw std::invalid_argument("step dt must be positive");
    }
    if (!std::isfinite(config.cfl_safety) || config.cfl_safety <= 0.0) {
        throw std::invalid_argument("CFL_safety must be positive");
    }
    if (!std::isfinite(config.c_rollback) || config.c_rollback <= 0.0) {
        throw std::invalid_argument("C_rollback must be positive");
    }
    if (!std::isfinite(config.h_min) || config.h_min < 0.0) {
        throw std::invalid_argument("h_min must be non-negative");
    }
    if (!std::isfinite(config.gravity) || config.gravity <= 0.0) {
        throw std::invalid_argument("gravity must be positive");
    }
    if (mesh.cells.empty()) {
        throw std::invalid_argument("step mesh must contain at least one cell");
    }
    validate_state_matches_mesh(state, mesh);
}

// Re-creates the exact set of CVC phi_t validations the CPU edge loop would
// perform (only wb-paired internal edges with a phi_t jump are validated).
void prevalidate_cvc_phi_t(
    const mesh::Mesh& mesh,
    const GeometryCache& geometry,
    const DpmFields& dpm_fields,
    const StepConfig& config) {
    if (!config.enable_cvc_spatial_phi_t_correction) return;
    for (std::size_t e = 0; e < mesh.edges.size(); ++e) {
        const auto& adjacency = geometry.edge_cells[e];
        if (!adjacency.is_internal()) continue;
        if (!classify_edge(dpm_fields.edges[e].omega_edge, dpm_fields.edges[e].phi_e_n).wb_pairing_assembled) {
            continue;
        }
        const core::Real phi_l = dpm_fields.cells[*adjacency.left].phi_t;
        const core::Real phi_r = dpm_fields.cells[*adjacency.right].phi_t;
        if (phi_l == phi_r) continue;
        if (!std::isfinite(phi_l) || phi_l <= 0.0) {
            throw std::invalid_argument("CVC left phi_t must be finite and positive");
        }
        if (!std::isfinite(phi_r) || phi_r <= 0.0) {
            throw std::invalid_argument("CVC right phi_t must be finite and positive");
        }
    }
}

// Re-creates the CPU checked-source failure surface on the host (exchange
// volume/phi_t domains for cells with a nonzero exchange, Manning roughness
// domain for every live cell).
void prevalidate_sources(
    const SourceTermFields& sources,
    const DpmFields& dpm_fields,
    const GeometryCache& geometry,
    std::size_t cell_count) {
    for (std::size_t c = 0; c < cell_count; ++c) {
        if (geometry.cell_areas[c] <= 0.0) continue;
        if (sources.exchange_volume[c] != 0.0) {
            if (!std::isfinite(sources.exchange_volume[c])) {
                throw std::invalid_argument("exchange volume must be finite");
            }
            const core::Real phi_t = dpm_fields.cells[c].phi_t;
            if (!std::isfinite(phi_t) || phi_t <= 0.0 || phi_t > 1.0) {
                throw std::invalid_argument("exchange phi_t must be finite and in (0, 1]");
            }
        }
        if (!std::isfinite(sources.manning_n[c]) || sources.manning_n[c] < 0.0) {
            throw std::invalid_argument("manning_n must be finite and non-negative");
        }
    }
}

// Re-creates the CPU checked-runoff failure surface on the host for every
// live cell (the CPU validates per visited cell; values here are host inputs).
void prevalidate_runoff(
    const RunoffStepInputs& inputs,
    const RunoffState& runoff_state,
    const DpmFields& dpm_fields,
    const GeometryCache& geometry,
    std::size_t cell_count,
    std::vector<SoilParams>& soil_out) {
    soil_out.resize(cell_count);
    for (std::size_t c = 0; c < cell_count; ++c) {
        soil_out[c] = SoilParams{};
        if (geometry.cell_areas[c] <= 0.0) continue;
        if (!std::isfinite(inputs.rainfall_rate[c]) || inputs.rainfall_rate[c] < 0.0) {
            throw std::invalid_argument("runoff rainfall_rate must be finite and non-negative");
        }
        const core::Real phi_t = dpm_fields.cells[c].phi_t;
        if (!std::isfinite(phi_t) || phi_t <= 0.0) {
            throw std::invalid_argument("runoff phi_t must be finite and positive");
        }
        const core::Real fp = inputs.fields.pervious_fraction[c];
        const core::Real fi = inputs.fields.impervious_fraction[c];
        const core::Real fr = inputs.fields.roof_fraction[c];
        if (!std::isfinite(fp) || !std::isfinite(fi) || !std::isfinite(fr) ||
            fp < 0.0 || fi < 0.0 || fr < 0.0 || fp > 1.0 || fi > 1.0 || fr > 1.0) {
            throw std::invalid_argument("runoff area fractions must be finite and in [0,1]");
        }
        if (fp + fi + fr > 1.0 + 1.0e-6) {
            throw std::invalid_argument("runoff area fractions must sum to <= 1");
        }
        if (!std::isfinite(inputs.fields.initial_abstraction_capacity[c])
            || inputs.fields.initial_abstraction_capacity[c] < 0.0) {
            throw std::invalid_argument("initial_abstraction_capacity must be finite and non-negative");
        }
        if (!std::isfinite(inputs.fields.depression_storage_capacity[c])
            || inputs.fields.depression_storage_capacity[c] < 0.0) {
            throw std::invalid_argument("depression_storage_capacity must be finite and non-negative");
        }
        // Fail-closed LUT lookup on the host (the CPU throws std::out_of_range
        // from lut.at mid-loop; the CUDA path resolves every cell up front).
        soil_out[c] = inputs.lut.at(inputs.fields.soil_type[c]);
        if (fp * geometry.cell_areas[c] > 0.0) {
            validate_soil_params(soil_out[c]);
            if (!std::isfinite(runoff_state.cumulative_infiltration[c])
                || runoff_state.cumulative_infiltration[c] < 0.0) {
                throw std::invalid_argument("green-ampt cumulative infiltration must be finite and non-negative");
            }
            if (!std::isfinite(inputs.f_inf_floor) || inputs.f_inf_floor <= 0.0) {
                throw std::invalid_argument("green-ampt f_inf_floor must be finite and positive");
            }
        }
    }
}

struct DeviceContext {
    int cell_count{0};
    int edge_count{0};

    DeviceArray<double> h, hu, hv, eta, phi_t, area;
    DeviceArray<double> nx, ny, len, e_phi, e_omega, e_q, e_eta_bc;
    DeviceArray<int> e_left, e_right, e_bkind;
    DeviceArray<int> csr_offsets, csr_edge, csr_slot;

    DeviceArray<double> te, cell_cfl;
    DeviceArray<double> c_mass, c_fx, c_fy, c_wx, c_wy;
    DeviceArray<int> cvc_flag;
    DeviceArray<double> cvc_mass, cvc_mx, cvc_my, cvc_resid;
    DeviceArray<double> binflow;
    DeviceArray<EdgeStepDiagnostics> ediag;
    DeviceArray<CellStepDiagnostics> cdiag;
    DeviceArray<double> applied_exchange;
    DeviceArray<int> error_flag;

    CellsView cells_view() const {
        return CellsView{
            .h = h.get(), .hu = hu.get(), .hv = hv.get(),
            .eta = eta.get(), .phi_t = phi_t.get(), .area = area.get(),
            .cell_count = cell_count};
    }
    EdgesView edges_view() const {
        return EdgesView{
            .nx = nx.get(), .ny = ny.get(), .len = len.get(),
            .left = e_left.get(), .right = e_right.get(), .bkind = e_bkind.get(),
            .phi_e_n = e_phi.get(), .omega = e_omega.get(),
            .q_inflow = e_q.get(), .eta_bc = e_eta_bc.get(),
            .edge_count = edge_count};
    }
    CsrView csr_view() const {
        return CsrView{.offsets = csr_offsets.get(), .edge_index = csr_edge.get(), .slot = csr_slot.get()};
    }
    ContribView contrib_view() const {
        return ContribView{.mass = c_mass.get(), .fx = c_fx.get(), .fy = c_fy.get(),
                           .wx = c_wx.get(), .wy = c_wy.get()};
    }
    CvcOutView cvc_view() const {
        return CvcOutView{.flag = cvc_flag.get(), .mass = cvc_mass.get(),
                          .mx = cvc_mx.get(), .my = cvc_my.get(), .resid = cvc_resid.get()};
    }
};

void ensure_device_or_throw() {
    if (!cuda_backend_device_available()) {
        throw std::runtime_error("surface2d CUDA backend: no CUDA device available");
    }
}

void check_device_error_flag(const DeviceContext& ctx) {
    int flag = 0;
    ctx.error_flag.download(&flag, 1);
    if (flag != 0) {
        // The caller's host state has not been committed yet, so unlike the
        // CPU reference (which can throw mid-mutation) the CUDA step is
        // atomic: nothing changed.
        throw std::invalid_argument(
            "surface2d CUDA backend: non-finite or negative state detected on device; step aborted, state unchanged");
    }
}

// Shared core. runoff/roof pointers select the overload semantics.
StepDiagnostics advance_cuda_core(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs* runoff_inputs,
    const RoofStepInputs* roof_inputs,
    RunoffState* runoff_state) {
    ensure_device_or_throw();

    // Same validation set and order as the corresponding CPU overloads.
    validate_step_inputs_like_cpu(mesh, state, config);
    validate_dpm_fields_match_mesh(dpm_fields, mesh);
    validate_boundary_conditions_match_mesh(boundary, mesh);
    validate_source_term_fields_match_mesh(sources, mesh);
    validate_geometry_cache_matches_mesh(geometry, mesh);
    if (runoff_inputs != nullptr) {
        validate_runoff_step_inputs_match_mesh(*runoff_inputs, mesh);
        if (roof_inputs != nullptr) {
            validate_roof_step_inputs_match_mesh(*roof_inputs, mesh);
        }
        validate_runoff_state_match_mesh(*runoff_state, mesh);
    }
    prevalidate_cvc_phi_t(mesh, geometry, dpm_fields, config);
    prevalidate_sources(sources, dpm_fields, geometry, mesh.cells.size());
    std::vector<SoilParams> soil_per_cell;
    if (runoff_inputs != nullptr) {
        prevalidate_runoff(*runoff_inputs, *runoff_state, dpm_fields, geometry, mesh.cells.size(), soil_per_cell);
    }

    const int cell_count = static_cast<int>(mesh.cells.size());
    const int edge_count = static_cast<int>(mesh.edges.size());

    // ---- Pack host SoA. ----
    std::vector<double> h(cell_count), hu(cell_count), hv(cell_count), eta(cell_count);
    std::vector<double> phi_t(cell_count), area(cell_count);
    for (int c = 0; c < cell_count; ++c) {
        h[c] = state.cells[c].conserved.h;
        hu[c] = state.cells[c].conserved.hu;
        hv[c] = state.cells[c].conserved.hv;
        eta[c] = state.cells[c].eta;
        phi_t[c] = dpm_fields.cells[c].phi_t;
        area[c] = geometry.cell_areas[c];
    }
    std::vector<double> nx(edge_count), ny(edge_count), len(edge_count);
    std::vector<double> e_phi(edge_count), e_omega(edge_count), e_q(edge_count), e_eta_bc(edge_count);
    std::vector<int> e_left(edge_count), e_right(edge_count), e_bkind(edge_count);
    for (int e = 0; e < edge_count; ++e) {
        const auto& edge = mesh.edges[e];
        const auto& adjacency = geometry.edge_cells[e];
        nx[e] = edge.normal.x;
        ny[e] = edge.normal.y;
        len[e] = edge.length;
        e_phi[e] = dpm_fields.edges[e].phi_e_n;
        e_omega[e] = dpm_fields.edges[e].omega_edge;
        e_q[e] = boundary.discharge_at(static_cast<std::size_t>(e));
        e_eta_bc[e] = static_cast<std::size_t>(e) < boundary.water_level.size()
            ? boundary.water_level[static_cast<std::size_t>(e)]
            : 0.0;
        e_left[e] = adjacency.left.has_value() ? static_cast<int>(*adjacency.left) : -1;
        e_right[e] = adjacency.right.has_value() ? static_cast<int>(*adjacency.right) : -1;
        e_bkind[e] = adjacency.is_internal()
            ? kBoundaryNone
            : boundary_kind_code(boundary.edges[static_cast<std::size_t>(e)]);
    }

    // Cell -> owned edge slots CSR, ascending edge index per cell (bitwise
    // CPU association order). Built by scanning edges in ascending order.
    std::vector<int> csr_offsets(static_cast<std::size_t>(cell_count) + 1, 0);
    for (int e = 0; e < edge_count; ++e) {
        if (e_left[e] >= 0) ++csr_offsets[static_cast<std::size_t>(e_left[e]) + 1];
        if (e_right[e] >= 0) ++csr_offsets[static_cast<std::size_t>(e_right[e]) + 1];
    }
    for (int c = 0; c < cell_count; ++c) {
        csr_offsets[static_cast<std::size_t>(c) + 1] += csr_offsets[static_cast<std::size_t>(c)];
    }
    std::vector<int> csr_edge(csr_offsets[static_cast<std::size_t>(cell_count)]);
    std::vector<int> csr_slot(csr_edge.size());
    {
        std::vector<int> cursor(csr_offsets.begin(), csr_offsets.end() - 1);
        for (int e = 0; e < edge_count; ++e) {
            if (e_left[e] >= 0) {
                const int at = cursor[static_cast<std::size_t>(e_left[e])]++;
                csr_edge[static_cast<std::size_t>(at)] = e;
                csr_slot[static_cast<std::size_t>(at)] = 0;
            }
            if (e_right[e] >= 0) {
                const int at = cursor[static_cast<std::size_t>(e_right[e])]++;
                csr_edge[static_cast<std::size_t>(at)] = e;
                csr_slot[static_cast<std::size_t>(at)] = 1;
            }
        }
    }

    // ---- Upload. ----
    DeviceContext ctx;
    ctx.cell_count = cell_count;
    ctx.edge_count = edge_count;
    ctx.h.allocate(h.size()); ctx.h.upload(h);
    ctx.hu.allocate(hu.size()); ctx.hu.upload(hu);
    ctx.hv.allocate(hv.size()); ctx.hv.upload(hv);
    ctx.eta.allocate(eta.size()); ctx.eta.upload(eta);
    ctx.phi_t.allocate(phi_t.size()); ctx.phi_t.upload(phi_t);
    ctx.area.allocate(area.size()); ctx.area.upload(area);
    if (edge_count > 0) {
        ctx.nx.allocate(nx.size()); ctx.nx.upload(nx);
        ctx.ny.allocate(ny.size()); ctx.ny.upload(ny);
        ctx.len.allocate(len.size()); ctx.len.upload(len);
        ctx.e_phi.allocate(e_phi.size()); ctx.e_phi.upload(e_phi);
        ctx.e_omega.allocate(e_omega.size()); ctx.e_omega.upload(e_omega);
        ctx.e_q.allocate(e_q.size()); ctx.e_q.upload(e_q);
        ctx.e_eta_bc.allocate(e_eta_bc.size()); ctx.e_eta_bc.upload(e_eta_bc);
        ctx.e_left.allocate(e_left.size()); ctx.e_left.upload(e_left);
        ctx.e_right.allocate(e_right.size()); ctx.e_right.upload(e_right);
        ctx.e_bkind.allocate(e_bkind.size()); ctx.e_bkind.upload(e_bkind);
        ctx.te.allocate(len.size());
        ctx.c_mass.allocate(2U * len.size());
        ctx.c_fx.allocate(2U * len.size());
        ctx.c_fy.allocate(2U * len.size());
        ctx.c_wx.allocate(2U * len.size());
        ctx.c_wy.allocate(2U * len.size());
        ctx.cvc_flag.allocate(len.size());
        ctx.cvc_mass.allocate(len.size());
        ctx.cvc_mx.allocate(len.size());
        ctx.cvc_my.allocate(len.size());
        ctx.cvc_resid.allocate(len.size());
        ctx.binflow.allocate(len.size());
        ctx.ediag.allocate(len.size());
    }
    ctx.csr_offsets.allocate(csr_offsets.size()); ctx.csr_offsets.upload(csr_offsets);
    if (!csr_edge.empty()) {
        ctx.csr_edge.allocate(csr_edge.size()); ctx.csr_edge.upload(csr_edge);
        ctx.csr_slot.allocate(csr_slot.size()); ctx.csr_slot.upload(csr_slot);
    }
    ctx.cell_cfl.allocate(h.size());
    ctx.cdiag.allocate(h.size());
    ctx.applied_exchange.allocate(h.size());
    ctx.error_flag.allocate(1);
    ctx.error_flag.zero(1);

    const int cell_grid = grid_for(cell_count);

    // ---- CFL (raw states, before any update; default 9.81 wave gravity
    // exactly like raw_cell_cfl). ----
    double max_cell_cfl = 0.0;
    if (edge_count > 0) {
        k_edge_cfl<<<grid_for(edge_count), kBlock>>>(ctx.cells_view(), ctx.edges_view(), config.dt, ctx.te.get());
        cuda_check(cudaGetLastError(), "k_edge_cfl launch");
        k_cfl_gather<<<cell_grid, kBlock>>>(ctx.csr_view(), ctx.te.get(), ctx.area.get(), ctx.cell_cfl.get(), cell_count);
        cuda_check(cudaGetLastError(), "k_cfl_gather launch");
        max_cell_cfl = reduce_max(ctx.cell_cfl, cell_count);
    }
    const bool rollback_required = max_cell_cfl > config.c_rollback;

    StepDiagnostics diagnostics{};
    diagnostics.cell_count = mesh.cells.size();
    diagnostics.edge_count = mesh.edges.size();
    diagnostics.max_cell_cfl = max_cell_cfl;
    diagnostics.rollback_required = rollback_required;
    diagnostics.cells.resize(mesh.cells.size());
    diagnostics.edges.resize(mesh.edges.size());

    // ---- Edge flux + residual gather (also run on rollback: the CPU loop
    // fills edge/cell diagnostics before the rollback early-return). ----
    if (edge_count > 0) {
        const FluxConfig flux_config{
            .h_min = config.h_min,
            .gravity = config.gravity,
            .dt = config.dt,
            .enable_cvc = config.enable_cvc_spatial_phi_t_correction ? 1 : 0,
        };
        k_edge_flux<<<grid_for(edge_count), kBlock>>>(
            ctx.cells_view(), ctx.edges_view(), flux_config,
            ctx.contrib_view(), ctx.cvc_view(), ctx.binflow.get(), ctx.ediag.get());
        cuda_check(cudaGetLastError(), "k_edge_flux launch");
        ctx.ediag.download(diagnostics.edges.data(), diagnostics.edges.size());
    }
    k_residual_gather<<<cell_grid, kBlock>>>(
        ctx.csr_view(), ctx.contrib_view(), ctx.cdiag.get(), cell_count);
    cuda_check(cudaGetLastError(), "k_residual_gather launch");
    ctx.cdiag.download(diagnostics.cells.data(), diagnostics.cells.size());

    if (rollback_required) {
        // CPU zeroes the CVC aggregates on rollback; ours were never summed.
        cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
        return diagnostics;
    }

    if (edge_count > 0) {
        diagnostics.count_phi_t_jump_events = reduce_count(ctx.cvc_flag, edge_count);
        diagnostics.cvc_mass_correction_volume = reduce_sum(ctx.cvc_mass, edge_count);
        diagnostics.cvc_momentum_correction_x = reduce_sum(ctx.cvc_mx, edge_count);
        diagnostics.cvc_momentum_correction_y = reduce_sum(ctx.cvc_my, edge_count);
        diagnostics.max_cvc_storage_residual_after = reduce_max(ctx.cvc_resid, edge_count);
        diagnostics.boundary_inflow_volume = reduce_sum(ctx.binflow, edge_count) * config.dt;
    }

    // ---- State update. ----
    k_state_update<<<cell_grid, kBlock>>>(
        ctx.cells_view(), ctx.cdiag.get(), config.dt, config.h_min, ctx.error_flag.get());
    cuda_check(cudaGetLastError(), "k_state_update launch");

    // ---- Ground runoff (device), then roof (host CPU stage). ----
    SurfaceState work = state;
    RunoffState runoff_work;
    if (runoff_inputs != nullptr) {
        runoff_work = *runoff_state;
        const int n = cell_count;
        DeviceArray<double> rain(n), fp(n), fi(n), fr(n), iac(n), dsc(n), rac(n), rsc(n);
        DeviceArray<SoilParams> soil(static_cast<std::size_t>(n));
        DeviceArray<double> cum(n), pond(n), absf(n), depf(n), rabs(n), rpend(n);
        DeviceArray<double> d_rain(n), d_surf(n), d_pinf(n), d_inf(n), d_abs(n), d_dep(n);
        rain.upload(runoff_inputs->rainfall_rate);
        fp.upload(runoff_inputs->fields.pervious_fraction);
        fi.upload(runoff_inputs->fields.impervious_fraction);
        fr.upload(runoff_inputs->fields.roof_fraction);
        iac.upload(runoff_inputs->fields.initial_abstraction_capacity);
        dsc.upload(runoff_inputs->fields.depression_storage_capacity);
        rac.upload(runoff_inputs->fields.roof_abstraction_capacity);
        rsc.upload(runoff_inputs->fields.roof_storage_capacity);
        soil.upload(soil_per_cell.data(), soil_per_cell.size());
        cum.upload(runoff_work.cumulative_infiltration);
        pond.upload(runoff_work.ponding_time);
        absf.upload(runoff_work.abstraction_filled);
        depf.upload(runoff_work.depression_storage_filled);
        rabs.upload(runoff_work.roof_abstraction_filled);
        rpend.upload(runoff_work.roof_pending_volume);

        GroundView ground{
            .rainfall_rate = rain.get(),
            .pervious_fraction = fp.get(),
            .impervious_fraction = fi.get(),
            .roof_fraction = fr.get(),
            .initial_abstraction_capacity = iac.get(),
            .depression_storage_capacity = dsc.get(),
            .roof_abstraction_capacity = rac.get(),
            .roof_storage_capacity = rsc.get(),
            .soil = soil.get(),
            .cumulative_infiltration = cum.get(),
            .ponding_time = pond.get(),
            .abstraction_filled = absf.get(),
            .depression_storage_filled = depf.get(),
            .roof_abstraction_filled = rabs.get(),
            .roof_pending_volume = rpend.get(),
            .diag_rainfall = d_rain.get(),
            .diag_surface_added = d_surf.get(),
            .diag_ponded_infiltration = d_pinf.get(),
            .diag_infiltration = d_inf.get(),
            .diag_abstraction = d_abs.get(),
            .diag_depression_delta = d_dep.get(),
            .f_inf_floor = runoff_inputs->f_inf_floor,
            .dt = config.dt,
        };
        k_ground_runoff<<<cell_grid, kBlock>>>(ctx.cells_view(), ground, ctx.error_flag.get());
        cuda_check(cudaGetLastError(), "k_ground_runoff launch");

        diagnostics.rainfall_volume = reduce_sum(d_rain, n);
        diagnostics.surface_added_volume = reduce_sum(d_surf, n);
        diagnostics.ponded_infiltration_volume = reduce_sum(d_pinf, n);
        diagnostics.infiltration_volume = reduce_sum(d_inf, n);
        diagnostics.abstraction_volume = reduce_sum(d_abs, n);
        diagnostics.depression_storage_delta_volume = reduce_sum(d_dep, n);

        cum.download(runoff_work.cumulative_infiltration.data(), runoff_work.cumulative_infiltration.size());
        pond.download(runoff_work.ponding_time.data(), runoff_work.ponding_time.size());
        absf.download(runoff_work.abstraction_filled.data(), runoff_work.abstraction_filled.size());
        depf.download(runoff_work.depression_storage_filled.data(), runoff_work.depression_storage_filled.size());
        cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize (ground)");
        check_device_error_flag(ctx);

        if (roof_inputs != nullptr) {
            // Roof stage stays on the host: its CouplingLib acceptance port is
            // a std::function coupling seam by design. Run the exact CPU stage
            // on the downloaded post-ground state, then re-upload h.
            std::vector<double> h_now(h.size());
            ctx.h.download(h_now.data(), h_now.size());
            for (int c = 0; c < cell_count; ++c) {
                work.cells[c].conserved.h = h_now[static_cast<std::size_t>(c)];
            }
            apply_roof_runoff_stage(
                work, config, dpm_fields, *runoff_inputs, *roof_inputs, runoff_work, geometry, diagnostics);
            for (int c = 0; c < cell_count; ++c) {
                h_now[static_cast<std::size_t>(c)] = work.cells[c].conserved.h;
            }
            ctx.h.upload(h_now);
        }
    }

    // ---- Coupling exchange + friction. ----
    DeviceArray<double> manning(h.size()), exch(h.size());
    manning.upload(sources.manning_n);
    exch.upload(sources.exchange_volume);
    k_sources<<<cell_grid, kBlock>>>(
        ctx.cells_view(), manning.get(), exch.get(), ctx.applied_exchange.get(),
        config.dt, config.h_min, config.gravity, ctx.error_flag.get());
    cuda_check(cudaGetLastError(), "k_sources launch");
    diagnostics.exchange_volume = reduce_sum(ctx.applied_exchange, cell_count);

    cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
    check_device_error_flag(ctx);

    // ---- Commit. ----
    std::vector<double> h_out(h.size()), hu_out(h.size()), hv_out(h.size());
    ctx.h.download(h_out.data(), h_out.size());
    ctx.hu.download(hu_out.data(), hu_out.size());
    ctx.hv.download(hv_out.data(), hv_out.size());
    for (int c = 0; c < cell_count; ++c) {
        work.cells[c].conserved.h = h_out[static_cast<std::size_t>(c)];
        work.cells[c].conserved.hu = hu_out[static_cast<std::size_t>(c)];
        work.cells[c].conserved.hv = hv_out[static_cast<std::size_t>(c)];
    }
    state = work;
    if (runoff_inputs != nullptr) {
        *runoff_state = runoff_work;
    }
    return diagnostics;
}

}  // namespace

bool cuda_backend_device_available() noexcept {
    int count = 0;
    const cudaError_t status = cudaGetDeviceCount(&count);
    return status == cudaSuccess && count > 0;
}

CudaBackendDeviceInfo cuda_backend_device_info() noexcept {
    CudaBackendDeviceInfo info{};
    if (!cuda_backend_device_available()) {
        return info;
    }
    cudaDeviceProp prop{};
    if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
        info.compute_capability_major = prop.major;
        info.compute_capability_minor = prop.minor;
        std::strncpy(info.device_name, prop.name, sizeof(info.device_name) - 1);
    }
    (void)cudaDriverGetVersion(&info.driver_version);
    (void)cudaRuntimeGetVersion(&info.runtime_version);
    return info;
}

bool cuda_snapshot_restore_roundtrip(const SurfaceState& state) {
    ensure_device_or_throw();
    const std::size_t n = state.cells.size();
    std::vector<double> packed(4U * n);
    for (std::size_t c = 0; c < n; ++c) {
        packed[4U * c] = state.cells[c].conserved.h;
        packed[4U * c + 1U] = state.cells[c].conserved.hu;
        packed[4U * c + 2U] = state.cells[c].conserved.hv;
        packed[4U * c + 3U] = state.cells[c].eta;
    }
    DeviceArray<double> live(packed.size());
    DeviceArray<double> snapshot(packed.size());
    live.upload(packed);
    snapshot.copy_from(live, packed.size());

    const int total = static_cast<int>(packed.size());
    if (total > 0) {
        k_perturb<<<grid_for(total), kBlock>>>(live.get(), total);
        cuda_check(cudaGetLastError(), "k_perturb launch");
    }
    live.copy_from(snapshot, packed.size());
    cuda_check(cudaDeviceSynchronize(), "cudaDeviceSynchronize (snapshot)");

    std::vector<double> restored(packed.size());
    live.download(restored.data(), restored.size());
    return packed.empty()
        || std::memcmp(packed.data(), restored.data(), packed.size() * sizeof(double)) == 0;
}

StepDiagnostics advance_one_step_cuda(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry) {
    return advance_cuda_core(
        mesh, state, config, dpm_fields, boundary, sources, geometry, nullptr, nullptr, nullptr);
}

StepDiagnostics advance_one_step_cuda(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    RunoffState& runoff_state) {
    return advance_cuda_core(
        mesh, state, config, dpm_fields, boundary, sources, geometry,
        &runoff_inputs, nullptr, &runoff_state);
}

StepDiagnostics advance_one_step_cuda(
    const mesh::Mesh& mesh,
    SurfaceState& state,
    const StepConfig& config,
    const DpmFields& dpm_fields,
    const BoundaryConditions& boundary,
    const SourceTermFields& sources,
    const GeometryCache& geometry,
    const RunoffStepInputs& runoff_inputs,
    const RoofStepInputs& roof_inputs,
    RunoffState& runoff_state) {
    return advance_cuda_core(
        mesh, state, config, dpm_fields, boundary, sources, geometry,
        &runoff_inputs, &roof_inputs, &runoff_state);
}

}  // namespace scau::surface2d
