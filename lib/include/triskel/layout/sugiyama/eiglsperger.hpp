#pragma once
#include <cstddef>
#include <deque>
#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"

namespace triskel {

struct LongEdge {
    const Node* p = nullptr;
    const Node* q = nullptr;

    // The dummy vertices of this long edge.
    // levels[5] contains the dummy node on layer 5
    std::vector<const Node*> level;
};

struct EVertexOrdering {
    EVertexOrdering(const IGraph& g,
                    const NodeAttribute<size_t>& layers,
                    const NodeAttribute<bool>& is_dummy,
                    size_t layer_count_);

    NodeAttribute<size_t> orders_;

   private:
    const IGraph& g_;

    const NodeAttribute<size_t>& layers_;
    const NodeAttribute<bool>& is_dummy_;

    std::deque<LongEdge> long_edges;
    NodeAttribute<LongEdge*> long_edges_{0, nullptr};
    std::vector<std::vector<const Node*>> node_layers_;

    [[nodiscard]] auto get_dummy_parent(const Node* node) const -> const Node*;
    [[nodiscard]] auto get_dummy_child(const Node* node) const -> const Node*;
    [[nodiscard]] auto get_segment(const Node* node) const -> const Node*;

    void make_node_layers();
};
}  // namespace triskel