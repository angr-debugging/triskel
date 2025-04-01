#include <algorithm>
#include <set>
#include <triskel/graph/graph.hpp>
#include <vector>
#include "gtest/gtest.h"
#include "triskel/graph/igraph.hpp"
#include "triskel/utils/generator.hpp"

#include <gtest/gtest.h>

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

// The graph from the wikipedia example
// https://en.wikipedia.org/wiki/Depth-first_search#Output_of_a_depth-first_search
#define GRAPH1                        \
    auto g   = Graph{};               \
    auto& ge = g.editor();            \
    ge.push();                        \
                                      \
    auto n1 = ge.make_node();         \
    auto n2 = ge.make_node();         \
    auto n3 = ge.make_node();         \
    auto n4 = ge.make_node();         \
    auto n5 = ge.make_node();         \
    auto n6 = ge.make_node();         \
    auto n7 = ge.make_node();         \
    auto n8 = ge.make_node();         \
                                      \
    auto e1_2 = ge.make_edge(n1, n2); \
    auto e1_5 = ge.make_edge(n1, n5); \
    auto e1_8 = ge.make_edge(n1, n8); \
                                      \
    auto e2_3 = ge.make_edge(n2, n3); \
                                      \
    auto e3_4 = ge.make_edge(n3, n4); \
                                      \
    auto e4_2 = ge.make_edge(n4, n2); \
                                      \
    auto e5_6 = ge.make_edge(n5, n6); \
                                      \
    auto e6_3 = ge.make_edge(n6, n3); \
    auto e6_7 = ge.make_edge(n6, n7); \
    auto e6_8 = ge.make_edge(n6, n8); \
    ge.commit();

TEST(Graph, Smoke) {
    ASSERT_NO_THROW(GRAPH1);
}

TEST(Graph, Counts) {
    GRAPH1;

    ASSERT_EQ(g.node_count(), 8);
    ASSERT_EQ(g.edge_count(), 10);

    ASSERT_EQ(n1.parent_count(), 0);
    ASSERT_EQ(n1.parent_count(), 3);

    ASSERT_EQ(n2.children_count(), 1);
    ASSERT_EQ(n2.parent_count(), 2);

    ASSERT_EQ(n3.children_count(), 1);
    ASSERT_EQ(n3.parent_count(), 2);

    ASSERT_EQ(n4.children_count(), 1);
    ASSERT_EQ(n4.parent_count(), 1);

    ASSERT_EQ(n5.children_count(), 1);
    ASSERT_EQ(n5.parent_count(), 1);

    ASSERT_EQ(n6.children_count(), 3);
    ASSERT_EQ(n6.parent_count(), 1);

    ASSERT_EQ(n7.children_count(), 0);
    ASSERT_EQ(n7.parent_count(), 1);

    ASSERT_EQ(n8.children_count(), 0);
    ASSERT_EQ(n8.parent_count(), 2);

    for (const auto& n : g.nodes()) {
        ASSERT_EQ(n1.neighbor_count(), n.parent_count() + n.children_count());
    }
}

TEST(Graph, Children) {
    GRAPH1;

    auto sets_match = [](const std::vector<Node>& v1,
                         const std::vector<Node>& v2) {
        return std::set<Node>(v1.begin(), v1.end()) ==
               std::set<Node>(v2.begin(), v2.end());
    };

    {
        auto children = gen_to_v(n1.child_nodes());
        auto expected = std::vector<Node>{n2, n5, n8};
        ASSERT_PRED2(sets_match, children, expected);
    }
}

#undef GRAPH1
