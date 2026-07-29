#include <gtest/gtest.h>

#include <limits>

#include "core/error.hpp"
#include "stcf/topology.hpp"
#include "stcf_case_fixture.hpp"

namespace {

using scau::core::ScauError;
using scau::stcf::face_node_count;
using scau::stcf::kConnectivityFillValue;
using scau::stcf::test::make_mixed_case;
using scau::stcf::validate_stcf_case;

TEST(StcfTopology, AcceptsMixedTriangleQuadrilateralTopology) {
    const auto stcf_case = make_mixed_case();
    EXPECT_NO_THROW(validate_stcf_case(stcf_case));
    EXPECT_EQ(face_node_count(stcf_case.topology.face_nodes[0]), 4U);
    EXPECT_EQ(face_node_count(stcf_case.topology.face_nodes[1]), 3U);
}

TEST(StcfTopology, RejectsFieldTopologyCountMismatch) {
    auto stcf_case = make_mixed_case();
    stcf_case.fields.edges.phi_e_n.pop_back();
    EXPECT_THROW(validate_stcf_case(stcf_case), ScauError);
}

TEST(StcfTopology, RejectsNonFiniteNodeCoordinate) {
    auto stcf_case = make_mixed_case();
    stcf_case.topology.node_y[2] = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(validate_stcf_case(stcf_case), ScauError);
}

TEST(StcfTopology, RejectsNonContiguousOrWrongFaceNodeCount) {
    auto non_contiguous = make_mixed_case();
    non_contiguous.topology.face_nodes[1] = {1, kConnectivityFillValue, 2, 4};
    EXPECT_THROW(validate_stcf_case(non_contiguous), ScauError);

    auto too_short = make_mixed_case();
    too_short.topology.face_nodes[1] = {1, 4, kConnectivityFillValue, kConnectivityFillValue};
    EXPECT_THROW(validate_stcf_case(too_short), ScauError);
}

TEST(StcfTopology, RejectsDuplicateOrOutOfRangeFaceNode) {
    auto duplicate = make_mixed_case();
    duplicate.topology.face_nodes[1] = {1, 4, 1, kConnectivityFillValue};
    EXPECT_THROW(validate_stcf_case(duplicate), ScauError);

    auto out_of_range = make_mixed_case();
    out_of_range.topology.face_nodes[1] = {1, 99, 2, kConnectivityFillValue};
    EXPECT_THROW(validate_stcf_case(out_of_range), ScauError);
}

TEST(StcfTopology, RejectsInvalidEdgeEndpointsAndFaces) {
    auto duplicate_endpoint = make_mixed_case();
    duplicate_endpoint.topology.edge_nodes[0] = {0, 0};
    EXPECT_THROW(validate_stcf_case(duplicate_endpoint), ScauError);

    auto invalid_face = make_mixed_case();
    invalid_face.topology.edge_faces[0] = {99, kConnectivityFillValue};
    EXPECT_THROW(validate_stcf_case(invalid_face), ScauError);

    auto duplicate_face = make_mixed_case();
    duplicate_face.topology.edge_faces[1] = {0, 0};
    EXPECT_THROW(validate_stcf_case(duplicate_face), ScauError);

    auto duplicate_edge = make_mixed_case();
    duplicate_edge.topology.edge_nodes[5] = {1, 4};
    EXPECT_THROW(validate_stcf_case(duplicate_edge), ScauError);
}

TEST(StcfTopology, RejectsFaceEdgeCrossConsistencyFailures) {
    auto duplicate_edge = make_mixed_case();
    duplicate_edge.topology.face_edges[0] = {0, 1, 1, 3};
    EXPECT_THROW(validate_stcf_case(duplicate_edge), ScauError);

    auto wrong_endpoints = make_mixed_case();
    wrong_endpoints.topology.face_edges[1] = {5, 4, 1, kConnectivityFillValue};
    EXPECT_THROW(validate_stcf_case(wrong_endpoints), ScauError);

    auto missing_back_reference = make_mixed_case();
    missing_back_reference.topology.edge_faces[4] = {0, kConnectivityFillValue};
    EXPECT_THROW(validate_stcf_case(missing_back_reference), ScauError);

    auto missing_face_reference = make_mixed_case();
    missing_face_reference.topology.face_edges[1] = {4, 5, 1, 0};
    EXPECT_THROW(validate_stcf_case(missing_face_reference), ScauError);
}

}  // namespace
