// Based on Fast and Simple Horizontal Coordinate Assignment Ulrik Brandes and
// Boris Kopf https://link.springer.com/content/pdf/10.1007/3-540-45848-4_3
#include "triskel/layout/sugiyama/x_coordinate_assignment.hpp"

#include <fmt/base.h>
#include <fmt/format.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"
#include "triskel/utils/generator.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {

struct FSHCA {
    FSHCA(const IGraph& g,
          const std::vector<std::vector<const Node*>>& layers,
          const NodeAttribute<size_t>& layer,
          const NodeAttribute<size_t>& order,
          const NodeAttribute<float>& widths,
          const NodeAttribute<bool>& is_top_bottom,
          const EdgeAttribute<bool>& is_dummy,
          const EdgeAttribute<float>& start_x_offset,
          const EdgeAttribute<float>& end_x_offset)
        : xs_(g, 0.0F),

          g_(g),
          layers_{layers},
          layer_{layer},
          order_{order},
          widths_(widths),
          is_top_bottom_{is_top_bottom},
          is_dummy_{is_dummy},
          start_x_offset_(start_x_offset),
          end_x_offset_(end_x_offset),

          // Used in preprocessing
          marked_(g, false),

          // Used in align
          aligns_(g, nullptr),
          roots_(g, nullptr),

          // Used in balancing
          sinks_(g, nullptr),
          shifts_(g, std::numeric_limits<int64_t>::max()),
          was_placed_(g, false),
          drift_(g, 0)

    {
        for (const auto* node : g_.nodes()) {
            aligns_[node] = node;
            roots_[node]  = node;
        }

        preprocessing();

        align();

        compact();
    }

    int64_t delta = 50;
    NodeAttribute<float> xs_;

   private:
    const IGraph& g_;

    const std::vector<std::vector<const Node*>>& layers_;

    /// @brief The layer a node belongs to
    const NodeAttribute<size_t>& layer_;

    /// @brief The index of a node in its layer
    /// layers_[layer_[node]][orders_[node]] = node
    const NodeAttribute<size_t>& order_;

    /// @brief One of the ends of this edge is a dummy edge
    const EdgeAttribute<bool>& is_dummy_;

    /// @brief The end of an edge
    const EdgeAttribute<float>& start_x_offset_;

    /// @brief The start of an edge
    const EdgeAttribute<float>& end_x_offset_;

    EdgeAttribute<bool> marked_;

    /// @brief Compensates for edges not being centered on nodes
    NodeAttribute<float> drift_;

    NodeAttribute<const Node*> aligns_;
    NodeAttribute<const Node*> roots_;

    NodeAttribute<Node*> sinks_;
    NodeAttribute<int64_t> shifts_;
    NodeAttribute<float> widths_;
    NodeAttribute<bool> is_top_bottom_;
    NodeAttribute<bool> was_placed_;

    [[nodiscard]] auto get_layer_size(size_t i) const -> size_t {
        return layers_[i].size();
    }

    /// @brief The order of a parent inner node if it exists
    auto is_incident(const Node* node) -> std::optional<size_t> {
        auto res = std::optional<size_t>();
        for (const auto* edge : node->parent_edges()) {
            if (is_dummy_[edge]) {
                if (!res.has_value()) {
                    res = order_[edge->from];
                } else {
                    res = std::max(order_[edge->from], *res);
                }
            }
        }
        return res;
    }

    auto preprocessing() -> void {
        for (size_t layer_idx = 1; layer_idx < layers_.size(); ++layer_idx) {
            size_t k0 = 0;
            size_t l  = 0;

            const auto& layer = layers_[layer_idx];

            for (size_t l1 = 0; l1 < get_layer_size(layer_idx); ++l1) {
                const auto* node = layer[l1];

                auto incident = is_incident(node);

                if (l1 == get_layer_size(layer_idx) - 1 ||
                    incident.has_value()) {
                    size_t k1 = get_layer_size(layer_idx - 1);

                    if (incident.has_value()) {
                        k1 = incident.value();
                    }

                    /// @brief The leftmost node can't intersect on the left
                    bool can_intersect_left = false;

                    for (; l <= l1; ++l) {
                        const auto* node = layer[l];

                        for (const auto* edge : node->parent_edges()) {
                            const auto* vk = edge->from;

                            const auto k = order_[vk];

                            if ((can_intersect_left && k < k0) ||
                                (k > k1 && l != l1)) {
                                marked_[edge] = true;
                            }
                        }

                        can_intersect_left = true;
                    }

                    k0 = k1;
                    l  = l1;
                }
            }
        }
    }

