#pragma once

#include <vector>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/attribute.hpp"

namespace triskel {
auto make_x_coords(const IGraph& g,
                   const std::vector<std::vector<const Node*>>& layers,
                   const NodeAttribute<size_t>& layer,
                   const NodeAttribute<size_t>& order,
                   const NodeAttribute<float>& widths,
                   const EdgeAttribute<bool>& is_dummy,
                   const EdgeAttribute<float>& start_x_offset,
                   const EdgeAttribute<float>& end_x_offset)
    -> NodeAttribute<float>;
}  // namespace triskel