#include "triskel/layout/sugiyama/sugiyama.hpp"
#include "triskel/layout/ilayout.hpp"
#include "triskel/layout/sugiyama/layer_assignement.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <map>
#include <ranges>
#include <stack>
#include <vector>

#include <fmt/base.h>
#include <fmt/core.h>
#include <fmt/printf.h>

#include "triskel/analysis/dfs.hpp"
#include "triskel/graph/igraph.hpp"
#include "triskel/layout/sugiyama/vertex_ordering.hpp"
#include "triskel/layout/sugiyama/x_coordinate_assignment.hpp"
#include "triskel/utils/attribute.hpp"
#include "triskel/utils/generator.hpp"
#include "triskel/utils/point.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {
void graph_sanity(const IGraph& g) {
    for (const auto* node : g.nodes()) {
        assert(static_cast<size_t>(node->id()) < 500);

        for (const auto* e : node->edges()) {
            assert(static_cast<size_t>(e->id()) < 500);

            const auto& n = e->other(*node);
            assert(static_cast<size_t>(n->id()) < 500);
        }
    }
}
}  // namespace

void SugiyamaAnalysis::normalize_order() {
    for (size_t l = 0; l < layer_count_; ++l) {
        auto& nodes = node_layers_[l];

        // THIS IS IMPORTANT
        std::ranges::shuffle(nodes, rng_);

        std::ranges::sort(nodes, [this](const Node* a, const Node* b) {
            return orders_[*a] < orders_[*b];
        });

        auto order = 0;

        for (const auto* node : nodes) {
            orders_[*node] = order;
            order++;
        }
    }
}

void SugiyamaAnalysis::init_node_layers() {
    node_layers_.clear();
    node_layers_.resize(layer_count_);

    for (const auto* node : g.nodes()) {
        node_layers_[layers_.get(*node)].push_back(node);
    }
}

SugiyamaAnalysis::SugiyamaAnalysis(IGraph& g)
    : SugiyamaAnalysis(g,
                       NodeAttribute<float>{g, 1.0F},
                       NodeAttribute<float>{g, 1.0F},
                       EdgeAttribute<float>{g, -1.0F},
                       EdgeAttribute<float>{g, -1.0F}) {}

SugiyamaAnalysis::SugiyamaAnalysis(IGraph& g, const LayoutSettings& settings)
    : SugiyamaAnalysis(g,
                       NodeAttribute<float>{g, 1.0F},
                       NodeAttribute<float>{g, 1.0F},
                       EdgeAttribute<float>{g, -1.0F},
                       EdgeAttribute<float>{g, -1.0F},
                       {},
                       {},
                       settings) {}

SugiyamaAnalysis::SugiyamaAnalysis(IGraph& g,
                                   const NodeAttribute<float>& heights,
                                   const NodeAttribute<float>& widths,
                                   const LayoutSettings& settings)
    : SugiyamaAnalysis(g,
                       heights,
                       widths,
                       EdgeAttribute<float>(g, -1),
                       EdgeAttribute<float>(g, -1),
                       {},
                       {},
                       settings) {}

SugiyamaAnalysis::SugiyamaAnalysis(IGraph& g,
                                   const NodeAttribute<float>& heights,
                                   const NodeAttribute<float>& widths,
                                   const EdgeAttribute<float>& start_x_offset,
                                   const EdgeAttribute<float>& end_x_offset,
                                   const std::vector<IOPair>& entries,
                                   const std::vector<IOPair>& exits,
                                   const LayoutSettings& settings)
    : ILayout(settings),
      layers_(g, 0),
      orders_(g, 0),
      waypoints_(g, {}),
      widths_(widths),
      heights_(heights),
      paddings_(g, Padding::horizontal(settings.X_GUTTER)),
      xs_(g, 0.0F),
      ys_(g, 0.0F),
      inner_edges_(g, {}),
      is_flipped_(g, false),
      is_top_bottom_(g, false),
      edge_weights_(g, 1.0F),
      priorities_(g, 0),
      entries(entries),
      exits(exits),
      start_x_offset_(start_x_offset),
      end_x_offset_(end_x_offset),
      is_inner_(g, false),
      is_dummy_(g, false),
      g{g}

