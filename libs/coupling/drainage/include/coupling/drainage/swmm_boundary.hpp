#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>

#include "coupling/core/state.hpp"

namespace scau::coupling::drainage {

class SwmmEngineError : public std::runtime_error {
public:
    explicit SwmmEngineError(
        const std::string& message,
        std::string engine_id = "SWMM",
        std::string error_code = "swmm_engine_error")
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

class ISwmmEngine {
public:
    virtual ~ISwmmEngine() = default;

    virtual void initialize(const std::string& inp_path) = 0;
    virtual void step(double dt_swmm) = 0;
    virtual void finalize() = 0;

    [[nodiscard]] virtual double get_node_head(int node_id) const = 0;
    [[nodiscard]] virtual double get_node_lateral_inflow(int node_id) const = 0;
    virtual void set_node_lateral_inflow(int node_id, double q) = 0;

    [[nodiscard]] virtual double get_link_flow(int link_id) const = 0;
    [[nodiscard]] virtual bool is_surcharged(int node_id) const = 0;
};

class MockSwmmEngine final : public ISwmmEngine {
public:
    void initialize(const std::string& inp_path) override;
    void step(double dt_swmm) override;
    void finalize() override;

    [[nodiscard]] double get_node_head(int node_id) const override;
    [[nodiscard]] double get_node_lateral_inflow(int node_id) const override;
    void set_node_lateral_inflow(int node_id, double q) override;
    void set_node_head_fixture(int node_id, double head);

    [[nodiscard]] double get_link_flow(int link_id) const override;
    void set_link_flow_fixture(int link_id, double q);
    [[nodiscard]] bool is_surcharged(int node_id) const override;
    void set_node_surcharge_fixture(int node_id, bool surcharged);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] double elapsed_time() const noexcept;

private:
    bool initialized_{false};
    double elapsed_time_{0.0};
    std::unordered_map<int, double> node_heads_{};
    std::unordered_map<int, double> node_lateral_inflows_{};
    std::unordered_map<int, double> link_flows_{};
    std::unordered_map<int, bool> node_surcharge_flags_{};
};

[[nodiscard]] core::ExchangeRequest make_swmm_exchange_request(double q_request, double dt_sub);
[[nodiscard]] core::SharedExchangeIntent make_swmm_shared_exchange_intent(
    int node_id,
    double q_request,
    double priority_weight);
void accept_swmm_exchange_decision(ISwmmEngine& engine, int node_id, const core::ExchangeDecision& decision, double dt_sub);
void accept_swmm_shared_exchange_decision(
    ISwmmEngine& engine,
    const core::SharedExchangeDecision& decision,
    double dt_sub);
[[nodiscard]] core::EngineReport make_swmm_engine_report(const MockSwmmEngine& engine);
[[nodiscard]] core::EngineReport make_swmm_engine_report(const SwmmEngineError& error);

}  // namespace scau::coupling::drainage
