// SCAU-UFM M280 CUDA determinism spike (standalone; NOT part of the main
// graph). Purpose: prove on the governed GPU that a solver-shaped double
// precision update plus a FIXED-ORDER reduction is bitwise deterministic
// across repeated runs, and matches a sequential CPU reference exactly when
// the summation order is fixed. This de-risks the core G9 requirement
// before any CUDA code enters libs/surface2d (M266 contract).
//
// Deliberately mirrors the solver's accumulation shape:
//   per-cell: state update from per-edge fluxes (each cell owns its edges'
//   contributions in a fixed order -> no atomics, no race);
//   diagnostic: whole-domain storage sum via a fixed-order tree reduction
//   (sequential pairwise within a block, fixed block order across grid).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <cuda_runtime.h>

namespace {

constexpr int kCells = 1 << 20;  // ~1M cells
constexpr int kEdgesPerCell = 4;
constexpr int kSteps = 25;
constexpr int kRepeats = 5;
constexpr int kBlock = 256;

#define CUDA_CHECK(call)                                                      \
    do {                                                                      \
        const cudaError_t status = (call);                                    \
        if (status != cudaSuccess) {                                          \
            std::fprintf(stderr, "CUDA error %s at %s:%d\n",                  \
                         cudaGetErrorString(status), __FILE__, __LINE__);     \
            std::exit(1);                                                     \
        }                                                                     \
    } while (0)

// Solver-shaped per-cell update: every cell reads its own edge slots in a
// FIXED order and updates its state. Gather-by-owner (no atomics) is the
// deterministic pattern the future CUDA backend must use.
__global__ void update_cells(const double* edge_flux, double* h, int cells) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= cells) return;
    double dh = 0.0;
    // Fixed order over the cell's edges: matches CPU reference exactly.
    for (int e = 0; e < kEdgesPerCell; ++e) {
        dh += edge_flux[static_cast<size_t>(i) * kEdgesPerCell + e];
    }
    h[i] += 1.0e-6 * dh * (1.0 + 1.0e-3 * h[i]);
}

// Fixed-order block reduction: sequential pairwise tree inside the block
// (identical association every launch), one partial per block, partials
// combined SEQUENTIALLY on the host in block order. No atomics anywhere.
__global__ void reduce_blocks(const double* h, double* partials, int cells) {
    __shared__ double shared[kBlock];
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    shared[threadIdx.x] = (i < cells) ? h[i] : 0.0;
    __syncthreads();
    for (int stride = kBlock / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared[threadIdx.x] += shared[threadIdx.x + stride];
        }
        __syncthreads();
    }
    if (threadIdx.x == 0) partials[blockIdx.x] = shared[0];
}

struct RunResult {
    std::vector<double> h;
    double storage;
};