{
    auto& ge = g.editor();
    ge.push();

    cycle_removal();

    layer_assignment();

    ensure_io_at_extremities();

    remove_long_edges();

    init_node_layers();

    ge.push();
    flip_edges();

    vertex_ordering();

    waypoint_creation();

    x_coordinate_assignment();

    translate_waypoints();

    y_coordinate_assignment();

    height_ = compute_graph_height();
    width_  = compute_graph_width();

    ge.pop();

    make_io_waypoints();

    build_long_edges_waypoints();

    // Sets the edge waypoints

    ge.pop();

    // Fix the self loops
    draw_self_loops();

    // Remove the "kink" in the back edges from the ghost nodes
    for (const auto* edge : long_edges_) {
        if (ys_[edge->from] < ys_[edge->to]) {
            continue;
        }

        auto& waypoints = waypoints_.get(*edge);

        waypoints.erase(waypoints.begin() + 3);
        waypoints.erase(waypoints.begin() + 3);
        // waypoints.erase(waypoints.begin() + 3);
        // waypoints.erase(waypoints.begin() + 3);

        waypoints.erase(waypoints.end() - 4);
        waypoints.erase(waypoints.end() - 4);
        // waypoints.erase(waypoints.end() - 4);
        // waypoints.erase(waypoints.end() - 4);
    }
}

// We want to resize the node to take into account the edge
void SugiyamaAnalysis::remove_self_loop(const Edge* edge) {
    const auto& node = edge->to;

    // Change the padding to account for the new edge
    auto& padding = paddings_[node];
    padding.right += settings.X_GUTTER;
    padding.top += settings.EDGE_HEIGHT;
    padding.bottom += settings.EDGE_HEIGHT;

    self_loops_.push_back(edge);

    g.editor().remove_edge(*edge);
}

void SugiyamaAnalysis::draw_self_loops() {
    for (const auto* edge : self_loops_) {
        const auto& node = edge->from;

        const auto width  = widths_[node];
        const auto height = heights_[node];

        // Revert the changes to the height and width
        auto& padding = paddings_[node];
        padding.right -= settings.X_GUTTER;
        padding.top -= settings.EDGE_HEIGHT;
        padding.bottom -= settings.EDGE_HEIGHT;

        // Translate the node down
        const auto x = xs_[node];
        auto& y      = ys_[node];

        auto top_x = x + (width / static_cast<float>(node->parent_count() + 1) *
                          static_cast<float>(node->parent_count()));
        auto bottom_x =
            x + (width / static_cast<float>(node->children_count() + 1) *
                 static_cast<float>(node->children_count()));

        waypoints_.set(*edge,
                       {{.x = bottom_x, .y = y + height},
                        {.x = bottom_x, .y = y + height + settings.EDGE_HEIGHT},
                        {.x = x + width + settings.X_GUTTER,
                         .y = y + height + settings.EDGE_HEIGHT},
                        {.x = x + width + settings.X_GUTTER,
                         .y = y - settings.EDGE_HEIGHT},
                        {.x = top_x, .y = y - settings.EDGE_HEIGHT},
                        {.x = top_x, .y = y}});
    }
}

// The idea of this function is to reverse each backedge
// However if the backedge is from a node to itself, we have to remove that
// edge
void SugiyamaAnalysis::cycle_removal() {
    auto dfs = DFSAnalysis(g);
    auto& ge = g.editor();

    auto edges = span_to_vec(g.edges());

    for (const auto* edge : edges) {
        if (dfs.is_backedge(*edge)) {
            // Self loop
            if (edge->to == edge->from) {
                remove_self_loop(edge);
                continue;
            }

            ge.edit_edge(*edge, *edge->to, *edge->from);
            is_flipped_.set(*edge, true);
        }
    }
}

void SugiyamaAnalysis::layer_assignment() {
    auto layers  = network_simplex(g);
    layers_      = layers->layers;
    layer_count_ = layers->layer_count;
}

struct SlideCandidate {
    Node node;
    size_t min_layer;
    size_t max_layer;
    float height;
};

