#pragma once

#include <cstddef>
#include <vector>
#include "triskel/graph/igraph.hpp"

namespace triskel {
/// @brief A graph that owns a GraphData
struct OwningGraph : public IGraph {
    ~OwningGraph() override = default;

    /// @brief The root of this graph
    [[nodiscard]] auto root() const -> Node* override;

    /// @brief The nodes in this graph
    [[nodiscard]] auto nodes() const -> Container<Node*> override;

    /// @brief The edges in this graph
    [[nodiscard]] auto edges() const -> Container<Edge*> override;

    /// @brief Turns a NodeId into a Node
    [[nodiscard]] auto get_node(NodeId id) const -> Node* override;

    /// @brief Turns an EdgeId into an Edge
    [[nodiscard]] auto get_edge(EdgeId id) const -> Edge* override;

    /// @brief The number of nodes in this graph
    [[nodiscard]] auto node_count() const -> size_t override;

    /// @brief The number of edges in this graph
    [[nodiscard]] auto edge_count() const -> size_t override;

    /// @brief Does this subgraph have this node
    [[nodiscard]] auto contains(NodeId node) -> bool override;

    /// @brief Does this subgraph have this edge
    [[nodiscard]] auto contains(EdgeId edge) -> bool override;

   private:
    GraphData data_;

    mutable bool invalid_node_cache = true;
    mutable std::vector<Node*> cached_nodes_;

    mutable bool invalid_edge_cache = true;
    mutable std::vector<Edge*> cached_edges_;

    friend struct GraphEditor;
    friend struct SubGraphEditor;
    friend struct SubGraph;
    friend struct Frame;
};
}  // namespace triskel