#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/layout/sugiyama/layer_assignement.hpp"
#include "triskel/utils/attribute.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {

auto select_node(const IGraph& g,
                 std::vector<const Node*>& U,
                 std::vector<const Node*>& Z) -> const Node* {
    for (const auto* node : g.nodes()) {
        if (std::ranges::contains(U, node)) {
            continue;
        }

        auto valid = true;
        for (const auto* child_edge : node->child_edges()) {
            const auto& child = child_edge->to();
            if (!std::ranges::contains(Z, child)) {
                valid = false;
                break;
            }
        }

        if (valid) {
            return node;
        }
    }

    return nullptr;
}

auto layer_assignment(const IGraph& g, NodeAttribute<size_t>& layers)
    -> size_t {
    auto U = std::vector<const Node*>{};
    auto Z = std::vector<const Node*>{};

    size_t current_layer = 1;

    while (U.size() < g.node_count()) {
        const auto* v = select_node(g, U, Z);

        if (v != nullptr) {
            layers.set(*v, current_layer);
            U.push_back(v);
            continue;
        }

        current_layer++;
        Z.insert(Z.end(), U.begin(), U.end());
    }

    return current_layer + 1;
}
}  // namespace

auto triskel::longest_path_tamassia(const IGraph& graph)
    -> std::unique_ptr<LayerAssignment> {
    auto layers         = std::make_unique<LayerAssignment>(graph);
    layers->layer_count = layer_assignment(graph, layers->layers);

    return layers;
}
