#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "coupling/core/state.hpp"

namespace scau::coupling::river {

class DFlowFMEngineError : public std::runtime_error {
public:
    explicit DFlowFMEngineError(
        const std::string& message,
        std::string engine_id = "DFlowFM",
        std::string error_code = "dflowfm_engine_error")
        : std::runtime_error(message),
          engine_id_(std::move(engine_id)),
          error_code_(std::move(error_code)) {}

    [[nodiscard]] const std::string& engine_id() const noexcept {
        return engine_id_;
    }

    [[nodiscard]] const std::string& error_code() const noexcept {
        return error_code_;
    }

private:
    std::string engine_id_;
    std::string error_code_;
};

class IDFlowFMEngine {
public:
    virtual ~IDFlowFMEngine() = default;

    virtual void initialize(const std::string& mdu_path) = 0;
    virtual void update(double dt_dfm) = 0;
    virtual void finalize() = 0;

    [[nodiscard]] virtual double get_value(const std::string& var_name, int location_id) const = 0;
    virtual void set_value(const std::string& var_name, int location_id, double value) = 0;
};

struct RiverExchangePoint {
    int branch_id{0};
    int link_id{0};
    std::string exchange_type{};
    double crest_level{0.0};
    double exchange_width{0.0};
    double priority_weight{1.0};
    bool rtc_controlled{false};
    std::vector<int> neighbor_2d_edges{};
};

[[nodiscard]] bool is_valid_river_exchange_point(const RiverExchangePoint& point);

class MockDFlowFMEngine final : public IDFlowFMEngine {
public:
    void initialize(const std::string& mdu_path) override;
    void update(double dt_dfm) override;
    void finalize() override;

    [[nodiscard]] double get_value(const std::string& var_name, int location_id) const override;
    void set_value(const std::string& var_name, int location_id, double value) override;
    void set_water_level_fixture(int location_id, double water_level);
    void set_discharge_fixture(int location_id, double discharge);
    void set_lateral_discharge_fixture(int location_id, double lateral_discharge);
    void set_gate_opening_fixture(int location_id, double gate_opening);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] double elapsed_time() const noexcept;

private:
    struct ValueKey {
        std::string var_name{};
        int location_id{0};

        [[nodiscard]] bool operator==(const ValueKey& other) const noexcept {
            return var_name == other.var_name && location_id == other.location_id;
        }
    };

    struct ValueKeyHash {
        [[nodiscard]] std::size_t operator()(const ValueKey& key) const noexcept;
    };

    bool initialized_{false};
    double elapsed_time_{0.0};
    std::unordered_map<ValueKey, double, ValueKeyHash> values_{};
};

[[nodiscard]] core::ExchangeRequest make_dflowfm_exchange_request(double q_request, double dt_sub);
[[nodiscard]] core::SharedExchangeIntent make_dflowfm_shared_exchange_intent(
    int location_id,
    double q_request,
    double priority_weight);
void accept_dflowfm_exchange_decision(
    IDFlowFMEngine& engine,
    const std::string& var_name,
    int location_id,
    const core::ExchangeDecision& decision,
    double dt_sub);
void accept_dflowfm_lateral_discharge_exchange_decision(
    IDFlowFMEngine& engine,
    int location_id,
    const core::ExchangeDecision& decision,
    double dt_sub);
void accept_dflowfm_shared_exchange_decision(
    IDFlowFMEngine& engine,
    const core::SharedExchangeDecision& decision,
    double dt_sub);
[[nodiscard]] core::EngineReport make_dflowfm_engine_report(const MockDFlowFMEngine& engine);
[[nodiscard]] core::EngineReport make_dflowfm_engine_report(const DFlowFMEngineError& error);

}  // namespace scau::coupling::river