void SugiyamaAnalysis::slide_nodes() {
    // auto candidates = std::vector<SlideCandidate>{};

    // for (const auto* node : g.nodes()) {
    //     const auto layer = layers_[*node];

    //     auto neighbor_layers =
    //         node.neighbors()  //
    //         | std::ranges::views::transform(
    //               [&](const Node* n) { return layers_.get(n); })  //
    //         | std::ranges::to<std::vector<size_t>>();

    //     auto smaller_layers =
    //         neighbor_layers |
    //         std::ranges::views::filter([&](size_t l) { return l <= layer; });

    //     auto min_layer = layer;
    //     if (!smaller_layers.empty()) {
    //         min_layer = std::ranges::max(smaller_layers);
    //         min_layer += 1;
    //     }
    //     assert(min_layer <= layer);

    //     auto bigger_layers =
    //         neighbor_layers |
    //         std::ranges::views::filter([&](size_t l) { return l >= layer; });

    //     auto max_layer = layer;
    //     if (!bigger_layers.empty()) {
    //         max_layer = std::ranges::min(bigger_layers);
    //         max_layer -= 1;
    //     }
    //     assert(layer <= max_layer);

    //     if (min_layer == max_layer) {
    //         continue;
    //     }

    //     candidates.push_back({.node      = node,
    //                           .min_layer = min_layer,
    //                           .max_layer = max_layer,
    //                           .height    = heights_.get(node)});
    // }

    // std::ranges::sort(candidates,
    //                   [](const SlideCandidate& c1, const SlideCandidate& c2)
    //                   {
    //                       return c1.height > c2.height;
    //                   });

    // for (const auto& candidate : candidates) {
    //     const auto* node     = candidate.node;
    //     const auto min_layer = candidate.min_layer;
    //     const auto max_layer = candidate.max_layer;
    //     const auto layer     = layers_.get(node);

    //     auto best_height = compute_graph_height();
    //     auto best_layer  = layer;

    //     for (size_t r = min_layer; r <= max_layer; ++r) {
    //         if (r == layer) {
    //             continue;
    //         }

    //         layers_.set(node, r);
    //         const auto height = compute_graph_height();
    //         if (height < best_height) {
    //             best_height = height;
    //             best_layer  = r;
    //         }
    //     }

    //     layers_.set(node, best_layer);
    // }
}

void SugiyamaAnalysis::set_layer(const Node* node, size_t layer) {
    assert(layer >= 0);
    assert(layer < layer_count_);

    layers_[node] = layer;

    if (layer == layer_count_) {
        has_top_loop_ = true;
    }

    if (layer == 0) {
        has_bottom_loop_ = true;
    }
}

auto SugiyamaAnalysis::create_waypoint() -> Node* {
    auto& editor   = g.editor();
    auto* waypoint = editor.make_node();

    heights_[waypoint]    = settings.WAYPOINT_HEIGHT;
    widths_[waypoint]     = settings.WAYPOINT_WIDTH;
    priorities_[waypoint] = settings.WAYPOINT_PRIORITY;
    dummy_nodes_.push_back(waypoint);

    return waypoint;
}

auto SugiyamaAnalysis::create_ghost_node(size_t layer) -> Node* {
    auto* waypoint = create_waypoint();
    set_layer(waypoint, layer);
    return waypoint;
}

auto SugiyamaAnalysis::is_io_edge(const Edge* edge) const -> bool {
    return std::ranges::any_of(
        io_edges_, [&edge](const auto& kv) { return kv.second == edge; });
}

auto SugiyamaAnalysis::make_inner_edge(const Node* from, const Node* to)
    -> Edge* {
    auto* new_edge      = g.editor().make_edge(*from, *to);
    is_inner_[new_edge] = true;
    return new_edge;
}

