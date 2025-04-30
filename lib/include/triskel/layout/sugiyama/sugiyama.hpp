#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/layout/ilayout.hpp"
#include "triskel/utils/attribute.hpp"

namespace triskel {

using Pair = std::pair<size_t, size_t>;

struct IOPair {
    NodeId node;
    EdgeId edge;

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator Pair() const {
        return {static_cast<size_t>(node), static_cast<size_t>(edge)};
    }

    friend auto operator==(IOPair lhs, IOPair rhs) -> bool {
        return static_cast<Pair>(lhs) == static_cast<Pair>(rhs);
    }

    friend auto operator<=>(IOPair lhs, IOPair rhs) -> std::strong_ordering {
        return static_cast<Pair>(lhs) <=> static_cast<Pair>(rhs);
    }
};
static_assert(std::is_trivially_copyable_v<IOPair>);

struct SugiyamaAnalysis : public ILayout {
    explicit SugiyamaAnalysis(IGraph& g);

    explicit SugiyamaAnalysis(IGraph& g, const LayoutSettings& settings);

    explicit SugiyamaAnalysis(IGraph& g,
                              const NodeAttribute<float>& heights,
                              const NodeAttribute<float>& widths,
                              const LayoutSettings& settings);

    explicit SugiyamaAnalysis(IGraph& g,
                              const NodeAttribute<float>& heights,
                              const NodeAttribute<float>& widths,
                              const EdgeAttribute<float>& start_x_offset,
                              const EdgeAttribute<float>& end_x_offset,
                              const std::vector<IOPair>& entries = {},
                              const std::vector<IOPair>& exits   = {},
                              const LayoutSettings& settings     = {});

    ~SugiyamaAnalysis() override = default;

    [[nodiscard]] auto get_x(NodeId node) const -> float override;
    [[nodiscard]] auto get_y(NodeId node) const -> float override;
    [[nodiscard]] auto get_waypoints(EdgeId edge) const
        -> const std::vector<Point>& override;

    [[nodiscard]] auto get_graph_width() const -> float;
    [[nodiscard]] auto get_graph_height() const -> float;

    [[nodiscard]] auto get_width(NodeId node) const -> float override;
    [[nodiscard]] auto get_height(NodeId node) const -> float override;

    [[nodiscard]] auto get_io_waypoints() const
        -> const std::map<Pair, std::vector<Point>>&;

   private:
    NodeAttribute<size_t> layers_;
    NodeAttribute<size_t> orders_;
    NodeAttribute<float> widths_;
    NodeAttribute<float> heights_;
    NodeAttribute<float> xs_;
    NodeAttribute<float> ys_;

    IGraph& g;

    /// @brief The number of layers in the graph view
    size_t layer_count_;

    /// @brief The inner edges that a long edge follows
    EdgeAttribute<std::vector<const Edge*>> inner_edges_;

    /// @brief Is an edge an inner edge
    EdgeAttribute<bool> is_inner_;

    /// @brief Long edges that get deleted and replaced by inner edges
    std::vector<const Edge*> long_edges_;

    /// @brief Hack
    EdgeAttribute<bool> is_flipped_;

    /// @brief Nodes that were created in Sugiyama
    std::vector<const Node*> dummy_nodes_;

    /// @brief The nodes on a given layer
    std::vector<std::vector<const Node*>> node_layers_;
    void init_node_layers();

    std::default_random_engine rng_;

    struct Padding {
        float top;
        float bottom;

        float left;
        float right;

        [[nodiscard]] auto width() const -> float { return left + right; }

        [[nodiscard]] auto height() const -> float { return top + bottom; }

        [[nodiscard]] static auto all(float padding) -> Padding {
            return {.top    = padding,
                    .bottom = padding,
                    .left   = padding,
                    .right  = padding};
        }

        [[nodiscard]] static auto horizontal(float padding) -> Padding {
            return {
                .top = 0.0F, .bottom = 0.0F, .left = padding, .right = padding};
        }

