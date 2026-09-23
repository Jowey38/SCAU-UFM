#include "surface_timeseries.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <netcdf.h>

#include "coupling/driver/checkpoint_payloads.hpp"

namespace scau::apps::sim_driver {
namespace {
void check(int rc) {
    if (rc != NC_NOERR) throw std::runtime_error(std::string("surface timeseries: ") + nc_strerror(rc));
}
void text(int file, int var, const char* name, const std::string& value) {
    check(nc_put_att_text(file, var, name, value.size(), value.c_str()));
}
std::string json_escape(const std::string& value) {
    std::string out;
    for (const char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}
}

std::string hash_file_bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot hash missing file: " + path.string());
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    char buffer[1 << 16];
    while (stream.read(buffer, sizeof(buffer)) || stream.gcount() > 0) {
        for (std::streamsize i = 0; i < stream.gcount(); ++i) {
            hash ^= static_cast<std::uint8_t>(buffer[i]);
            hash *= 0x100000001B3ULL;
        }
    }
    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

SurfaceTimeseries::SurfaceTimeseries(const RuntimeConfig& config)
    : h_wet_(config.h_wet), source_stcf_(config.stcf_case_path),
      source_stcf_hash_(hash_file_bytes(config.stcf_case_path)),
      output_(config.surface_timeseries_path),
      partial_(config.surface_timeseries_path + ".partial") {
    // Preflight every destination of the artifact set (output, manifest and
    // both staging files) BEFORE any engine advances, so a publication
    // conflict cannot surface only after a long run.
    const std::filesystem::path manifest = output_.string() + ".manifest.json";
    const std::filesystem::path manifest_partial = manifest.string() + ".partial";
    for (const auto& p : {output_, partial_, manifest, manifest_partial}) {
        if (std::filesystem::exists(p)) {
            throw std::invalid_argument("surface timeseries artifact already exists: " + p.string());
        }
    }
    if (!output_.parent_path().empty()) std::filesystem::create_directories(output_.parent_path());
    std::filesystem::copy_file(config.stcf_case_path, partial_);
    try {
        check(nc_open(partial_.string().c_str(), NC_WRITE, &file_));
        int cell_dim = -1;
        check(nc_inq_dimid(file_, "cell", &cell_dim));
        check(nc_inq_dimlen(file_, cell_dim, &cells_));
        check(nc_redef(file_));
        int time_dim = -1;
        check(nc_def_dim(file_, "time", NC_UNLIMITED, &time_dim));
        check(nc_def_var(file_, "time", NC_DOUBLE, 1, &time_dim, &time_var_));
        text(file_, time_var_, "units", "s");
        text(file_, time_var_, "long_name", "model logical time");
        text(file_, NC_GLOBAL, "time_origin_semantics", "model logical seconds; no civil-date interpretation");
        text(file_, NC_GLOBAL, "surface_results_schema_version", "2");
        text(file_, NC_GLOBAL, "run_status", "incomplete");
        text(file_, NC_GLOBAL, "source_stcf", source_stcf_);
        text(file_, NC_GLOBAL, "source_stcf_hash", source_stcf_hash_);
        text(file_, NC_GLOBAL, "output_every_epochs", std::to_string(config.surface_output_every_epochs));
        check(nc_put_att_double(file_, NC_GLOBAL, "h_wet", NC_DOUBLE, 1, &h_wet_));
        check(nc_put_att_double(file_, NC_GLOBAL, "dt_couple", NC_DOUBLE, 1, &config.dt_couple));
        check(nc_put_att_double(file_, NC_GLOBAL, "start_time", NC_DOUBLE, 1, &config.start_time));
        check(nc_put_att_double(file_, NC_GLOBAL, "end_time", NC_DOUBLE, 1, &config.end_time));
        text(file_, NC_GLOBAL, "engine_participation", config.enable_dflowfm ? "surface,swmm,dflowfm" : "surface,swmm");
        const char* names[] = {"h", "eta", "hu", "hv", "wet_mask"};
        const char* units[] = {"m", "m", "m2 s-1", "m2 s-1", "1"};
        const int dims[] = {time_dim, cell_dim};
        for (std::size_t i = 0; i < vars_.size(); ++i) {
            check(nc_def_var(file_, names[i], NC_DOUBLE, 2, dims, &vars_[i]));
            text(file_, vars_[i], "units", units[i]);
            text(file_, vars_[i], "mesh", "mesh2");
            text(file_, vars_[i], "location", "face");
        }
        check(nc_enddef(file_));
    } catch (...) {
        if (file_ >= 0) nc_close(file_);
        file_ = -1;
        throw;
    }
}

SurfaceTimeseries::~SurfaceTimeseries() {
    if (file_ >= 0) nc_close(file_);
}

void SurfaceTimeseries::append(double time, const surface2d::SurfaceState& state) {
    if (file_ < 0 || state.cells.size() != cells_ || !std::isfinite(time) ||
        (frames_ > 0 && time <= previous_time_)) {
        throw std::invalid_argument("invalid surface timeseries frame shape/time");
    }
    std::array<std::vector<double>, 5> values;
    for (auto& v : values) v.reserve(cells_);
    for (const auto& cell : state.cells) {
        const double h = cell.conserved.h;
        const double eta = cell.eta;
        const double hu = cell.conserved.hu;
        const double hv = cell.conserved.hv;
        if (!std::isfinite(h) || h < 0.0 || !std::isfinite(eta) ||
            !std::isfinite(hu) || !std::isfinite(hv)) {
            throw std::invalid_argument("non-finite or negative-depth surface timeseries frame");
        }
        values[0].push_back(h); values[1].push_back(eta);
        values[2].push_back(hu); values[3].push_back(hv);
        values[4].push_back(h >= h_wet_ ? 1.0 : 0.0);
    }
    const std::size_t start[] = {frames_, 0};
    const std::size_t count[] = {1, cells_};
    check(nc_put_var1_double(file_, time_var_, &frames_, &time));
    for (std::size_t i = 0; i < values.size(); ++i) {
        check(nc_put_vara_double(file_, vars_[i], start, count, values[i].data()));
    }
    check(nc_sync(file_));
    previous_time_ = time;
    ++frames_;
}

void SurfaceTimeseries::complete(const surface2d::SurfaceState& final_state,
                                 std::size_t committed_epochs) {
    if (file_ < 0 || frames_ == 0) throw std::logic_error("cannot complete empty surface timeseries");
    const std::string final_hash = coupling::driver::hash_surface_state(final_state);
    check(nc_redef(file_));
    text(file_, NC_GLOBAL, "run_status", "completed");
    text(file_, NC_GLOBAL, "final_surface_state_hash", final_hash);
    text(file_, NC_GLOBAL, "committed_epochs", std::to_string(committed_epochs));
    check(nc_enddef(file_));
    const int rc = nc_close(file_);
    file_ = -1;
    check(rc);

    // Artifact-set publish protocol. Both files are fully written, flushed,
    // closed and re-read from their staging names; only then are they
    // published, NetCDF first (hard link = atomic, never overwrites), manifest
    // second. If manifest publication fails the NetCDF is withdrawn so the set
    // is never half-published.
    const std::filesystem::path manifest = output_.string() + ".manifest.json";
    const std::filesystem::path manifest_partial = manifest.string() + ".partial";
    const std::string output_hash = hash_file_bytes(partial_);   // == published bytes (hard link)
    std::string body;
    body += "{\n";
    body += "  \"manifest_schema_version\": 1,\n";
    body += "  \"output\": \"" + json_escape(output_.filename().string()) + "\",\n";
    body += "  \"output_hash\": \"" + output_hash + "\",\n";
    body += "  \"source_stcf\": \"" + json_escape(source_stcf_) + "\",\n";
    body += "  \"source_stcf_hash\": \"" + source_stcf_hash_ + "\",\n";
    body += "  \"final_surface_state_hash\": \"" + final_hash + "\",\n";
    body += "  \"committed_epochs\": " + std::to_string(committed_epochs) + ",\n";
    body += "  \"frames\": " + std::to_string(frames_) + "\n";
    body += "}\n";
    {
        std::ofstream out(manifest_partial, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot stage manifest: " + manifest_partial.string());
        out << body;
        out.flush();
        if (!out) throw std::runtime_error("manifest staging write failed: " + manifest_partial.string());
        out.close();
        if (out.fail()) throw std::runtime_error("manifest staging close failed: " + manifest_partial.string());
    }
    {
        std::ifstream verify(manifest_partial, std::ios::binary);
        const std::string on_disk((std::istreambuf_iterator<char>(verify)), {});
        if (on_disk != body) throw std::runtime_error("manifest staging verify failed: " + manifest_partial.string());
    }

    std::filesystem::create_hard_link(partial_, output_);
    try {
        std::filesystem::rename(manifest_partial, manifest);   // same directory: atomic
    } catch (...) {
        std::error_code ignore;
        std::filesystem::remove(output_, ignore);               // withdraw the half-published set
        throw;
    }
    std::filesystem::remove(partial_);
}

}  // namespace scau::apps::sim_driver