// TODO: split edges on the same layer
void SugiyamaAnalysis::remove_long_edges() {
    std::stack<const Edge*> edges_to_split;

    for (const auto* edge : g.edges()) {
        auto from_layer = layers_[edge->from];
        auto to_layer   = layers_[edge->to];

        auto bottom_layer = std::min(from_layer, to_layer);
        auto top_layer    = std::max(from_layer, to_layer);

        if (top_layer - bottom_layer > 1 || is_flipped_.get(*edge)) {
            edges_to_split.push(edge);
        }
    }

    auto& ge = g.editor();
    while (!edges_to_split.empty()) {
        const auto* edge = edges_to_split.top();
        edges_to_split.pop();

        auto& inner_edges = inner_edges_[edge];

        if (!is_io_edge(edge)) {
            // IO edges are handles differently
            long_edges_.push_back(edge);
        }

        // Arrows go from top to bottom

        auto from_layer = layers_[edge->from];
        auto to_layer   = layers_[edge->to];

        auto bottom_layer = std::min(from_layer, to_layer);
        auto top_layer    = std::max(from_layer, to_layer);

        auto* bottom = bottom_layer == from_layer ? edge->from : edge->to;
        auto* top    = top_layer == to_layer ? edge->to : edge->from;

        auto is_flipped = is_flipped_[edge];

        auto is_going_up = ((is_flipped && (from_layer != bottom_layer)) ||
                            (!is_flipped && (from_layer == bottom_layer)));

        if (is_going_up) {
            // We need to insert additional nodes for the edge to wrap around
            bottom_layer -= 2;
            top_layer += 2;
        }

        auto* previous_point = bottom;
        for (size_t layer = bottom_layer + 1; layer < top_layer; layer++) {
            auto* waypoint      = create_ghost_node(layer);
            is_dummy_[waypoint] = true;

            auto* new_edge = make_inner_edge(waypoint, previous_point);
            inner_edges.push_back(new_edge);
            if (is_going_up && (layer == bottom_layer + 1)) {
                edge_weights_[new_edge]  = 0;
                is_top_bottom_[waypoint] = true;
            }

            previous_point = waypoint;
        }

        auto* new_edge = make_inner_edge(top, previous_point);
        inner_edges.push_back(new_edge);
        if (is_going_up) {
            edge_weights_[new_edge]        = 0;
            is_top_bottom_[previous_point] = true;
        }

        // TODO: I can just say these edges are flipped and remove a lot of
        // complexity
        if (!is_going_up) {
            std::ranges::reverse(inner_edges);
            for (const auto* edge : inner_edges) {
                ge.edit_edge(*edge, *edge->to, *edge->from);
            }
        }

        ge.remove_edge(*edge);
    }
}

void SugiyamaAnalysis::vertex_ordering() {
    auto ordering = VertexOrdering(g, layers_, layer_count_);
    orders_       = ordering.orders_;
    for (size_t l = 0; l < layer_count_; ++l) {
        auto& nodes = node_layers_[l];

        std::ranges::sort(nodes, [this](const Node* a, const Node* b) {
            return orders_[*a] < orders_[*b];
        });
    }
};

auto SugiyamaAnalysis::get_priority(const Node* node, size_t layer) -> size_t {
    if (std::ranges::contains(dummy_nodes_, node)) {
        return -1;
    }

    return std::ranges::distance(
        node->edges()  //
        | std::ranges::views::transform(
              [&node](const Edge* e) -> Node* { return e->other(*node); })  //
        | std::ranges::views::filter(
              [&](const Node* n) { return layers_[n] == layer; }));
}

auto SugiyamaAnalysis::min_x(std::vector<const Node*>& nodes, size_t id)
    -> float {
    auto priority = priorities_[*nodes[id]];
    auto w        = 0.0F;

    for (size_t i = id - 1; i < id; --i) {
        w += widths_[*nodes[i]] + paddings_[*nodes[i]].width();

        // Nodes are laid out left to right so we also care about equal
        // priority nodes
        if (priorities_[*nodes[i]] >= priority) {
            return xs_[*nodes[i]] + w;
        }
    }
    // The left gutter of this block
    w += paddings_[*nodes[id]].left;

    return w;
}

auto SugiyamaAnalysis::max_x(std::vector<const Node*>& nodes,
                             size_t id,
                             float graph_width) -> float {
    auto priority = priorities_[*nodes[id]];
    auto w        = widths_[*nodes[id]] + paddings_[*nodes[id]].right;

    for (size_t i = id + 1; i < nodes.size(); ++i) {
        // Nodes are laid out left to right so we only care about higher
        // priority nodes
        if (priorities_[*nodes[i]] > priority) {
            return xs_[*nodes[i]] - w;
        }

        w += widths_[*nodes[i]] + paddings_[*nodes[i]].width();
    }
    assert(graph_width >= w);
    return graph_width - w;
}

auto SugiyamaAnalysis::average_position(const Node* node,
                                        size_t layer,
                                        bool is_going_down) -> float {
    auto n = 0.0F;
    auto d = 0.0F;

    for (const auto* edge : node->edges()) {
        const auto& child = edge->other(*node);

        if (layers_[child] == layer) {
            const auto w          = edge_weights_[*edge];
            const auto& waypoints = waypoints_[*edge];
            auto waypoint_offset  = waypoints[1].x - waypoints[2].x;

            if (is_going_down) {
                waypoint_offset *= -1;
            }

            n += (xs_[child] + waypoint_offset) * w;
            d += w;
        }
    }

    if (d == 0.0F) {
        return -1.0F;
    }

    return n / d;
}