        [[nodiscard]] static auto vertical(float padding) -> Padding {
            return {
                .top = padding, .bottom = padding, .left = 0.0F, .right = 0.0F};
        }
    };

    NodeAttribute<Padding> paddings_;

    // Priorities in the coordinate assignment
    NodeAttribute<uint8_t> priorities_;

    /// @brief Coordinates an edge follows
    EdgeAttribute<std::vector<Point>> waypoints_;

    /// @deprecated
    EdgeAttribute<float> edge_weights_;

    /// @brief Edges with the same node as origin and destination
    /// These get deleted and added manually later on
    std::vector<const Edge*> self_loops_;

    /// @brief Edges entering the region
    std::vector<IOPair> entries;

    /// @brief Edges exiting the region
    std::vector<IOPair> exits;

    /// @brief The first coordinate of an edge
    EdgeAttribute<float> start_x_offset_;

    /// @brief The last coordinate of an edge
    EdgeAttribute<float> end_x_offset_;

    /// @brief Edges entering or exiting the region
    std::map<Pair, const Edge*> io_edges_;

    /// @brief Waypoints for the io edges
    std::map<Pair, std::vector<Point>> io_waypoints_;

    /// @brief The width of the graph
    float width_;

    /// @brief The height of the graph
    float height_;

    // Ensures the order on each layer has nodes 1 unit from each other
    void normalize_order();

    void remove_self_loop(const Edge* edge);

    void draw_self_loops();

    void cycle_removal();

    void layer_assignment();

    /// @brief After layer assignment attempts to move nodes that still have a
    /// degree of liberty to minimize the graph height
    void slide_nodes();

    /// @brief Edit edges so that every edge is pointing down
    /// i.e.: layer(edge.to) > layer(edge.from)
    void flip_edges();

    void remove_long_edges();

    void vertex_ordering();

    /// @brief Computes the x coordinate of each node
    void x_coordinate_assignment();

    /// @brief Compute the y coordinate of each node
    void y_coordinate_assignment();

    void coordinate_assignment_iteration(size_t layer,
                                         size_t next_layer,
                                         float graph_width);

    auto get_priority(const Node* node, size_t layer) -> size_t;

    auto min_x(std::vector<const Node*>& nodes, size_t id) -> float;

    auto max_x(std::vector<const Node*>& nodes, size_t id, float graph_width)
        -> float;

    auto average_position(const Node* node, size_t layer, bool is_going_down)
        -> float;

    void set_layer(const Node* node, size_t layer);

    /// @brief Creates waypoints to draw the edges connecting nodes
    void waypoint_creation();

    /// @brief Translate edge waypoints after coordinate assignment
    void translate_waypoints();

    /// @brief Calculates Y coordinates for waypoints
    void calculate_waypoints_y();

    auto get_waypoint_y(size_t id,
                        const std::vector<const Edge*>& edges,
                        std::vector<int64_t>& layers) -> int64_t;

    /// @brief Creates an edge waypoint and sets its layer
    auto create_ghost_node(size_t layer) -> Node*;

    /// @brief Creates an edge waypoint
    auto create_waypoint() -> Node*;

    void build_waypoints(const Edge* edge);
    void build_long_edges_waypoints();

    auto make_inner_edge(const Node* from, const Node* to) -> Edge*;

    [[nodiscard]] auto compute_graph_width() -> float;

    [[nodiscard]] auto compute_graph_height() -> float;

    // ----- Entry and exits -----
    /// @brief Ensures the entry/exit nodes are connected to the top/bottom
    /// layers
    void ensure_io_at_extremities();

    [[nodiscard]] auto is_io_edge(const Edge* edge) const -> bool;

    // Saves the data of the I/O waypoints
    void make_io_waypoint(IOPair pair);

    void make_io_waypoints();
    // -----

    bool has_top_loop_    = false;
    bool has_bottom_loop_ = false;

    friend struct Layout;
};
}  // namespace triskel