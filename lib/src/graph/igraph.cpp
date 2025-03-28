#include "triskel/graph/igraph.hpp"

#include <cassert>
#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

// =============================================================================
// Iterators
// =============================================================================
inline auto EdgeExtractor::get(const Unit& /**/, const EdgeData& data) -> Edge {
    return {data};
}

inline auto ChildExtractor::get(const Unit& /**/,
                                const EdgeData& data) -> Node {
    return {*data.to};
}

inline auto ParentExtractor::get(const Unit& /**/,
                                 const EdgeData& data) -> Node {
    return {*data.from};
}

inline auto NeighborExtractor::get(const Node& self,
                                   const EdgeData& data) -> Node {
    return {self == data.to->id ? *data.from : *data.to};
}

// =============================================================================
// Nodes
// =============================================================================
Node::Node(const NodeData& n) : n_{&n} {}

auto Node::id() const -> NodeId {
    return n_->id;
}

auto Node::edges() const -> EdgeIterator {
    return {{}, n_->edges};
}

auto Node::child_edges() const -> EdgeIterator {
    return {{}, n_->edges, n_->separator};
}

auto Node::parent_edges() const -> EdgeIterator {
    return {{}, n_->edges, 0, n_->separator};
}

auto Node::neighbors() const -> NeighborNodeIterator {
    return {*this, n_->edges};
}

auto Node::child_nodes() const -> ChildNodeIterator {
    return {{}, n_->edges};
}

auto Node::parent_nodes() const -> ParentNodeIterator {
    return {{}, n_->edges};
}

// =============================================================================
// Edges
// =============================================================================
Edge::Edge(const EdgeData& e) : e_{&e} {}

auto Edge::id() const -> EdgeId {
    return e_->id;
}

auto Edge::from() const -> Node {
    return {*e_->from};
}

auto Edge::to() const -> Node {
    return {*e_->to};
}

auto Edge::other(NodeId n) const -> Node {
    if (n == to()) {
        return from();
    }

    return to();
}

// =============================================================================
// Formats
// =============================================================================
auto triskel::format_as(const Node& n) -> std::string {
    return fmt::format("n{}", n.id());
}

auto triskel::format_as(const Edge& e) -> std::string {
    return fmt::format("{} -> {}", e.from(), e.to());
}

auto triskel::format_as(const IGraph& g) -> std::string {
    auto s = std::string{"digraph G {\n"};

    for (auto node : g.nodes()) {
        s += fmt::format("{}\n", node);
    }

    s += "\n";

    for (auto edge : g.edges()) {
        s += fmt::format("{}\n", edge);
    }

    s += "}\n";

    return s;
}
