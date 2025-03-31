#pragma once

#include "triskel/graph/graph.hpp"
#include "triskel/graph/igraph.hpp"

namespace triskel {

struct SubGraph;

struct SubGraphEditor : public IGraphEditor {
    /// @brief A graph editor with source control
    explicit SubGraphEditor(SubGraph& g);

    /// @brief Adds a node from the graph to the subgraph
    void select_node(NodeId id);

    /// @brief Removes a node from the subgraph
    void unselect_node(NodeId id);

    /// @brief Make root
    void make_root(NodeId node);

    auto make_node() -> Node override;
    void remove_node(NodeId node) override;
    auto make_edge(NodeId from, NodeId to) -> Edge override;
    void edit_edge(EdgeId edge, NodeId new_from, NodeId new_to) override;
    void remove_edge(EdgeId edge) override;
    void push() override;
    void pop() override;
    void commit() override;

   private:
    SubGraph& sg_;
    IGraphEditor& editor_;

    /// @brief Add an edge to the subgraph
    void select_edge(const Edge& edge);

    /// @brief Add a node's edges to the subgraph
    void select_edges(NodeId node);

    /// @brief Remove a node's edges to the subgraph
    void unselect_edges(NodeId node);

    /// @brief Assert that an edge is in the subgraph
    void assert_present(EdgeId edge);

    /// @brief Assert that an edge is not the subgraph
    void assert_missing(EdgeId edge);
};

/// @brief A graph that contains only some nodes of another graph
struct SubGraph : public Graph {
    explicit SubGraph(Graph& g);

    /// @brief The root of this graph
    [[nodiscard]] auto root() const -> Node override;
    [[nodiscard]] auto nodes() const -> std::generator<Node> override;
    [[nodiscard]] auto edges() const -> std::generator<Edge> override;
    [[nodiscard]] auto get_node(NodeId id) const -> Node override;
    [[nodiscard]] auto get_edge(EdgeId id) const -> Edge override;
    [[nodiscard]] auto max_node_id() const -> size_t override;
    [[nodiscard]] auto max_edge_id() const -> size_t override;
    [[nodiscard]] auto node_count() const -> size_t override;
    [[nodiscard]] auto edge_count() const -> size_t override;
    [[nodiscard]] auto editor() -> SubGraphEditor& override;

    [[nodiscard]] auto contains(NodeId node) -> bool;
    [[nodiscard]] auto contains(EdgeId edge) -> bool;

   private:
    IGraph& g_;

    NodeId root_;
    NodeMap nodes_;
    EdgeMap edges_;

    SubGraphEditor editor_;

    friend struct SubGraphEditor;
};
}  // namespace triskel