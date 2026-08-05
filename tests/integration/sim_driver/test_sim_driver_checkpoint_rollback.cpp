#include <cstdlib>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "coupling/drainage/swmm_boundary.hpp"
#include "coupling/river/dflowfm_boundary.hpp"
#include "run_loop.hpp"
#include "run_summary.hpp"
#include "sim_driver.hpp"

// M269 epoch commit protocol integration:
//   (a) a CFL rollback BEFORE engine advancement restores the run to the last
//       committed boundary bit-exactly (verified by re-running a fresh
//       simulation up to that boundary and comparing state hashes);
//   (b) an engine failure AT/AFTER advancement refuses rollback (SWMM cannot
//       rewind), records the decision evidence, and freezes the committed
//       counter;
//   (c) every committed epoch carries a committed checkpoint record (asserted
//       in test_sim_driver_run_loop).

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

// Rising-water scenario: no head-driven drain (crests far above the water),
// a constant manhole overflow keeps returning volume, so the wave speed grows
// every epoch until the CFL contract trips mid-run.
sim::RuntimeConfig rising_water_config(double end_time) {
    sim::RuntimeConfig config{};
    config.start_time = 0.0;
    config.end_time = end_time;
    config.dt_couple = 1.0;
    config.dt_surface = 0.05;
    config.dt_swmm = 1.0;
    config.dt_dflowfm = 1.0;
    config.enable_swmm = true;
    config.enable_dflowfm = true;
    config.stcf_case_path = case_path_from_env();
    config.swmm_inp_path = "mock.inp";
    config.dflowfm_mdu_path = "mock.mdu";
    config.initial_eta = 1.0;
    config.enable_cvc_spatial_phi_t_correction = true;
    config.c_rollback = 0.4;

    sim::SurfaceDrainageLinkConfig drainage{};
    drainage.cell = 0U;
    drainage.node_name = "11";
    drainage.crest_level = 100.0;  // never drains; the node only returns overflow
    drainage.exchange_width = 1.0;
    config.surface_drainage.push_back(drainage);

    sim::SurfaceRiverLinkConfig river{};
    river.cell = 1U;
    river.location_id = 5;
    river.native_lateral_id = "lat1";
    river.crest_level = 100.0;  // never drains
    river.exchange_width = 1.0;
    config.surface_river.push_back(river);
    return config;
}

void set_rising_water_fixtures(scau::coupling::drainage::MockSwmmEngine& swmm,
                               scau::coupling::river::MockDFlowFMEngine& dflowfm) {
    swmm.set_node_head_fixture(11, 0.0);
    swmm.set_node_overflow_fixture(11, 0.5);  // +0.5 m3 per epoch onto cell 0
    dflowfm.set_water_level_fixture(5, 0.2);
}

sim::RunLoopHooks mock_hooks(scau::coupling::drainage::MockSwmmEngine& swmm,
                             scau::coupling::river::MockDFlowFMEngine& dflowfm) {
    sim::RunLoopHooks hooks{};
    hooks.swmm_elapsed_time = [&swmm]() { return swmm.elapsed_time(); };
    hooks.dflowfm_elapsed_time = [&dflowfm]() { return dflowfm.elapsed_time(); };
    return hooks;
}

// Delegating SWMM engine that fails the post-step overflow read after a
// configured number of successful reads: a genuine at/after-engine-advancement
// failure inside the coupled substep.
class ThrowingOverflowSwmmEngine final : public scau::coupling::drainage::ISwmmEngine {
public:
    explicit ThrowingOverflowSwmmEngine(int allowed_reads) : allowed_reads_(allowed_reads) {}

    void initialize(const std::string& inp_path) override { inner_.initialize(inp_path); }
    void step(double dt_swmm) override { inner_.step(dt_swmm); }
    void finalize() override { inner_.finalize(); }
    [[nodiscard]] double get_node_head(int node_id) const override {
        return inner_.get_node_head(node_id);
    }
    [[nodiscard]] double get_node_lateral_inflow(int node_id) const override {
        return inner_.get_node_lateral_inflow(node_id);
    }
    void set_node_lateral_inflow(int node_id, double q) override {
        inner_.set_node_lateral_inflow(node_id, q);
    }
    [[nodiscard]] double get_node_inflow(int node_id) const override {
        return inner_.get_node_inflow(node_id);
    }
    [[nodiscard]] double get_node_overflow(int node_id) const override {
        if (overflow_reads_ >= allowed_reads_) {
            throw std::runtime_error("injected overflow read failure after engine step");
        }
        ++overflow_reads_;
        return inner_.get_node_overflow(node_id);
    }
    void set_outfall_stage(int node_id, double stage) override {
        inner_.set_outfall_stage(node_id, stage);
    }
    [[nodiscard]] double get_link_flow(int link_id) const override {
        return inner_.get_link_flow(link_id);
    }
    [[nodiscard]] bool is_surcharged(int node_id) const override {
        return inner_.is_surcharged(node_id);
    }

