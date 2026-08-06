#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_loop.hpp"
#include "sim_driver.hpp"

// M270 deterministic whole-system audit integration. The two test engines
// expose complete physical-storage ledgers:
//   SWMM storage += node lateral * dt - node overflow * dt
//   D-Flow vol1  += lateral discharge * dt
// so internal transfers can be audited without counting them as external
// sources/sinks. Head-driven requests create/repay endpoint deficits; deficits
// remain parallel obligation evidence, never physical storage.

namespace {

namespace sim = scau::apps::sim_driver;

std::string case_path_from_env() {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    if (::_dupenv_s(&value, &size, "SCAU_SIM_RUN_LOOP_CASE") != 0 || value == nullptr) {
        throw std::runtime_error("SCAU_SIM_RUN_LOOP_CASE is not set");
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv("SCAU_SIM_RUN_LOOP_CASE");
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error("SCAU_SIM_RUN_LOOP_CASE is not set");
    }
    return value;
#endif
}

class StorageTrackingSwmm final : public scau::coupling::drainage::ISwmmEngine {
public:
    void initialize(const std::string&) override {
        initialized_ = true;
        elapsed_ = 0.0;
        storage_ = 0.0;
    }
    void step(double dt) override {
        require_initialized();
        for (const auto& [node, q] : laterals_) {
            const double overflow = overflows_.contains(node) ? overflows_.at(node) : 0.0;
            storage_ += (q - overflow) * dt;
        }
        if (storage_ < -1.0e-12) {
            throw std::runtime_error("tracking SWMM storage became negative");
        }
        if (storage_ < 0.0) storage_ = 0.0;
        elapsed_ += dt;
    }
    void finalize() override { initialized_ = false; }
    [[nodiscard]] double get_node_head(int) const override {
        require_initialized();
        return 0.0;
    }
    [[nodiscard]] double get_node_lateral_inflow(int node) const override {
        require_initialized();
        const auto found = laterals_.find(node);
        return found == laterals_.end() ? 0.0 : found->second;
    }
    void set_node_lateral_inflow(int node, double q) override {
        require_initialized();
        laterals_[node] = q;
    }
    [[nodiscard]] double get_node_inflow(int) const override {
        require_initialized();
        return 0.0;
    }
    [[nodiscard]] double get_node_overflow(int node) const override {
        require_initialized();
        const auto found = overflows_.find(node);
        return found == overflows_.end() ? 0.0 : found->second;
    }
    void set_outfall_stage(int, double) override { require_initialized(); }
    [[nodiscard]] double get_link_flow(int) const override {
        require_initialized();
        return 0.0;
    }
    [[nodiscard]] bool is_surcharged(int) const override {
        require_initialized();
        return false;
    }
    [[nodiscard]] double elapsed_time() const noexcept { return elapsed_; }
    [[nodiscard]] double storage() const noexcept { return storage_; }

private:
    void require_initialized() const {
        if (!initialized_) throw std::logic_error("tracking SWMM is not initialized");
    }
    bool initialized_{false};
    double elapsed_{0.0};
    double storage_{0.0};
    std::unordered_map<int, double> laterals_{};
    std::unordered_map<int, double> overflows_{};
};

class StorageTrackingDFlow final : public scau::coupling::river::IDFlowFMEngine {
public:
    void initialize(const std::string&) override {
        initialized_ = true;
        elapsed_ = 0.0;
        storage_ = 100.0;
        lateral_q_ = 0.0;
    }
    void update(double dt) override {
        require_initialized();
        storage_ += lateral_q_ * dt;
        elapsed_ += dt;
    }
    void finalize() override { initialized_ = false; }
    [[nodiscard]] double get_value(const std::string& var_name, int) const override {
        require_initialized();
        if (var_name == "water_level") return 0.2;
        if (var_name.find("water_discharge") != std::string::npos) return lateral_q_;
        return 0.0;
    }
    [[nodiscard]] std::vector<double> get_rank1_double_values(
        const std::string& var_name) const override {
        require_initialized();
        if (var_name != "vol1") {
            throw std::invalid_argument("tracking D-Flow exposes only vol1 rank-1 storage");
        }
        return {storage_};
    }
    void set_value(const std::string& var_name, int, double value) override {
        require_initialized();
        if (var_name.find("water_discharge") != std::string::npos) {
            lateral_q_ = value;
        }
    }
    [[nodiscard]] double elapsed_time() const noexcept { return elapsed_; }

private:
    void require_initialized() const {
        if (!initialized_) throw std::logic_error("tracking D-Flow is not initialized");
    }
    bool initialized_{false};
    double elapsed_{0.0};
    double storage_{100.0};
    double lateral_q_{0.0};
};

sim::RuntimeConfig audit_config() {
    sim::RuntimeConfig config{};
    config.start_time = 0.0;
    config.end_time = 20.0;
    config.dt_couple = 1.0;
    config.dt_surface = 0.05;
    config.dt_swmm = 1.0;
    config.dt_dflowfm = 1.0;
    config.enable_swmm = true;
    config.enable_dflowfm = true;
    config.stcf_case_path = case_path_from_env();
    config.swmm_inp_path = "tracking.inp";
    config.dflowfm_mdu_path = "tracking.mdu";
    config.initial_eta = 1.0;
    config.enable_cvc_spatial_phi_t_correction = true;
    config.enable_whole_system_mass_audit = true;
    config.engine_mode = sim::EngineMode::mock;

    sim::SurfaceDrainageLinkConfig drainage{};
    drainage.cell = 0U;
    drainage.node_name = "11";
    drainage.crest_level = 0.95;
    drainage.exchange_width = 3.0;  // Q intent > Q_limit initially -> deficit
    config.surface_drainage.push_back(drainage);

    sim::SurfaceRiverLinkConfig river{};
    river.cell = 1U;
    river.location_id = 5;
    river.native_lateral_id = "lat1";
    river.crest_level = 0.95;
    river.exchange_width = 3.0;
    config.surface_river.push_back(river);
    return config;
}

}  // namespace

