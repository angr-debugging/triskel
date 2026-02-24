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
// Edge
// =============================================================================
/// @brief Adds an edge to each of its nodes
void Edge::link() {
    // Child edges go in the back
    from->edges_.push_back(this);

    // Parent edges go in the front
    to->edges_.insert(to->edges_.begin(), this);
    to->separator_++;
}

/// @brief Removes an edge from each of its nodes
void Edge::unlink() {
    std::erase(from->edges_, this);
    std::erase(to->edges_, this);
    to->separator_--;
}

// =============================================================================
// Nodes
// =============================================================================
auto Node::id() const -> NodeId {
    return id_;
}

auto Node::neighbor_count() const -> size_t {
    return edges_.size();
}

auto Node::children_count() const -> size_t {
    return edges_.size() - separator_;
}

auto Node::parent_count() const -> size_t {
    return separator_;
}

auto Node::edges() const -> Container<Edge*> {
    return {edges_};
}

auto Node::child_edges() const -> Container<Edge*> {
    return Container<Edge*>(edges_).subspan(separator_);
}

auto Node::parent_edges() const -> Container<Edge*> {
    return Container<Edge*>(edges_).subspan(0, separator_);
}

// =============================================================================
// Edges
// =============================================================================

auto Edge::id() const -> EdgeId {
    return id_;
}

auto Edge::other(NodeId n) const -> Node* {
    if (n == *to) {
        return from;
    }

    return to;
}

// =============================================================================
// Formats
// =============================================================================
auto triskel::format_as(const Node& n) -> std::string {
    return fmt::format("n{}", n.id());
}

auto triskel::format_as(const Edge& e) -> std::string {
    return fmt::format("{} -> {}", *e.from, *e.to);
}

auto triskel::format_as(const IGraph& g) -> std::string {
    auto s = std::string{"digraph G {\n"};

    for (auto* node : g.nodes()) {
        s += fmt::format("{}\n", *node);
    }
    s += fmt::format("{} [shape=square]", *g.root());

    s += "\n";

    for (auto* edge : g.edges()) {
        s += fmt::format("{}\n", *edge);
    }

    s += "}\n";

    return s;
}
