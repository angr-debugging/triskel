// Based on Fast and Simple Horizontal Coordinate Assignment Ulrik Brandes and
// Boris Kopf https://link.springer.com/content/pdf/10.1007/3-540-45848-4_3
#include "triskel/layout/sugiyama/x_coordinate_assignment.hpp"

#include <fmt/base.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstddef>
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

enum class LR : uint8_t { LEFT, RIGHT };
enum class TD : uint8_t { TOP, DOWN };

template <const TD vdir, const LR hdir>
struct FSHCA {
    FSHCA(const IGraph& g,
          const std::vector<std::vector<const Node*>>& layers,
          const NodeAttribute<size_t>& layer,
          const NodeAttribute<size_t>& order,
          const NodeAttribute<float>& widths,
          const NodeAttribute<bool>& is_top_bottom,
          const NodeAttribute<bool>& is_dummy,
          const EdgeAttribute<float>& start_x_offset,
          const EdgeAttribute<float>& end_x_offset,
          float delta)
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
          shifts_(g,
                  hdir == LR::LEFT ? std::numeric_limits<int64_t>::max()
                                   : std::numeric_limits<int64_t>::min()),
          was_placed_(g, false),
          drift_(g, 0),

          delta{delta}

    {
        for (const auto* node : g_.nodes()) {
            aligns_[node] = node;
            roots_[node]  = node;
        }

        preprocessing();

        align();

        compact();
    }

    NodeAttribute<float> xs_;

   private:
    const IGraph& g_;

    const std::vector<std::vector<const Node*>>& layers_;

    /// @brief The layer a node belongs to
    const NodeAttribute<size_t>& layer_;

    /// @brief The index of a node in its layer
    /// layers_[layer_[node]][orders_[node]] = node
    const NodeAttribute<size_t>& order_;

    /// @brief Is the node part of a split long edge
    const NodeAttribute<bool>& is_dummy_;

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

    // X gutter
    float delta;

    [[nodiscard]] auto get_layer_size(size_t i) const -> size_t {
        return layers_[i].size();
    }

    /// @brief The order of a parent inner node if it exists

    auto is_incident(const Node* node) -> std::optional<size_t> {
        auto res = std::optional<size_t>();

        const auto& neighbors =
            vdir == TD::TOP ? node->parent_edges() : node->child_edges();

        for (const auto* edge : neighbors) {
            if (is_dummy_[edge->to] || is_dummy_[edge->to]) {
                const auto* neighbor = vdir == TD::TOP ? edge->from : edge->to;

                if (is_top_bottom_[neighbor] && !is_dummy_[node] ||
                    is_top_bottom_[node] && !is_dummy_[neighbor]) {
                    continue;
                }

                const auto order = order_[neighbor];
                if (!res.has_value()) {
                    res = order;
                } else {
                    res = hdir == LR::LEFT ? std::max(order, *res)
                                           : std::min(order, *res);
                }
            }
        }
        return res;
    }

