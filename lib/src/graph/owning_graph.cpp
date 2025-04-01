#include "triskel/graph/owning_graph.hpp"

#include <cassert>
#include <cstddef>
#include <generator>

#include "triskel/graph/igraph.hpp"

using triskel::OwningGraph;

auto OwningGraph::root() const -> Node {
    return get_node(data_.root);
}

auto OwningGraph::nodes() const -> std::generator<Node> {
    for (const auto& node : data_.nodes) {
        co_yield Node{*node.second};
    }
}

auto OwningGraph::edges() const -> std::generator<Edge> {
    for (const auto& edge : data_.edges) {
        co_yield Edge{*edge.second};
    }
}

auto OwningGraph::get_node(NodeId id) const -> Node {
    assert(id != NodeId::InvalidID);
    return {*data_.nodes.at(id)};
}

auto OwningGraph::get_edge(EdgeId id) const -> Edge {
    assert(id != EdgeId::InvalidID);
    return {*data_.edges.at(id)};
}

auto OwningGraph::node_count() const -> size_t {
    return data_.nodes.size();
}

auto OwningGraph::edge_count() const -> size_t {
    return data_.edges.size();
}

auto OwningGraph::contains(NodeId node) -> bool {
    return data_.nodes.contains(node);
}

auto OwningGraph::contains(EdgeId edge) -> bool {
    return data_.edges.contains(edge);
}
