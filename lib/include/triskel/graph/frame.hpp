#pragma once

#include <stack>
#include "triskel/graph/igraph.hpp"
#include "triskel/graph/owning_graph.hpp"
#include <variant>

namespace triskel {
struct Frame {
    struct Modification {
        ~Modification()                         = default;
        virtual void revert(OwningGraph& graph) = 0;
    };

    struct MakeNode : public Modification {
        explicit MakeNode(NodeId id) : id{id} {}
        void revert(OwningGraph& graph) override;
        NodeId id;
    };

    struct RemoveNode : public Modification {
        explicit RemoveNode(std::unique_ptr<Node>&& node)
            : node{std::move(node)} {}
        void revert(OwningGraph& graph) override;
        std::unique_ptr<Node> node;
    };

    struct AddEdge : public Modification {
        explicit AddEdge(EdgeId id) : id{id} {}
        void revert(OwningGraph& graph) override;
        EdgeId id;
    };

    struct RemoveEdge : public Modification {
        explicit RemoveEdge(std::unique_ptr<Edge>&& edge)
            : edge{std::move(edge)} {}
        void revert(OwningGraph& graph) override;
        std::unique_ptr<Edge> edge;
    };

    struct ModifyEdge : public Modification {
        explicit ModifyEdge(EdgeId id, Node* from, Node* to)
            : id{id}, from{from}, to{to} {}
        void revert(OwningGraph& graph) override;
        EdgeId id;
        Node* from;
        Node* to;
    };

    auto operator=(const Frame&) -> Frame& = delete;

    std::stack<
        std::variant<MakeNode, RemoveNode, AddEdge, RemoveEdge, ModifyEdge>>
        changes;
};
};  // namespace triskel