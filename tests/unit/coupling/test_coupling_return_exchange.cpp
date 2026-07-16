#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

// 1D -> 2D return flow (engine_to_surface): manhole surcharge/overflow or
// river spill returning water onto the 2D surface through the core event
// queue, closing the bidirectional surface/engine loop.

namespace {

using scau::coupling::core::CouplingState;
using scau::coupling::core::ReturnExchangeRequest;
using scau::coupling::core::SharedExchangeEngine;

CouplingState make_state() {
    return CouplingState{{
        {.volume = 40.0, .mass_deficit_account = {.volume = 0.0},
         .phi_t = 0.4, .h = 2.0, .area = 50.0},
    }};
}

ReturnExchangeRequest make_request() {
    return ReturnExchangeRequest{
        .source = {.engine = SharedExchangeEngine::drainage, .node_id = 9U},
        .q_return = 0.5,
        .dt_sub = 4.0,
    };
}

}  // namespace

TEST(CouplingReturnExchange, ReplayAddsReturnedVolumeToCell) {
    auto state = make_state();

    const auto decision = state.apply_return_exchange(0U, make_request());

    EXPECT_DOUBLE_EQ(decision.q_returned, 0.5);
    EXPECT_DOUBLE_EQ(decision.v_returned, 2.0);
    EXPECT_EQ(decision.source.engine, SharedExchangeEngine::drainage);
    EXPECT_EQ(decision.source.node_id, 9U);

    // Like apply_exchange, the mutation is deferred to replay_pending().
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 40.0);

    state.replay_pending();
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 42.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].h, 2.0 + 2.0 / (0.4 * 50.0));
}

TEST(CouplingReturnExchange, SystemMassReflectsReturnedVolume) {
    auto state = make_state();
    const auto baseline = state.compute_system_mass(1.0e-6);

    static_cast<void>(state.apply_return_exchange(0U, make_request()));
    state.replay_pending();

    const auto current = state.compute_system_mass(1.0e-6);
    EXPECT_NEAR(current.surface_mass - baseline.surface_mass, 2.0, 1.0e-9);
}

TEST(CouplingReturnExchange, ZeroReturnIsANoop) {
    auto state = make_state();
    auto request = make_request();
    request.q_return = 0.0;

    const auto decision = state.apply_return_exchange(0U, request);
    state.replay_pending();

    EXPECT_DOUBLE_EQ(decision.q_returned, 0.0);
    EXPECT_DOUBLE_EQ(decision.v_returned, 0.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 40.0);
}

TEST(CouplingReturnExchange, RejectsInvalidInputs) {
    auto state = make_state();

    {
        auto request = make_request();
        request.q_return = -0.1;
        EXPECT_THROW(
            static_cast<void>(state.apply_return_exchange(0U, request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.q_return = std::nan("");
        EXPECT_THROW(
            static_cast<void>(state.apply_return_exchange(0U, request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.dt_sub = 0.0;
        EXPECT_THROW(
            static_cast<void>(state.apply_return_exchange(0U, request)),
            std::invalid_argument);
    }
    {
        auto request = make_request();
        request.dt_sub = std::nan("");
        EXPECT_THROW(
            static_cast<void>(state.apply_return_exchange(0U, request)),
            std::invalid_argument);
    }

    EXPECT_THROW(
        static_cast<void>(state.apply_return_exchange(1U, make_request())),
        std::out_of_range);
}