    auto preprocessing() -> void {
        for (size_t layer_idx = (vdir == TD::TOP) ? layers_.size() - 2 : 1;
             layer_idx < layers_.size();  // Uses overlap for DOWN
             vdir == TD::TOP ? --layer_idx : ++layer_idx) {
            const auto& layer = layers_[layer_idx];

            const auto last_idx =
                hdir == LR::LEFT ? get_layer_size(layer_idx) - 1 : 0;

            size_t k0 =
                hdir == LR::LEFT
                    ? 0
                    : get_layer_size(layer_idx + (vdir == TD::TOP ? 1 : -1));
            size_t l = hdir == LR::LEFT ? 0 : get_layer_size(layer_idx) - 1;

            for (size_t l1 = (hdir == LR::LEFT) ? 0
                                                : get_layer_size(layer_idx) - 1;
                 l1 < get_layer_size(layer_idx);  // Uses overlap for RIGHT
                 hdir == LR::LEFT ? ++l1 : --l1) {
                const auto* node = layer[l1];

                auto incident = is_incident(node);

                if (l1 == last_idx || incident.has_value()) {
                    size_t k1 = hdir == LR::LEFT
                                    ? get_layer_size(layer_idx +
                                                     (vdir == TD::TOP ? 1 : -1))
                                    : 0;

                    if (incident.has_value()) {
                        k1 = incident.value();
                    }

                    /// @brief The leftmost node can't intersect on the left
                    bool can_intersect_left = false;

                    for (; hdir == LR::LEFT
                               ? l <= l1
                               : l >= l1 && l < get_layer_size(layer_idx);
                         hdir == LR::LEFT ? ++l : --l) {
                        const auto* node = layer[l];

                        const auto& edges = vdir == TD::TOP
                                                ? node->parent_edges()
                                                : node->child_edges();
                        for (const auto* edge : edges) {
                            const auto* vk =
                                vdir == TD::TOP ? edge->from : edge->to;

                            const auto k = order_[vk];

                            if ((can_intersect_left &&
                                 (hdir == LR::LEFT ? k < k0 : k > k0)) ||
                                ((hdir == LR::LEFT ? k > k1 : k < k1) &&
                                 l != l1)) {
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
        const auto& layers = vdir == TD::TOP
                                 ? std::vector(layers_.rbegin(), layers_.rend())
                                 : layers_;

        for (const auto& layer : layers) {
            size_t r = -1;

            const auto& nodes = hdir == LR::LEFT
                                    ? layer
                                    : std::vector(layer.rbegin(), layer.rend());
            for (const auto* node : nodes) {
                auto neighbors =
                    span_to_vec(vdir == TD::TOP ? node->parent_edges()
                                                : node->child_edges());

                if (!is_dummy_[node]) {
                    neighbors = std::ranges::views::filter(
                                    neighbors,
                                    [this](const Edge* n) {
                                        return !is_top_bottom_[n->from];
                                    }) |
                                std::ranges::to<std::vector<Edge*>>();
                }

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
                    if (hdir == LR::LEFT) {
                        measures = std::vector<size_t>{(d / 2) - 1, d / 2};
                    } else {
                        measures = std::vector<size_t>{d / 2, (d / 2) - 1};
                    }
                } else {
                    measures = std::vector<size_t>{(d - 1) / 2};
                }

                for (const auto m : measures) {
                    if (aligns_[node] != node) {
                        continue;
                    }

                    const auto* edge = neighbors[m];
                    const auto* um   = vdir == TD::TOP ? edge->from : edge->to;

                    if ((is_top_bottom_[node] && !is_dummy_[um]) ||
                        (is_top_bottom_[um] && !is_dummy_[node])) {
                        continue;
                    }

                    if (!marked_[edge] &&
                        (r == -1 || (hdir == LR::LEFT ? r < order_[um]
                                                      : r > order_[um]))) {
                        aligns_[um]   = node;
                        roots_[node]  = roots_[um];
                        aligns_[node] = roots_[node];
                        r             = order_[um];
                        drift_[node] =
                            drift_[um] +
                            ((start_x_offset_[edge] - end_x_offset_[edge]) *
                             (vdir == TD::TOP ? 1 : -1));
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

        while (hdir == LR::LEFT ? 0 < order : order < layer.size() - 1) {
            order += hdir == LR::LEFT ? -1 : 1;

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
            if (is_top_bottom_[w]) {
                w = aligns_[w];
                continue;
            }

            const auto* p = pred(w);
            if (p == nullptr) {
                w = aligns_[w];
                continue;
            }

            const auto* u = roots_[p];
            place_block(u);

            if (sinks_[node] == node) {
                sinks_[node] = sinks_[u];
            }

            if (sinks_[node] != sinks_[u]) {
                if (hdir == LR::LEFT) {
                    shifts_[sinks_[u]] = std::min(
                        shifts_[sinks_[u]],
                        static_cast<int64_t>(xs_[node] + drift_[w] - xs_[u] -
                                             delta - drift_[p] - widths_[p]));
                } else {
                    shifts_[sinks_[u]] = std::min(
                        shifts_[sinks_[u]],
                        static_cast<int64_t>(xs_[u] + drift_[p] - xs_[node] -
                                             drift_[w] + widths_[w] + delta));
                }
            } else {
                if (hdir == LR::LEFT) {
                    xs_[node] = std::max(xs_[node], xs_[u] + delta + drift_[p] +
                                                        widths_[p] - drift_[w]);
                } else {
                    xs_[node] = std::min(xs_[node], xs_[u] - delta + drift_[p] -
                                                        widths_[w] - drift_[w]);
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

        float min_x = 0.0F;

        for (const auto& node : g_.nodes()) {
            xs_[node]        = xs_[roots_[node]] + drift_[node];
            const auto shift = shifts_[sinks_[roots_[node]]];
            if (hdir == LR::LEFT
                    ? shift < std::numeric_limits<int64_t>::max()
                    : shift > std::numeric_limits<int64_t>::min()) {
                xs_[node] += shifts_[sinks_[roots_[node]]];
            }

            min_x = std::min(xs_[node], 0.0F);
        }

        for (const auto& node : g_.nodes()) {
            xs_[node] -= min_x;
        }
    }
};

void sort4(std::array<float, 4>& a) {
    auto swap = [](auto& x, auto& y) {
        if (x > y) {
            std::swap(x, y);
        }
    };

    swap(a[0], a[1]);
    swap(a[2], a[3]);
    swap(a[0], a[2]);
    swap(a[1], a[3]);
    swap(a[1], a[2]);
}

}  // namespace

auto triskel::make_x_coords(const IGraph& g,
                            const std::vector<std::vector<const Node*>>& layers,
                            const NodeAttribute<size_t>& layer,
                            const NodeAttribute<size_t>& order,
                            const NodeAttribute<float>& widths,
                            const NodeAttribute<bool>& is_top_bottom,
                            const NodeAttribute<bool>& is_dummy,
                            const EdgeAttribute<float>& start_x_offset,
                            const EdgeAttribute<float>& end_x_offset,
                            const float x_gutter) -> NodeAttribute<float> {
    const auto tl = FSHCA<TD::TOP, LR::LEFT>(
                        g, layers, layer, order, widths, is_top_bottom,
                        is_dummy, start_x_offset, end_x_offset, x_gutter)
                        .xs_;
    const auto tr = FSHCA<TD::TOP, LR::RIGHT>(
                        g, layers, layer, order, widths, is_top_bottom,
                        is_dummy, start_x_offset, end_x_offset, x_gutter)
                        .xs_;
    const auto dl = FSHCA<TD::DOWN, LR::LEFT>(
                        g, layers, layer, order, widths, is_top_bottom,
                        is_dummy, start_x_offset, end_x_offset, x_gutter)
                        .xs_;
    const auto dr = FSHCA<TD::DOWN, LR::RIGHT>(
                        g, layers, layer, order, widths, is_top_bottom,
                        is_dummy, start_x_offset, end_x_offset, x_gutter)
                        .xs_;

    NodeAttribute<float> xs{g, 0.0F};

    for (const auto& node : g.nodes()) {
        auto x = std::array<float, 4>{tl[node], tr[node], dl[node], dr[node]};
        sort4(x);
        xs[node] = (x[1] + x[2]) / 2;
    }

    return xs;
}