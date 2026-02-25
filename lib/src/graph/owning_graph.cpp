#include "triskel/graph/owning_graph.hpp"

#include <cassert>
#include <cstddef>

#include "triskel/graph/igraph.hpp"

using triskel::OwningGraph;

auto OwningGraph::root() const -> Node* {
    return get_node(data_.root);
}

auto OwningGraph::nodes() const -> Container<Node*> {
    if (invalid_node_cache) {
        invalid_node_cache = false;

        cached_nodes_.clear();

        cached_nodes_.reserve(data_.nodes.size());
        for (const auto& kv : data_.nodes) {
            cached_nodes_.push_back(kv.second.get());
        }
    }
    return cached_nodes_;
}

auto OwningGraph::edges() const -> Container<Edge*> {
    if (invalid_edge_cache) {
        invalid_edge_cache = false;

        cached_edges_.clear();

        cached_edges_.reserve(data_.edges.size());
        for (const auto& kv : data_.edges) {
            cached_edges_.push_back(kv.second.get());
        }
    }
    return cached_edges_;
}

auto OwningGraph::get_node(NodeId id) const -> Node* {
    assert(id != NodeId::InvalidID);
    return data_.nodes.at(id).get();
}

auto OwningGraph::get_edge(EdgeId id) const -> Edge* {
    assert(id != EdgeId::InvalidID);
    return data_.edges.at(id).get();
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
