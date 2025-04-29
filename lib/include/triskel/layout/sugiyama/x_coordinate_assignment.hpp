#pragma once

#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"

namespace triskel {
struct XCoordAssignment {
    /// @brief node_layers contains for each layer the nodes sorted by order
    XCoordAssignment(const IGraph& g,
                     const std::vector<std::vector<const Node*>>& node_layers);

    const IGraph& g_;
    std::vector<std::vector<const Node*>>& node_layers_;

    NodeAttribute<float> x_;
};
}  // namespace triskel