void SugiyamaAnalysis::coordinate_assignment_iteration(size_t layer,
                                                       size_t next_layer,
                                                       float graph_width) {
    auto nodes = node_layers_[layer];

    auto sorted_indexes =
        std::views::iota(static_cast<size_t>(0), nodes.size()) |
        std::ranges::to<std::vector<size_t>>();

    std::ranges::sort(sorted_indexes, [&](size_t a, size_t b) {
        auto pa = priorities_[*nodes[a]];
        auto pb = priorities_[*nodes[b]];
        return (pa > pb);
    });

    for (auto i : sorted_indexes) {
        const auto* node = nodes[i];

        auto lo = min_x(nodes, i);
        auto hi = max_x(nodes, i, graph_width);

        if (std::abs(hi - lo) < 0.01) {
            hi = lo;
        }

        if (lo > hi) {
            lo = (lo + hi) / 2;
            hi = lo;
            // FIXME: weird
            // fmt::print("LO/HI error\n");
        }

        assert(lo <= hi);

        auto avg = average_position(node, next_layer, next_layer < layer);

        if (avg >= 0) {
            auto x     = std::clamp(avg, lo, hi);
            xs_[*node] = x;
        } else {
            auto x     = std::clamp(xs_[*node], lo, hi);
            xs_[*node] = x;
        }
    }
}

auto SugiyamaAnalysis::compute_graph_width() -> float {
    auto graph_width = 0.0F;

    for (const auto& layer : node_layers_) {
        auto layer_width = 0.0F;

        for (const auto* node : layer) {
            layer_width = std::max(layer_width, widths_[node] + xs_[node]);
        }

        graph_width = std::max(graph_width, layer_width);
    }

    return graph_width;
}

auto SugiyamaAnalysis::get_graph_width() const -> float {
    return width_;
}

auto SugiyamaAnalysis::get_graph_height() const -> float {
    return height_;
}

auto SugiyamaAnalysis::compute_graph_height() -> float {
    auto max_y = 0.0F;
    auto min_y = std::numeric_limits<float>::min();

    for (const auto* node : g.nodes()) {
        const auto y = ys_[node];
        min_y        = std::min(min_y, y);
        max_y        = std::max(max_y, y + heights_[node]);
    }

    for (const auto* node : g.nodes()) {
        ys_[node] -= min_y;
    }

    return max_y - min_y;
}

#if 1
void SugiyamaAnalysis::x_coordinate_assignment() {
    xs_ = make_x_coords(g, node_layers_, layers_, orders_, widths_,
                        is_top_bottom_, is_dummy_, start_x_offset_,
                        end_x_offset_, settings.X_GUTTER);
}

#else
void SugiyamaAnalysis::x_coordinate_assignment() {
    auto priorities = NodeAttribute<size_t>{g.max_node_id(), 0};

    const float graph_width = compute_graph_width();

    // Init X coordinates
    for (size_t layer = 0; layer < layer_count_; ++layer) {
        auto nodes = node_layers_[layer];

        std::ranges::sort(nodes, [&](const Node* a, const Node* b) {
            return orders_[a] < orders_[b];
        });

        auto x = 0.0F;

        for (const auto* node : nodes) {
            auto& padding = paddings_[node];
            x += padding.left;
            xs_[node] = x;
            x += widths_[node] + padding.right;
        }
    }

    for (size_t i = 0; i < 5; ++i) {
        for (size_t r = 0 + 1; r < layer_count_; ++r) {
            coordinate_assignment_iteration(r, r - 1, graph_width);
        }

        for (size_t r = layer_count_ - 1; r < layer_count_; --r) {
            coordinate_assignment_iteration(r, r + 1, graph_width);
        }
    }

    for (size_t r = 0 + 1; r < layer_count_; ++r) {
        coordinate_assignment_iteration(r, r - 1, graph_width);
    }
}
#endif

auto SugiyamaAnalysis::get_x(NodeId node) const -> float {
    return xs_.get(node);
}
auto SugiyamaAnalysis::get_y(NodeId node) const -> float {
    return ys_.get(node);
}

