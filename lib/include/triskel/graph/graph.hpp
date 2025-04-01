#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stack>
#include <variant>
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

    auto make_node() -> Node override;
    void remove_node(NodeId id) override;
    auto make_edge(NodeId from, NodeId to) -> Edge override;
    void edit_edge(EdgeId id, NodeId new_from, NodeId new_to) override;
    void remove_edge(EdgeId id) override;
    void push() override;
    void pop() override;
    void commit() override;

   private:
    size_t next_node_id_ = 0;
    size_t next_edge_id_ = 0;

    struct Frame {
        struct Modification {
            ~Modification()                          = default;
            virtual void revert(GraphEditor& editor) = 0;
        };

        struct MakeNode : public Modification {
            explicit MakeNode(NodeId id) : id{id} {}
            void revert(GraphEditor& editor) override;
            NodeId id;
        };

        struct RemoveNode : public Modification {
            explicit RemoveNode(std::unique_ptr<NodeData>&& node)
                : node{std::move(node)} {}
            void revert(GraphEditor& editor) override;
            std::unique_ptr<NodeData> node;
        };

        struct AddEdge : public Modification {
            explicit AddEdge(EdgeId id) : id{id} {}
            void revert(GraphEditor& editor) override;
            EdgeId id;
        };

        struct RemoveEdge : public Modification {
            explicit RemoveEdge(std::unique_ptr<EdgeData>&& edge)
                : edge{std::move(edge)} {}
            void revert(GraphEditor& editor) override;
            std::unique_ptr<EdgeData> edge;
        };

        struct ModifyEdge : public Modification {
            explicit ModifyEdge(const EdgeData& edge) : edge{edge} {}
            void revert(GraphEditor& editor) override;
            EdgeData edge;
        };

        auto operator=(const Frame&) -> Frame& = delete;

        std::stack<
            std::variant<MakeNode, RemoveNode, AddEdge, RemoveEdge, ModifyEdge>>
            changes;
    };

    auto frame() -> Frame&;

    auto make_edge(EdgeId id, NodeId from, NodeId to) -> EdgeData&;

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