#include "coupling/driver/checkpoint_payloads.hpp"

#include <bit>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace scau::coupling::driver {

namespace {

class Fnv1a64 {
public:
    void mix_u64(std::uint64_t value) noexcept {
        for (int byte = 0; byte < 8; ++byte) {
            hash_ ^= (value >> (byte * 8)) & 0xFFU;
            hash_ *= 0x100000001B3ULL;
        }
    }

    void mix_double(double value) noexcept {
        mix_u64(std::bit_cast<std::uint64_t>(value));
    }

    void mix_size(std::size_t value) noexcept {
        mix_u64(static_cast<std::uint64_t>(value));
    }

    void mix_string(const std::string& value) noexcept {
        mix_size(value.size());
        for (const char character : value) {
            hash_ ^= static_cast<std::uint8_t>(character);
            hash_ *= 0x100000001B3ULL;
        }
    }

    [[nodiscard]] std::string digest() const {
        std::ostringstream out;
        out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash_;
        return out.str();
    }

private:
    std::uint64_t hash_{0xCBF29CE484222325ULL};
};

void mix_endpoint(Fnv1a64& hasher, const core::SharedExchangeEndpoint& endpoint) {
    hasher.mix_u64(static_cast<std::uint64_t>(endpoint.engine));
    hasher.mix_size(endpoint.node_id);
}

std::string memory_reference(const char* module_name, std::uint64_t epoch) {
    std::ostringstream out;
    out << "memory://" << module_name << "/" << epoch;
    return out.str();
}

}  // namespace

std::string hash_surface_state(const surface2d::SurfaceState& state) {
    Fnv1a64 hasher;
    hasher.mix_size(state.cells.size());
    for (const surface2d::CellState& cell : state.cells) {
        hasher.mix_double(static_cast<double>(cell.conserved.h));
        hasher.mix_double(static_cast<double>(cell.conserved.hu));
        hasher.mix_double(static_cast<double>(cell.conserved.hv));
        hasher.mix_double(static_cast<double>(cell.eta));
    }
    return hasher.digest();
}

std::string hash_coupling_snapshot(const core::CouplingSnapshot& snapshot) {
    Fnv1a64 hasher;
    hasher.mix_size(snapshot.cells().size());
    for (const core::ExchangeCellState& cell : snapshot.cells()) {
        hasher.mix_double(cell.volume);
        hasher.mix_double(cell.mass_deficit_account.volume);
        hasher.mix_size(cell.mass_deficit_account.age_steps);
        hasher.mix_double(cell.phi_t);
        hasher.mix_double(cell.h);
        hasher.mix_double(cell.area);
        hasher.mix_size(cell.shared_deficit_accounts.size());
        for (const core::SharedExchangeEndpointDeficit& deficit : cell.shared_deficit_accounts) {
            mix_endpoint(hasher, deficit.endpoint);
            hasher.mix_double(deficit.mass_deficit_account.volume);
            hasher.mix_size(deficit.mass_deficit_account.age_steps);
        }
    }
    hasher.mix_size(snapshot.runtime_counters().count_drain_split);
    hasher.mix_size(snapshot.runtime_counters().count_negative_depth_fix);
    hasher.mix_size(snapshot.runtime_counters().count_writeoff_events);
    hasher.mix_double(snapshot.runtime_counters().count_writeoff_volume_total);
    hasher.mix_size(snapshot.pending_events().size());
    for (const core::CouplingEvent& event : snapshot.pending_events()) {
        hasher.mix_size(event.exchange_cell_index);
        hasher.mix_u64(static_cast<std::uint64_t>(event.direction));
        hasher.mix_double(event.volume_delta);
        hasher.mix_double(event.unmet_volume);
        hasher.mix_double(event.repayment_volume);
        hasher.mix_size(event.shared_endpoint_events.size());
        for (const core::SharedExchangeEndpointEvent& endpoint_event :
             event.shared_endpoint_events) {
            mix_endpoint(hasher, endpoint_event.endpoint);
            hasher.mix_double(endpoint_event.unmet_volume);
            hasher.mix_double(endpoint_event.repayment_volume);
        }
    }
    return hasher.digest();
}

PreparedModuleCheckpoint prepare_surface2d_checkpoint(
    const surface2d::SurfaceState& state,
    std::uint64_t epoch,
    double logical_time) {
    PreparedModuleCheckpoint record{};
    record.module = CheckpointModule::surface2d;
    record.epoch = epoch;
    record.logical_time = logical_time;
    record.schema_version = "surface2d-state-v1";
    record.content_hash = hash_surface_state(state);
    record.payload_reference = memory_reference("surface2d", epoch);
    return record;
}

PreparedModuleCheckpoint prepare_coupling_checkpoint(
    const core::CouplingSnapshot& snapshot,
    std::uint64_t epoch,
    double logical_time) {
    PreparedModuleCheckpoint record{};
    record.module = CheckpointModule::coupling;
    record.epoch = epoch;
    record.logical_time = logical_time;
    record.schema_version = "coupling-snapshot-v1";
    record.content_hash = hash_coupling_snapshot(snapshot);
    record.payload_reference = memory_reference("coupling", epoch);
    return record;
}

PreparedModuleCheckpoint prepare_sim_driver_checkpoint(
    std::size_t completed_coupling_steps,
    std::uint64_t epoch,
    double logical_time) {
    Fnv1a64 hasher;
    hasher.mix_size(completed_coupling_steps);
    hasher.mix_u64(epoch);
    hasher.mix_double(logical_time);

    PreparedModuleCheckpoint record{};
    record.module = CheckpointModule::sim_driver;
    record.epoch = epoch;
    record.logical_time = logical_time;
    record.schema_version = "sim-driver-progress-v1";
    record.content_hash = hasher.digest();
    record.payload_reference = memory_reference("sim_driver", epoch);
    return record;
}

PreparedModuleCheckpoint prepare_swmm_checkpoint(
    double swmm_elapsed_time,
    std::uint64_t epoch,
    double logical_time) {
    Fnv1a64 hasher;
    hasher.mix_double(swmm_elapsed_time);
    hasher.mix_u64(epoch);

    PreparedModuleCheckpoint record{};
    record.module = CheckpointModule::swmm;
    record.epoch = epoch;
    record.logical_time = logical_time;
    record.schema_version = "swmm-noreload-v1";
    record.content_hash = hasher.digest();
    record.payload_reference = memory_reference("swmm", epoch);
    return record;
}

PreparedModuleCheckpoint prepare_dflowfm_checkpoint_record(
    double dflowfm_elapsed_time,
    const DFlowFMCheckpoint* file_checkpoint,
    std::uint64_t epoch,
    double logical_time) {
    Fnv1a64 hasher;
    hasher.mix_double(dflowfm_elapsed_time);
    hasher.mix_u64(epoch);

    PreparedModuleCheckpoint record{};
    record.module = CheckpointModule::dflowfm;
    record.epoch = epoch;
    record.logical_time = logical_time;
    if (file_checkpoint != nullptr) {
        validate_dflowfm_checkpoint(*file_checkpoint);
        hasher.mix_double(file_checkpoint->logical_time);
        hasher.mix_string(file_checkpoint->restart_datetime);
        hasher.mix_string(file_checkpoint->model_fingerprint);
        record.schema_version = "dflowfm-restart-file-v1";
        record.payload_reference = file_checkpoint->state_file.string();
    } else {
        record.schema_version = "dflowfm-elapsed-v1";
        record.payload_reference = memory_reference("dflowfm", epoch);
    }
    record.content_hash = hasher.digest();
    return record;
}

}  // namespace scau::coupling::driver