RunResult run_device(const std::vector<double>& edge_flux,
                     const std::vector<double>& h0) {
    const int grid = (kCells + kBlock - 1) / kBlock;
    double *d_flux = nullptr, *d_h = nullptr, *d_partials = nullptr;
    CUDA_CHECK(cudaMalloc(&d_flux, edge_flux.size() * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_h, h0.size() * sizeof(double)));
    CUDA_CHECK(cudaMalloc(&d_partials, static_cast<size_t>(grid) * sizeof(double)));
    CUDA_CHECK(cudaMemcpy(d_flux, edge_flux.data(),
                          edge_flux.size() * sizeof(double), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_h, h0.data(), h0.size() * sizeof(double),
                          cudaMemcpyHostToDevice));

    for (int step = 0; step < kSteps; ++step) {
        update_cells<<<grid, kBlock>>>(d_flux, d_h, kCells);
        CUDA_CHECK(cudaGetLastError());
    }
    reduce_blocks<<<grid, kBlock>>>(d_h, d_partials, kCells);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    RunResult result;
    result.h.resize(kCells);
    std::vector<double> partials(grid);
    CUDA_CHECK(cudaMemcpy(result.h.data(), d_h, kCells * sizeof(double),
                          cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(partials.data(), d_partials, grid * sizeof(double),
                          cudaMemcpyDeviceToHost));
    // Host combines block partials sequentially in block order (fixed).
    double storage = 0.0;
    for (int b = 0; b < grid; ++b) storage += partials[b];
    result.storage = storage;
    CUDA_CHECK(cudaFree(d_flux));
    CUDA_CHECK(cudaFree(d_h));
    CUDA_CHECK(cudaFree(d_partials));
    return result;
}

// CPU reference replicating the EXACT association order of the device path:
// per-cell fixed edge order; storage via the same block-tree association.
RunResult run_cpu_reference(const std::vector<double>& edge_flux,
                            const std::vector<double>& h0) {
    RunResult result;
    result.h = h0;
    for (int step = 0; step < kSteps; ++step) {
        for (int i = 0; i < kCells; ++i) {
            double dh = 0.0;
            for (int e = 0; e < kEdgesPerCell; ++e) {
                dh += edge_flux[static_cast<size_t>(i) * kEdgesPerCell + e];
            }
            result.h[i] += 1.0e-6 * dh * (1.0 + 1.0e-3 * result.h[i]);
        }
    }
    const int grid = (kCells + kBlock - 1) / kBlock;
    double storage = 0.0;
    for (int b = 0; b < grid; ++b) {
        double shared[kBlock];
        for (int t = 0; t < kBlock; ++t) {
            const int i = b * kBlock + t;
            shared[t] = (i < kCells) ? result.h[i] : 0.0;
        }
        for (int stride = kBlock / 2; stride > 0; stride >>= 1) {
            for (int t = 0; t < stride; ++t) shared[t] += shared[t + stride];
        }
        storage += shared[0];
    }
    result.storage = storage;
    return result;
}

}  // namespace

int main() {
    cudaDeviceProp prop{};
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    int driver = 0, runtime = 0;
    CUDA_CHECK(cudaDriverGetVersion(&driver));
    CUDA_CHECK(cudaRuntimeGetVersion(&runtime));
    std::printf("[env] device=%s cc=%d.%d driver=%d runtime=%d cells=%d steps=%d\n",
                prop.name, prop.major, prop.minor, driver, runtime, kCells, kSteps);

    // Deterministic pseudo-random fixture (no external RNG state).
    std::vector<double> edge_flux(static_cast<size_t>(kCells) * kEdgesPerCell);
    std::vector<double> h0(kCells);
    for (int i = 0; i < kCells; ++i) {
        h0[i] = 1.0 + 1.0e-3 * ((i * 2654435761u % 1000) / 1000.0);
        for (int e = 0; e < kEdgesPerCell; ++e) {
            const unsigned mix = (static_cast<unsigned>(i) * 40503u + e * 9973u) % 2000;
            edge_flux[static_cast<size_t>(i) * kEdgesPerCell + e] =
                (static_cast<double>(mix) - 1000.0) / 1.0e6;
        }
    }

    const RunResult first = run_device(edge_flux, h0);
    bool repeat_bitwise = true;
    for (int r = 1; r < kRepeats; ++r) {
        const RunResult repeat = run_device(edge_flux, h0);
        if (std::memcmp(repeat.h.data(), first.h.data(),
                        kCells * sizeof(double)) != 0 ||
            std::memcmp(&repeat.storage, &first.storage, sizeof(double)) != 0) {
            repeat_bitwise = false;
        }
    }
    std::printf("[repeat] runs=%d bitwise_identical=%s\n", kRepeats,
                repeat_bitwise ? "true" : "false");

    const RunResult reference = run_cpu_reference(edge_flux, h0);
    const bool state_bitwise = std::memcmp(reference.h.data(), first.h.data(),
                                           kCells * sizeof(double)) == 0;
    const bool storage_bitwise =
        std::memcmp(&reference.storage, &first.storage, sizeof(double)) == 0;
    std::printf("[cpu-match] state_bitwise=%s storage_bitwise=%s storage=%.17g\n",
                state_bitwise ? "true" : "false",
                storage_bitwise ? "true" : "false", first.storage);

    const bool pass = repeat_bitwise && state_bitwise && storage_bitwise;
    std::printf("[verdict] %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