TEST(SimDriverWholeSystemMassAudit, ClosesEveryEpochAcrossThreePhysicalStores) {
    sim::SimDriver driver;
    driver.configure(audit_config());

    StorageTrackingSwmm swmm;
    StorageTrackingDFlow dflowfm;
    swmm.initialize("tracking.inp");
    dflowfm.initialize("tracking.mdu");

    sim::RunLoopHooks hooks{};
    hooks.swmm_elapsed_time = [&swmm]() { return swmm.elapsed_time(); };
    hooks.dflowfm_elapsed_time = [&dflowfm]() { return dflowfm.elapsed_time(); };
    hooks.swmm_storage_volume = [&swmm]() { return swmm.storage(); };
    // These authored tracking cases have no external 1D sources/sinks;
    // providing complete zero observations makes the audit verdict-eligible.
    hooks.swmm_external_net_volume = []() { return 0.0; };
    hooks.dflowfm_external_net_volume = []() { return 0.0; };

    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm, hooks);

    ASSERT_EQ(result.final_state, sim::SimDriverState::completed)
        << result.summary.reason << " residual="
        << result.summary.final_whole_system_mass_residual << " tolerance="
        << result.summary.whole_system_mass_tolerance;
    ASSERT_EQ(result.committed_epochs, 20U);
    ASSERT_EQ(result.summary.epochs.size(), 20U);
    EXPECT_TRUE(result.summary.whole_system_mass_audit_enabled);
    EXPECT_EQ(result.summary.whole_system_mass_verdict, "conserved");
    EXPECT_LE(result.summary.max_abs_whole_system_mass_residual, 1.0e-3);

    bool saw_nonzero_deficit = false;
    bool saw_age_above_writeoff_threshold = false;
    for (const sim::EpochRecord& epoch : result.summary.epochs) {
        EXPECT_TRUE(epoch.whole_system_mass_audit_enabled);
        EXPECT_EQ(epoch.whole_system_mass_verdict, "conserved");
        EXPECT_LE(std::abs(epoch.whole_system_mass_residual),
                  epoch.whole_system_mass_tolerance);
        ASSERT_FALSE(epoch.deficit_account_volumes.empty());
        double total_deficit = 0.0;
        for (const double volume : epoch.deficit_account_volumes) {
            total_deficit += volume;
        }
        if (total_deficit > 0.0) saw_nonzero_deficit = true;
        for (const std::size_t age : epoch.deficit_age_steps) {
            if (age >= 3U) saw_age_above_writeoff_threshold = true;
        }
    }
    EXPECT_TRUE(saw_nonzero_deficit);
    // M270 observes the N_writeoff_steps=3 trigger but intentionally does not
    // write anything off; M271 owns the sovereign ledger mutation.
    EXPECT_TRUE(saw_age_above_writeoff_threshold);

    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), 20.0);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 20.0);
}