auto SugiyamaAnalysis::get_width(NodeId node) const -> float {
    return widths_.get(node);
}

auto SugiyamaAnalysis::get_height(NodeId node) const -> float {
    return heights_.get(node);
}

auto SugiyamaAnalysis::get_waypoints(EdgeId edge) const
    -> const std::vector<Point>& {
    return waypoints_.get(edge);
}

auto SugiyamaAnalysis::get_long_edge_order(const Node* node, const Node* dummy)
    -> size_t {
    std::array<const Node*, 2> neighbors;
    if (dummy->children_count() == 0) {
        assert(dummy->parent_count() == 2);
        neighbors[0] = dummy->parent_edges()[0]->from;
        neighbors[1] = dummy->parent_edges()[1]->from;
    } else {
        assert(dummy->children_count() == 2);
        neighbors[0] = dummy->child_edges()[0]->to;
        neighbors[1] = dummy->child_edges()[1]->to;
    }

    return neighbors[0] == node ? orders_[neighbors[1]] : orders_[neighbors[0]];
}

// TODO: it's kind of odd that the xs are offsets and ys are coords
void SugiyamaAnalysis::waypoint_creation() {
    for (const auto* edge : g.edges()) {
        auto& waypoints = waypoints_[edge];
        waypoints.resize(4, {.x = 0.0F, .y = 0.0F});
    }

    for (size_t layer = 0; layer < layer_count_; ++layer) {
        // Sort the nodes by order
        auto nodes = node_layers_[layer];

        std::ranges::sort(nodes, [&](const Node* a, const Node* b) {
            return orders_[a] < orders_[b];
        });

        // EXIT EDGES
        for (const auto* node : nodes) {
            auto y0 = ys_[node] + heights_[node];

            // Sort the edges by destination order
            // FIXME: vector
            auto edges = span_to_vec(node->child_edges());
            std::ranges::sort(edges, [&](const Edge* a, const Edge* b) {
                auto order_a = orders_[a->to];
                auto order_b = orders_[b->to];

                // TODO: backedge

                if (order_a == order_b) {
                    return end_x_offset_[a] < end_x_offset_[b];
                }

                return order_a < order_b;
            });

            auto spacer = widths_[node] / static_cast<float>(edges.size() + 1);

            auto x = spacer;

            for (const auto* edge : edges) {
                assert(layers_[edge->from] > layers_[edge->to]);

                // Creates 4 waypoints (Xs):
                // We calculate the x and y coordinates of the first 2 nodes
                // And the y coordinate of the extremities
                // |__X__|
                //    |
                //    X----X
                //         |
                //       __X__
                //      |     |

                auto& waypoints = waypoints_[edge];

                if (start_x_offset_[edge] < 0) {
                    waypoints[0].x        = x;
                    waypoints[1].x        = x;
                    start_x_offset_[edge] = x;
                } else {
                    // The value is imposed
                    waypoints[0].x = start_x_offset_[edge];
                    waypoints[1].x = start_x_offset_[edge];
                }

                x += spacer;
            }
        }

        // ENTRY EDGES
        for (const auto* node : nodes) {
            auto edges = span_to_vec(node->parent_edges());
            std::ranges::sort(edges, [&, this](const Edge* a, const Edge* b) {
                const auto order_a = orders_[a->from];
                const auto order_b = orders_[b->from];

                {  // Backedge logic
                    const auto order = orders_[node];

                    if (is_top_bottom_[a->from]) {
                        const auto order_a = get_long_edge_order(node, a->from);

                        if (is_top_bottom_[b->from]) {
                            const auto order_b =
                                get_long_edge_order(node, b->from);

                            if (order_a < order) {
                                return (order_b > order) || (order_b < order_a);
                            }

                            return (order_b > order && order_b < order_a);
                        }

                        return order_a < order;
                    }

                    if (is_top_bottom_[b->from]) {
                        const auto order_b = get_long_edge_order(node, b->from);
                        return order < order_b;
                    }
                }

                if (order_a == order_b) {
                    return start_x_offset_[a] < start_x_offset_[b];
                }

                return order_a < order_b;
            });

            auto spacer = widths_[node] / static_cast<float>(edges.size() + 1);

            auto x = spacer;

            for (const auto* edge : edges) {
                // Creates 4 waypoints (Xs):
                // We calculate the x and y coordinates of the last 2 nodes
                // |__X__|
                //    |
                //    X----X
                //         |
                //       __X__
                //      |     |

                auto& waypoints = waypoints_[edge];

                if (end_x_offset_[edge] < 0) {
                    waypoints[2].x      = x;
                    waypoints[3].x      = x;
                    end_x_offset_[edge] = x;
                } else {
                    // The value is imposed
                    waypoints[2].x = end_x_offset_[edge];
                    waypoints[3].x = end_x_offset_[edge];
                }

                x += spacer;
            }
        }
    }
}

