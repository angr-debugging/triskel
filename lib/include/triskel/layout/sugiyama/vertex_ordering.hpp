#pragma once

#include <cstddef>
#include <random>
#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"

namespace triskel {
struct VertexOrdering {
    VertexOrdering(const IGraph& g,
                   const NodeAttribute<size_t>& layers,
                   size_t layer_count_);

    NodeAttribute<size_t> orders_;

   private:
    const IGraph& g_;

    const NodeAttribute<size_t>& layers_;

    std::vector<std::vector<const Node*>> node_layers_;

    NodeAttribute<std::vector<size_t>> child_orders_;
    NodeAttribute<std::vector<size_t>> parent_orders_;

    std::default_random_engine rng_;

    [[nodiscard]] auto count_crossings(const Node* node1,
                                       const Node* node2) const -> size_t;

    [[nodiscard]] auto count_crossings_with_layer(
        size_t l1,
        NodeAttribute<std::vector<size_t>>& neighbor_orders) -> size_t;

    // Save the vectors to avoid reallocating memory every time
    [[nodiscard]] auto count_crossings() -> size_t;

    /// @brief transform the order to the index in the layer
    void normalize_order();

    void median(size_t iter);
    void transpose();

    void swap(const Node* node, size_t i, size_t old_order, size_t new_order);
};
}  // namespace triskel