    auto align() -> void {
        for (const auto& layer : std::ranges::views::reverse(layers_)) {
            size_t r = -1;

            for (const auto* node : layer) {
                auto neighbors = span_to_vec(node->parent_edges());

                neighbors = std::ranges::views::filter(
                                neighbors,
                                [this](const Edge* n) {
                                    return !is_top_bottom_[n->from];
                                }) |
                            std::ranges::to<std::vector<Edge*>>();

                std::ranges::sort(neighbors,
                                  [this](const Edge* a, const Edge* b) -> bool {
                                      return order_[a->from] < order_[b->from];
                                  });

                if (neighbors.empty()) {
                    continue;
                }

                std::vector<size_t> measures;
                const auto d = neighbors.size();
                if (d % 2 == 0) {
                    measures = std::vector<size_t>{(d / 2) - 1, d / 2};
                } else {
                    measures = std::vector<size_t>{(d - 1) / 2};
                }

                for (const auto m : measures) {
                    if (aligns_[node] != node) {
                        continue;
                    }

                    const auto* edge = neighbors[m];
                    const auto* um   = edge->from;

                    if (!marked_[edge] && (r < order_[um] || r == -1)) {
                        fmt::print("Aligning {} with {}\n", *node, *um);
                        aligns_[um]   = node;
                        roots_[node]  = roots_[um];
                        aligns_[node] = roots_[node];
                        r             = order_[um];
                        drift_[node]  = drift_[um] + start_x_offset_[edge] -
                                       end_x_offset_[edge];
                    }
                }
            }
        }
    };

    /// @brief returns the x coordinate of a node with drift taken into account
    auto drifted_x(const Node* node) { return xs_[node] + drift_[node]; }

    auto set_x(const Node* node, int64_t x) -> void {
        was_placed_[node] = true;
        xs_[node]         = x;
    }

    /// @brief Finds the first predecessor of a node that isn't a top down node
    [[nodiscard]] auto pred(const Node* node) const -> const Node* {
        const auto& layer = layers_[layer_[node]];
        auto order        = order_[node];

        while (order > 0) {
            order -= 1;

            const auto* node = layer[order];
            if (!is_top_bottom_[node]) {
                return node;
            }
        }

        return nullptr;
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    auto place_block(const Node* node) -> void {
        if (was_placed_[node]) {
            return;
        }

        set_x(node, 0);
        const auto* w = node;

        do {
            const auto* p = pred(w);
            if (p != nullptr) {
                const auto* u = roots_[p];
                place_block(u);

                if (sinks_[node] == node) {
                    sinks_[node] = sinks_[u];
                }

                if (sinks_[node] != sinks_[u]) {
                    // TODO: add block width somewhere here
                    shifts_[sinks_[u]] = std::min(
                        shifts_[sinks_[u]],
                        static_cast<int64_t>(xs_[node] + drift_[w] - xs_[u] -
                                             delta - drift_[p] - widths_[p]));
                } else {
                    xs_[node] = std::max(xs_[node], xs_[u] + delta + drift_[p] +
                                                        widths_[p] - drift_[w]);
                }
            }

            w = aligns_[w];
        } while (w != node);
    }

    auto compact() -> void {
        for (const auto& node : g_.nodes()) {
            sinks_[node] = node;
        }

        for (const auto& node : g_.nodes()) {
            if (roots_[node] == node) {
                place_block(node);
            }
        }

        for (const auto& node : g_.nodes()) {
            xs_[node]        = xs_[roots_[node]] + drift_[node];
            const auto shift = shifts_[sinks_[roots_[node]]];
            if (shift < std::numeric_limits<int64_t>::max()) {
                xs_[node] += shifts_[sinks_[roots_[node]]];
            }
        }
    }
};

}  // namespace

auto triskel::make_x_coords(const IGraph& g,
                            const std::vector<std::vector<const Node*>>& layers,
                            const NodeAttribute<size_t>& layer,
                            const NodeAttribute<size_t>& order,
                            const NodeAttribute<float>& widths,
                            const NodeAttribute<bool>& is_top_bottom,
                            const EdgeAttribute<bool>& is_dummy,
                            const EdgeAttribute<float>& start_x_offset,
                            const EdgeAttribute<float>& end_x_offset)
    -> NodeAttribute<float> {
    auto ctx = FSHCA(g, layers, layer, order, widths, is_top_bottom, is_dummy,
                     start_x_offset, end_x_offset);

    return ctx.xs_;
}