/// @file common operations for analysis that provide a "family" hierarchy with
/// nodes having parents, childrens, ancestors and descendants
#pragma once

#include "triskel/utils/attribute.hpp"

namespace triskel {

struct Patriarchal {
    explicit Patriarchal(const IGraph& g);

    virtual ~Patriarchal() = default;

    /// @brief Gets this node's parent
    [[nodiscard]] auto parents(NodeId) -> std::generator<Node>;

    /// @brief Gets this node's only parent.
    /// If this node has multiple parents raises an error
    [[nodiscard]] auto parent(NodeId) -> Node;

    /// @brief Gets this node's only parent.
    /// If this node has multiple parents raises an error
    [[nodiscard]] auto parent_id(NodeId) const -> NodeId;

    /// @brief The number of children of a node
    [[nodiscard]] auto parent_count(NodeId) -> size_t;

    /// @brief Gets this node's children
    [[nodiscard]] auto children(NodeId) -> std::generator<Node>;

    /// @brief Gets this node's only child.
    /// If this node has multiple children raises an error
    [[nodiscard]] auto child(NodeId) -> Node;

    /// @brief The number of children of a node
    [[nodiscard]] auto children_count(NodeId) -> size_t;

    /// @brief Does node n1 precede n2 ?
    /// Checks using a node's children
    [[nodiscard]] auto precedes(NodeId n1, NodeId n2) -> bool;

    /// @brief Does node n1 succeed n2 ?
    /// Checks using a node's parents
    [[nodiscard]] auto succeed(NodeId n1, NodeId n2) -> bool;

   protected:
    /// @brief Makes a node a parent of another node
    void add_parent(NodeId parent, NodeId child);

   private:
    const IGraph& g_;
    NodeAttribute<std::vector<NodeId>> parents_;
    NodeAttribute<std::vector<NodeId>> children_;
};
}  // namespace triskel