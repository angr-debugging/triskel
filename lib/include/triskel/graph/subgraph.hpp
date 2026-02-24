#pragma once

#include <stack>
#include "triskel/graph/frame.hpp"
#include "triskel/graph/igraph.hpp"
#include "triskel/graph/owning_graph.hpp"

namespace triskel {

struct SubGraph;

struct SubGraphEditor : public IGraphEditor {
    /// @brief A graph editor with source control
    explicit SubGraphEditor(SubGraph& g);

    explicit SubGraphEditor(const SubGraphEditor&)           = delete;
    auto operator=(const SubGraphEditor&) -> SubGraphEditor& = delete;

    /// @brief Adds a node from the graph to the subgraph
    void select_node(NodeId id, bool use_frame = false);

    /// @brief Removes a node from the subgraph
    void unselect_node(NodeId id, bool use_frame = false);

    /// @brief Make root
    void make_root(NodeId node);

    auto make_node() -> Node* override;
    void remove_node(NodeId node) override;
    auto make_edge(NodeId from, NodeId to) -> Edge* override;
    void edit_edge(EdgeId edge, NodeId new_from, NodeId new_to) override;
    void remove_edge(EdgeId edge) override;
    void push() override;
    void pop() override;
    void commit() override;

   private:
    SubGraph& sg_;
    IGraphEditor& editor_;

    auto frame() -> Frame&;
    std::stack<Frame> frames;

    /// @brief Add an edge to the subgraph
    void select_edge(const Edge* edge, bool use_frame);

    /// @brief Add a node's edges to the subgraph
    void select_edges(NodeId node, bool use_frame);

    /// @brief Remove a node's edges to the subgraph
    void unselect_edges(NodeId node, bool use_frame);
};

/// @brief A graph that contains only some nodes of another graph
// TODO: make an intersection of graph and subgraph that does not have the
// editor
struct SubGraph : public OwningGraph {
    explicit SubGraph(IGraph& g);

    /// @brief The root of this graph
    [[nodiscard]] auto editor() -> SubGraphEditor& override;

    /// @brief The greatest id in this graph
    [[nodiscard]] auto max_node_id() const -> size_t override;

    /// @brief The greatest id in this graph
    [[nodiscard]] auto max_edge_id() const -> size_t override;

   private:
    IGraph& g_;

    SubGraphEditor editor_;

    friend struct SubGraphEditor;
};
}  // namespace triskel