// Use dynamic programming to determine edge heights
//
// We want to ensure the down portion of an edge does not cross with the
// horizontal part of another edge.
//
// This is a topological sort. This function is roughly the DFS solution
//
// In the example we want to ensure 1-2 is on a higher level than b-c
// because c.x is in [1.x, 2.x]
//
//  a  0       |
//  |  |      \/
//  |  1 ------- 2
//  |            |
//  b---------c  |
//            |  |
//            d  3
//
// NOLINTNEXTLINE(misc-no-recursion)
auto SugiyamaAnalysis::get_waypoint_y(size_t id,
                                      const std::vector<const Edge*>& edges,
                                      std::vector<int64_t>& layers) -> int64_t {
    if (layers[id] != std::numeric_limits<int64_t>::min()) {
        // This includes std::numeric_limits<int64_t>::max()
        return layers[id];
    }

    // Marker
    layers[id] = std::numeric_limits<int64_t>::max();

    const auto* edge = edges[id];

    // Find all segments that contain the third waypoint (2) (It's the top of
    // 2-3, the segment going down)
    //
    //  0
    //  |
    //  1 --- 2
    //        |
    //        3
    //

    const auto& waypoints = waypoints_[edge];

    auto x1 = waypoints[1].x;
    auto x2 = waypoints[2].x;

    // Add a temporary mark to this node to detect infinite loops

    // The layer the edge 1-2 will rest on
    int64_t lmax = std::numeric_limits<int64_t>::min();
    int64_t lmin = std::numeric_limits<int64_t>::max();

    for (size_t i = 0; i < edges.size(); i++) {
        if (i == id) {
            continue;
        }

        const auto* other           = edges[i];
        const auto& waypoints_other = waypoints_[other];

        auto other_start = std::min(waypoints_other[1].x, waypoints_other[2].x);
        auto other_end   = std::max(waypoints_other[1].x, waypoints_other[2].x);

        if (other_start < x2 && x2 < other_end) {
            auto l = get_waypoint_y(i, edges, layers);

            if (l == std::numeric_limits<int64_t>::max()) {
                // We are in a loop: the system is over constrained
                // Example:
                // 0          a
                // |          |
                // 1 --- 2    |
                //       |    |
                //  c ---+--- b
                //  |    |
                //  d    3

                // We'll ignore this constraint
                continue;
            }

            lmin = std::min(l - 1, lmin);
        }

        else if (other_start < x1 && x1 < other_end) {
            // We don't want to perform a recursive call
            auto l = layers[i];

            if (l == std::numeric_limits<int64_t>::max() ||
                l == std::numeric_limits<int64_t>::min()) {
                continue;
            }

            lmax = std::max(l + 1, lmax);
        }
    }

    int64_t layer = 0;

    if (lmin != std::numeric_limits<int64_t>::max()) {
        layer = lmin;
    } else if (lmax != std::numeric_limits<int64_t>::min()) {
        layer = lmax;
    }

    layers[id] = layer;
    return layer;
}

