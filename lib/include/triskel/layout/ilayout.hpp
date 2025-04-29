#pragma once

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/constants.hpp"
#include "triskel/utils/point.hpp"

namespace triskel {

struct LayoutSettings {
    // =============================================================================
    // Layout
    // =============================================================================
    float WAYPOINT_WIDTH  = 0.0F;
    float WAYPOINT_HEIGHT = 0.0F;

    // Node priorities
    uint8_t DEFAULT_PRIORITY        = 0;
    uint8_t WAYPOINT_PRIORITY       = 1;
    uint8_t ENTRY_WAYPOINT_PRIORITY = 2;
    uint8_t EXIT_WAYPOINT_PRIORITY  = 2;

    // The space between nodes
    float X_GUTTER = 50.0F;

    // The space between nodes and the first / last edge
    float Y_GUTTER = 40.0F;

    // The height of an edge
    float EDGE_HEIGHT = 30.0F;

    // =============================================================================
    // Display
    // =============================================================================
    float DEFAULT_X_GUTTER = 0.0F;
    float DEFAULT_Y_GUTTER = 0.0F;
    float PADDING          = 150.0F;
};

struct ILayout {
    explicit ILayout(const LayoutSettings& settings) : settings(settings) {}

    virtual ~ILayout() = default;

    [[nodiscard]] virtual auto get_x(NodeId node) const -> float = 0;
    [[nodiscard]] virtual auto get_y(NodeId node) const -> float = 0;

    [[nodiscard]] virtual auto get_xy(NodeId node) const -> Point;

    [[nodiscard]] virtual auto get_waypoints(EdgeId edge) const
        -> const std::vector<Point>& = 0;

    [[nodiscard]] virtual auto get_width(NodeId node) const -> float  = 0;
    [[nodiscard]] virtual auto get_height(NodeId node) const -> float = 0;

    [[nodiscard]] virtual auto get_graph_width(const IGraph& graph) const
        -> float;
    [[nodiscard]] virtual auto get_graph_height(const IGraph& graph) const
        -> float;

   protected:
    const LayoutSettings& settings;
};
}  // namespace triskel
