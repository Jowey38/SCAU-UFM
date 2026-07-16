#include <cmath>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

// 1D-1D engine-interface exchange (e.g. SWMM outfall discharging into a
// D-Flow FM lateral). The two 1D engines never see each other: the source
// reports a discharge request, the target reports an acceptance capacity, and
// the core issues the conservative decision both adapters then accept.

namespace {

using scau::coupling::core::EngineInterfaceExchangeRequest;
using scau::coupling::core::SharedExchangeEngine;
using scau::coupling::core::evaluate_engine_interface_exchange;

EngineInterfaceExchangeRequest make_request() {
    return EngineInterfaceExchangeRequest{
        .source = {.engine = SharedExchangeEngine::drainage, .node_id = 7U},
        .target = {.engine = SharedExchangeEngine::river, .node_id = 3U},
        .q_request = 0.4,
        .q_capacity = 1.0,
        .dt_sub = 5.0,
    };
}

}  // namespace

TEST(CouplingEngineInterfaceExchange, GrantsRequestWithinTargetCapacity) {
    const auto decision = evaluate_engine_interface_exchange(make_request());

    EXPECT_EQ(decision.source.engine, SharedExchangeEngine::drainage);
    EXPECT_EQ(decision.source.node_id, 7U);
    EXPECT_EQ(decision.target.engine, SharedExchangeEngine::river);
    EXPECT_EQ(decision.target.node_id, 3U);
    EXPECT_DOUBLE_EQ(decision.q_granted, 0.4);
    EXPECT_DOUBLE_EQ(decision.v_granted, 2.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 0.0);
}

TEST(CouplingEngineInterfaceExchange, ClampsRequestToTargetCapacity) {
    auto request = make_request();
    request.q_request = 2.0;
    request.q_capacity = 0.5;
    request.dt_sub = 4.0;

    const auto decision = evaluate_engine_interface_exchange(request);

    EXPECT_DOUBLE_EQ(decision.q_granted, 0.5);
    EXPECT_DOUBLE_EQ(decision.v_granted, 2.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 6.0);
}

TEST(CouplingEngineInterfaceExchange, ZeroRequestYieldsZeroDecision) {
    auto request = make_request();
    request.q_request = 0.0;

    const auto decision = evaluate_engine_interface_exchange(request);

    EXPECT_DOUBLE_EQ(decision.q_granted, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_granted, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_unmet, 0.0);
}

TEST(CouplingEngineInterfaceExchange, VolumeIsConservedAcrossTheInterface) {
    auto request = make_request();
    request.q_request = 1.7;
    request.q_capacity = 0.9;
    request.dt_sub = 3.0;

    const auto decision = evaluate_engine_interface_exchange(request);

    // Volume leaving the source equals volume entering the target; nothing
    // is created or lost at the interface.
    EXPECT_DOUBLE_EQ(decision.v_granted + decision.v_unmet, request.q_request * request.dt_sub);
    EXPECT_DOUBLE_EQ(decision.v_granted, decision.q_granted * request.dt_sub);
}

TEST(CouplingEngineInterfaceExchange, RejectsSameEngineEndpoints) {
    auto request = make_request();
    request.target.engine = SharedExchangeEngine::drainage;

    EXPECT_THROW(
        static_cast<void>(evaluate_engine_interface_exchange(request)),
        std::invalid_argument);
}

TEST(CouplingEngineInterfaceExchange, RejectsInvalidInputs) {
    {
        auto request = make_request();
        request.q_request = -0.1;
        EXPECT_THROW(
            static_cast<void>(evaluate_engine_interface_exchange(request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.q_request = std::nan("");
        EXPECT_THROW(
            static_cast<void>(evaluate_engine_interface_exchange(request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.q_capacity = -1.0;
        EXPECT_THROW(
            static_cast<void>(evaluate_engine_interface_exchange(request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.q_capacity = std::numeric_limits<double>::infinity();
        EXPECT_THROW(
            static_cast<void>(evaluate_engine_interface_exchange(request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.dt_sub = 0.0;
        EXPECT_THROW(
            static_cast<void>(evaluate_engine_interface_exchange(request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.dt_sub = std::nan("");
        EXPECT_THROW(
            static_cast<void>(evaluate_engine_interface_exchange(request)),
            std::invalid_argument);
    }
}