void SugiyamaAnalysis::y_coordinate_assignment() {
    auto y = 0.0F;

    // The highest layer is on top
    for (size_t layer = layer_count_ - 1; layer <= layer_count_; --layer) {
        // Sort the nodes by order
        auto edges = std::vector<const Edge*>{};

        for (const auto* node : node_layers_[layer]) {
            for (const auto* edge : node->child_edges()) {
                edges.push_back(edge);
            }
        }

        auto layers       = std::vector<int64_t>(edges.size(),
                                                 std::numeric_limits<int64_t>::min());
        int64_t max_layer = std::numeric_limits<int64_t>::min();
        int64_t min_layer = std::numeric_limits<int64_t>::max();

        for (size_t i = 0; i < edges.size(); ++i) {
            auto layer = get_waypoint_y(i, edges, layers);
            max_layer  = std::max(layer, max_layer);
            min_layer  = std::min(layer, min_layer);
        }

        float max_node_height = 0.0F;
        for (const auto* node : node_layers_[layer]) {
            ys_[node]              = y;
            const auto node_height = heights_[node];
            max_node_height        = std::max(max_node_height, node_height);

            for (const auto* edge : node->parent_edges()) {
                waypoints_[edge][3].y = y;
            }

            for (const auto* edge : node->child_edges()) {
                waypoints_[edge][0].y = y + node_height;
            }
        }
        y += max_node_height;
        y += static_cast<float>(max_layer - min_layer) * settings.EDGE_HEIGHT;
        y += 2.0F * settings.Y_GUTTER;

        for (size_t i = 0; i < edges.size(); ++i) {
            const auto* edge = edges[i];
            auto& waypoints  = waypoints_[edge];

            waypoints[1].y = y - settings.Y_GUTTER -
                             static_cast<float>(layers[i] - min_layer) *
                                 settings.EDGE_HEIGHT;
            waypoints[2].y = y - settings.Y_GUTTER -
                             static_cast<float>(layers[i] - min_layer) *
                                 settings.EDGE_HEIGHT;
        }
    }
}

void SugiyamaAnalysis::translate_waypoints() {
    for (const auto* edge : g.edges()) {
        auto& waypoints = waypoints_[edge];

        waypoints[0].x += xs_[edge->from];
        waypoints[1].x += xs_[edge->from];

        waypoints[2].x += xs_[edge->to];
        waypoints[3].x += xs_[edge->to];
    }
}

void SugiyamaAnalysis::flip_edges() {
    auto& ge = g.editor();
    for (const auto* edge : g.edges()) {
        assert(layers_[edge->to] != layers_[edge->from]);

        if (layers_[edge->from] < layers_[edge->to]) {
            ge.edit_edge(*edge, *edge->to, *edge->from);
        }
    }
}

void SugiyamaAnalysis::ensure_io_at_extremities() {
    auto top_layer = layer_count_;
    layer_count_ += 1;

    // Creates nodes at the top layer that entry nodes are linked to
    for (auto entry_pair : entries) {
        const auto* ghost     = create_ghost_node(top_layer);
        auto& editor          = g.editor();
        auto* edge            = editor.make_edge(*ghost, entry_pair.node);
        io_edges_[entry_pair] = edge;
    }

    // Creates nodes at the bottom layer that exit nodes are linked to
    for (auto exit_pair : exits) {
        const auto* ghost    = create_ghost_node(0);
        auto& editor         = g.editor();
        auto* edge           = editor.make_edge(exit_pair.node, *ghost);
        io_edges_[exit_pair] = edge;
    }

    // These are not loops!
    has_top_loop_    = true;
    has_bottom_loop_ = true;
}

auto SugiyamaAnalysis::get_io_waypoints() const
    -> const std::map<Pair, std::vector<Point>>& {
    return io_waypoints_;
}

void SugiyamaAnalysis::make_io_waypoint(IOPair pair) {
    const auto* edge = io_edges_[pair];
    assert(edge != nullptr);
    build_waypoints(edge);
    io_waypoints_[pair] = waypoints_[edge];
}

void SugiyamaAnalysis::make_io_waypoints() {
    for (auto pair : entries) {
        make_io_waypoint(pair);
    }

    for (auto pair : exits) {
        make_io_waypoint(pair);
    }
}

void SugiyamaAnalysis::build_long_edges_waypoints() {
    for (const auto* edge : long_edges_) {
        build_waypoints(edge);
    }
}

void SugiyamaAnalysis::build_waypoints(const Edge* edge) {
    auto& waypoints      = waypoints_[edge];
    auto& edge_waypoints = inner_edges_[edge];

    // No waypoints to build
    if (edge_waypoints.empty()) {
        return;
    }

    for (const auto* edge : edge_waypoints) {
        auto& ws = waypoints_[edge];
        if (layers_[edge->from] < layers_[edge->to]) {
            waypoints.push_back(ws[0]);
            waypoints.push_back(ws[1]);
            waypoints.push_back(ws[2]);
            waypoints.push_back(ws[3]);
        } else {
            waypoints.push_back(ws[3]);
            waypoints.push_back(ws[2]);
            waypoints.push_back(ws[1]);
            waypoints.push_back(ws[0]);
        }
    }
}