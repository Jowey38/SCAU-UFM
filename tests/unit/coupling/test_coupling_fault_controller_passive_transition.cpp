#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {

scau::coupling::core::FaultControllerPassiveStateClassification make_classification(
    scau::coupling::core::FaultControllerProposedActionState state) {
    const scau::coupling::core::FaultControllerProposedAction action{.state = state};
    const auto observation = scau::coupling::core::observe_mock_scheduler_fault_action(action);
    const auto consumption = scau::coupling::core::consume_mock_scheduler_fault_action(observation);
    return scau::coupling::core::classify_fault_controller_passive_state(consumption);
}

}  // namespace

TEST(CouplingFaultControllerPassiveTransition, ContinueClassificationProducesNominalRunningTransition) {
    const auto classification = make_classification(
        scau::coupling::core::FaultControllerProposedActionState::continue_run);

    const auto transition = scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);

    EXPECT_EQ(
        transition.previous_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::running);
    EXPECT_EQ(
        transition.next_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::running);
    EXPECT_EQ(
        transition.reason,
        scau::coupling::core::FaultControllerPassiveTransitionReason::nominal_health);
    EXPECT_FALSE(transition.isolation_requested);
    EXPECT_FALSE(transition.reconnect_requested);
    EXPECT_FALSE(transition.abort_requested);
    EXPECT_FALSE(transition.runtime_action_executed);
    EXPECT_FALSE(transition.scheduler_control_enabled);
}

TEST(CouplingFaultControllerPassiveTransition, ReviewRequiredClassificationProducesReviewRequiredTransition) {
    const auto classification = make_classification(
        scau::coupling::core::FaultControllerProposedActionState::review_required);

    const auto transition = scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::running,
        classification);

    EXPECT_EQ(
        transition.previous_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::running);
    EXPECT_EQ(
        transition.next_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_EQ(
        transition.reason,
        scau::coupling::core::FaultControllerPassiveTransitionReason::fault_detected_review_required);
    EXPECT_TRUE(transition.classification.consumption.review_required_consumed);
    EXPECT_FALSE(transition.isolation_requested);
    EXPECT_FALSE(transition.reconnect_requested);
    EXPECT_FALSE(transition.abort_requested);
    EXPECT_FALSE(transition.runtime_action_executed);
    EXPECT_FALSE(transition.scheduler_control_enabled);
}

TEST(CouplingFaultControllerPassiveTransition, TransitionDoesNotPromoteClassificationControlFlags) {
    const scau::coupling::core::FaultControllerPassiveStateClassification classification{
        .state = scau::coupling::core::FaultControllerPassiveStateLabel::review_required,
        .scheduler_control_enabled = true,
        .isolation_enabled = true,
        .reconnect_enabled = true,
        .abort_enabled = true,
    };

    const auto transition = scau::coupling::core::make_fault_controller_passive_transition(
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required,
        classification);

    EXPECT_EQ(
        transition.previous_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_EQ(
        transition.next_state,
        scau::coupling::core::FaultControllerPassiveStateLabel::review_required);
    EXPECT_TRUE(transition.classification.scheduler_control_enabled);
    EXPECT_TRUE(transition.classification.isolation_enabled);
    EXPECT_TRUE(transition.classification.reconnect_enabled);
    EXPECT_TRUE(transition.classification.abort_enabled);
    EXPECT_FALSE(transition.isolation_requested);
    EXPECT_FALSE(transition.reconnect_requested);
    EXPECT_FALSE(transition.abort_requested);
    EXPECT_FALSE(transition.runtime_action_executed);
    EXPECT_FALSE(transition.scheduler_control_enabled);
}