    [[nodiscard]] scau::coupling::drainage::MockSwmmEngine& inner() { return inner_; }

private:
    scau::coupling::drainage::MockSwmmEngine inner_{};
    int allowed_reads_{0};
    mutable int overflow_reads_{0};
};

}  // namespace

TEST(SimDriverCheckpointRollback, CflRollbackRestoresLastCommittedBoundaryBitExactly) {
    sim::SimDriver driver;
    driver.configure(rising_water_config(30.0));

    scau::coupling::drainage::MockSwmmEngine swmm;
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    set_rising_water_fixtures(swmm, dflowfm);

    const sim::RunLoopResult tripped =
        sim::run_simulation(driver, swmm, dflowfm, mock_hooks(swmm, dflowfm));

    ASSERT_EQ(tripped.final_state, sim::SimDriverState::review_required);
    EXPECT_EQ(tripped.summary.recovery_action, "restored_to_last_commit");
    EXPECT_EQ(tripped.summary.dflowfm_rollback_decision, "memory_only");
    EXPECT_NE(tripped.summary.reason.find("cfl_rollback"), std::string::npos);
    const std::size_t committed = tripped.committed_epochs;
    ASSERT_GE(committed, 1U);
    ASSERT_LT(committed, 30U);
    ASSERT_EQ(tripped.summary.epochs.size(), committed);
    // Engines never advanced into the failed epoch.
    EXPECT_DOUBLE_EQ(swmm.elapsed_time(), static_cast<double>(committed));
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), static_cast<double>(committed));

    // A fresh deterministic run that ENDS at the last committed boundary must
    // reproduce the restored state bit-exactly.
    sim::SimDriver replay_driver;
    replay_driver.configure(rising_water_config(static_cast<double>(committed)));
    scau::coupling::drainage::MockSwmmEngine replay_swmm;
    scau::coupling::river::MockDFlowFMEngine replay_dflowfm;
    replay_swmm.initialize("mock.inp");
    replay_dflowfm.initialize("mock.mdu");
    set_rising_water_fixtures(replay_swmm, replay_dflowfm);

    const sim::RunLoopResult replay = sim::run_simulation(
        replay_driver, replay_swmm, replay_dflowfm, mock_hooks(replay_swmm, replay_dflowfm));

    ASSERT_EQ(replay.final_state, sim::SimDriverState::completed);
    ASSERT_EQ(replay.committed_epochs, committed);
    EXPECT_EQ(replay.summary.final_surface_state_hash,
              tripped.summary.final_surface_state_hash);
    // The committed epoch records must agree hash-for-hash.
    for (std::size_t index = 0U; index < committed; ++index) {
        EXPECT_EQ(replay.summary.epochs[index].surface_content_hash,
                  tripped.summary.epochs[index].surface_content_hash);
        EXPECT_EQ(replay.summary.epochs[index].coupling_content_hash,
                  tripped.summary.epochs[index].coupling_content_hash);
    }
}

TEST(SimDriverCheckpointRollback, RefusesRollbackWhenEngineFailsAfterAdvancement) {
    sim::SimDriver driver;
    driver.configure(rising_water_config(10.0));

    // The very first post-step overflow read fails inside epoch 0's coupled
    // substep: both engines have already advanced when the failure surfaces.
    ThrowingOverflowSwmmEngine swmm(0);
    scau::coupling::river::MockDFlowFMEngine dflowfm;
    swmm.initialize("mock.inp");
    dflowfm.initialize("mock.mdu");
    set_rising_water_fixtures(swmm.inner(), dflowfm);

    sim::RunLoopHooks hooks{};
    hooks.swmm_elapsed_time = [&swmm]() { return swmm.inner().elapsed_time(); };
    hooks.dflowfm_elapsed_time = [&dflowfm]() { return dflowfm.elapsed_time(); };

    const sim::RunLoopResult result = sim::run_simulation(driver, swmm, dflowfm, hooks);

    EXPECT_EQ(result.final_state, sim::SimDriverState::review_required);
    EXPECT_EQ(result.committed_epochs, 0U);
    EXPECT_EQ(result.summary.epochs.size(), 0U);
    EXPECT_EQ(result.summary.recovery_action, "refused_engine_rollback");
    EXPECT_NE(result.summary.dflowfm_rollback_decision.find("abort"), std::string::npos);
    EXPECT_NE(result.summary.reason.find("rollback refused"), std::string::npos);
    EXPECT_NE(result.summary.reason.find("injected overflow read failure"),
              std::string::npos);
    // The engines DID advance into the refused epoch: that is why rollback
    // must be refused.
    EXPECT_DOUBLE_EQ(swmm.inner().elapsed_time(), 1.0);
    EXPECT_DOUBLE_EQ(dflowfm.elapsed_time(), 1.0);
}
