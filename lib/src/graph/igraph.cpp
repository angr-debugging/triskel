#include "triskel/graph/igraph.hpp"

#include <cassert>
#include <cstddef>
#include <generator>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <fmt/format.h>

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

// =============================================================================
// EdgeData
// =============================================================================
/// @brief Adds an edge to each of its nodes
void EdgeData::link() {
    // Child edges go in the back
    from->edges.push_back(this);

    // Parent edges go in the front
    to->edges.insert(to->edges.begin(), this);
    to->separator++;
}

/// @brief Removes an edge from each of its nodes
void EdgeData::unlink() {
    std::erase(from->edges, this);
    std::erase(to->edges, this);
    to->separator--;
}

// =============================================================================
// Nodes
// =============================================================================
Node::Node(const NodeData& n) : n_{&n} {}

auto Node::id() const -> NodeId {
    return n_->id;
}

auto Node::neighbor_count() const -> size_t {
    return n_->edges.size();
}

auto Node::children_count() const -> size_t {
    return n_->edges.size() - n_->separator;
}

auto Node::parent_count() const -> size_t {
    return n_->separator;
}

auto Node::edges() const -> std::generator<Edge> {
    for (const auto* edge : n_->edges) {
        co_yield Edge{*edge};
    }
}

auto Node::child_edges() const -> std::generator<Edge> {
    for (size_t i = n_->separator; i < n_->edges.size(); ++i) {
        co_yield Edge{*n_->edges[i]};
    }
}

auto Node::parent_edges() const -> std::generator<Edge> {
    for (size_t i = 0; i < n_->separator; ++i) {
        co_yield Edge{*n_->edges[i]};
    }
}

auto Node::neighbors() const -> std::generator<Node> {
    for (const auto* edge : n_->edges) {
        if (edge->to == n_) {
            co_yield Node{*edge->from};
        } else {
            co_yield Node{*edge->to};
        }
    }
}

auto Node::child_nodes() const -> std::generator<Node> {
    for (size_t i = n_->separator; i < n_->edges.size(); ++i) {
        co_yield Node{*n_->edges[i]->to};
    }
}

auto Node::parent_nodes() const -> std::generator<Node> {
    for (size_t i = 0; i < n_->separator; ++i) {
        co_yield Node{*n_->edges[i]->from};
    }
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
