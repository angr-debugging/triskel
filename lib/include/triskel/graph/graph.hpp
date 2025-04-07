#pragma once

#include <cstddef>
#include <stack>
#include <variant>
#include "triskel/graph/frame.hpp"
#include "triskel/graph/igraph.hpp"
#include "triskel/graph/owning_graph.hpp"

namespace triskel {

struct Graph;

struct GraphEditor : public IGraphEditor {
    /// @brief A graph editor with source control
    explicit GraphEditor(Graph& g);
    explicit GraphEditor(const GraphEditor&) = delete;

    /// @brief Debug tests
    ~GraphEditor() override;

    auto operator=(const GraphEditor&) -> GraphEditor& = delete;

    auto make_node() -> Node* override;
    void remove_node(NodeId id) override;
    auto make_edge(NodeId from, NodeId to) -> Edge* override;
    void edit_edge(EdgeId id, NodeId new_from, NodeId new_to) override;
    void remove_edge(EdgeId id) override;
    void push() override;
    void pop() override;
    void commit() override;

   private:
    size_t next_node_id_ = 0;
    size_t next_edge_id_ = 0;

    auto frame() -> Frame&;

    auto make_edge(EdgeId id, NodeId from, NodeId to) -> Edge*;

    Graph& g_;
    std::stack<Frame> frames;

    friend struct Graph;
};

/// @brief A graph that owns its data
struct Graph : public OwningGraph {
    Graph();

    /// @brief The greatest id in this graph
    [[nodiscard]] auto max_node_id() const -> size_t override;

    /// @brief The greatest id in this graph
    [[nodiscard]] auto max_edge_id() const -> size_t override;

    /// @brief Gets the editor attached to this graph
    [[nodiscard]] auto editor() -> IGraphEditor& override;

   private:
    GraphEditor editor_;
};

}  // namespace triskel