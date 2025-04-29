#include "triskel/layout/layout.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <deque>
#include <memory>
#include <vector>

#include "triskel/analysis/sese.hpp"
#include "triskel/graph/graph.hpp"
#include "triskel/graph/igraph.hpp"
#include "triskel/graph/subgraph.hpp"
#include "triskel/layout/ilayout.hpp"
#include "triskel/layout/sugiyama/sugiyama.hpp"
#include "triskel/utils/attribute.hpp"
#include "triskel/utils/point.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

Layout::Layout(Graph& g) : Layout(g, {}) {}

Layout::Layout(Graph& g, const LayoutSettings& settings)
    : Layout(g,
             NodeAttribute<float>{g, 1.0F},
             NodeAttribute<float>{g, 1.0F},
             settings) {}

Layout::Layout(Graph& g,
               const NodeAttribute<float>& heights,
               const NodeAttribute<float>& widths,
               const LayoutSettings& settings)
    : ILayout(settings),
      g_{g},
      xs_(g, 0.0F),
      ys_(g, 0),
      waypoints_(g, {}),
      start_x_offset_(g, -1),
      end_x_offset_(g, -1),
      heights_(heights),
      widths_(widths)

{
    sese_ = std::make_unique<SESE>(g);

    remove_small_regions();

    g.editor().push();
    init_regions();

    compute_layout(*sese_->regions.root);
    translate_region(*sese_->regions.root);

    g.editor().pop();

    // Tie loose ends
    for (const auto& region : sese_->regions.nodes) {
        auto& data = regions_data_[region->id];

        // Add the exit waypoints
        for (auto exit_pair : data.exits) {
            auto& exit_waypoints = data.io_waypoints[exit_pair];
            assert(!exit_waypoints.empty());
            auto& waypoints = waypoints_[exit_pair.edge];

            // Remove the nodes connecting it to the next layer
            waypoints.erase(waypoints.begin());
            waypoints.front().x = exit_waypoints.back().x;
            exit_waypoints.pop_back();

            waypoints.insert(waypoints.begin(), exit_waypoints.begin(),
                             exit_waypoints.end());
        }

        for (auto entry_pair : data.entries) {
            auto& entry_waypoints = data.io_waypoints[entry_pair];
            assert(!entry_waypoints.empty());
            auto& waypoints = waypoints_[entry_pair.edge];

            // Remove the nodes connecting it to the next layer
            waypoints.pop_back();
            waypoints.back().x = entry_waypoints.front().x;
            entry_waypoints.erase(entry_waypoints.begin());

            waypoints.insert(waypoints.end(), entry_waypoints.begin(),
                             entry_waypoints.end());
        }
    }
}

void Layout::remove_small_regions() {
    // Remove SESE regions with a single node
    std::deque<SESE::SESERegion*> small_regions;
    for (auto& region : sese_->regions.nodes) {
        if ((*region)->nodes.size() == 1 && region->children().empty() &&
            !region->is_root()) {
            small_regions.push_back(region.get());
        }
    }

    while (!small_regions.empty()) {
        auto& region = small_regions.back();
        small_regions.pop_back();

        const auto* node_id = (*region)->nodes.front();

        region->parent()->nodes.push_back(node_id);
        sese_->node_regions[node_id] = &region->parent();

        sese_->regions.remove_node(region);
    }

    // fix the ids
    for (size_t i = 0; i < sese_->regions.nodes.size(); ++i) {
        sese_->regions.nodes[i]->id = i;
    }
}

void Layout::create_region_subgraphs() {
    for (const auto* node : g_.nodes()) {
        const auto& r = sese_->get_region(node);
        auto& editor  = get_editor(r);
        editor.push();
        editor.select_node(*node);
    }
}

void Layout::create_region_nodes() {
    auto& editor = g_.editor();

    for (const auto& r : sese_->regions.nodes) {
        auto& data    = regions_data_[r->id];
        data.node_ptr = editor.make_node();
    }

    // Adds the region nodes to the appropriate regions
    for (const auto& r : sese_->regions.nodes) {
        if (r->is_root()) {
            continue;
        }

        const auto& parent_region = r->parent();
        auto& editor = regions_data_[parent_region.id].subgraph->editor();
        editor.select_node(*regions_data_[r->id].node_ptr);
    }
}

void Layout::edit_region_entry(const SESE::SESERegion& r) {
    const auto* entry_id = r->entry_edge;
    if (entry_id == nullptr) {
        return;
    }

    const auto& entry = *entry_id;

    const auto& parent_region = r.parent();
    const auto& from_region   = sese_->get_region(entry.from);

    const auto* to_node = get_region_node(r);

    const auto* from_node = from_region == parent_region
                                ? entry.from
                                : get_region_node(from_region);

    auto& ge = get_editor(parent_region);
    ge.select_node(*to_node);
    ge.edit_edge(entry, *from_node, *to_node);
}

void Layout::edit_region_exit(const SESE::SESERegion& r) {
    const auto* exit_id = r->exit_edge;
    if (exit_id == nullptr) {
        return;
    }

    const auto& exit = exit_id;

    const auto& parent_region = r.parent();
    const auto& to_region     = sese_->get_region(exit->to);

    const auto& from_node = get_region_node(r);

    const auto& to_node =
        to_region == parent_region ? exit->to : get_region_node(to_region);

    auto& ge = get_editor(parent_region);
    ge.select_node(*to_node);
    ge.edit_edge(*exit, *from_node, *to_node);
}

namespace {
// Is `successor` a successor of `region` that is, is `successor` contained
// within `region`
// NOLINTNEXTLINE(misc-no-recursion)
auto is_region_successor(const SESE::SESERegion& region,
                         const SESE::SESERegion& successor) -> bool {
    if (successor == region) {
        return true;
    }

    if (successor.depth <= region.depth) {
        return false;
    }

    return is_region_successor(region, successor.parent());
}

[[nodiscard]] auto get_closest_ancestor(const SESE::SESERegion& r1,
                                        const SESE::SESERegion& r2)
    -> const SESE::SESERegion& {
    auto depth = std::min(r1.depth, r2.depth);

    const auto* r1_ = &r1;
    const auto* r2_ = &r2;

    while (r1_->depth != depth) {
        r1_ = &r1_->parent();
    }

    while (r2_->depth != depth) {
        r2_ = &r2_->parent();
    }

    while (r1_ != r2_) {
        r1_ = &r1_->parent();
        r2_ = &r2_->parent();
    }

    assert(r1_ == r2_);

    return *r1_;
}

}  // namespace

void Layout::edit_region_subgraph() {
    // return;
    for (const auto* edge : g_.edges()) {
        const auto* from_region = &sese_->get_region(edge->from);
        const auto* to_region   = &sese_->get_region(edge->to);

        if (from_region != to_region) {
            if (is_region_successor(*from_region, *to_region)) {
                // This is an entry edge
                const auto* parent_region = from_region;
                const auto* child_region  = to_region;
                const auto* node_ptr      = edge->to;

                while (child_region != parent_region) {
                    // make everyone an entry node
                    regions_data_[child_region->id].entries.push_back(
                        {node_ptr->id(), edge->id()});

                    node_ptr     = regions_data_[child_region->id].node_ptr;
                    child_region = &child_region->parent();
                }

                auto& ge = get_editor(*parent_region);
                ge.edit_edge(*edge, *edge->from, *node_ptr);
            } else if (is_region_successor(*to_region, *from_region)) {
                // This is an exit edge
                const auto* parent_region = to_region;
                const auto* child_region  = from_region;
                const auto* node_ptr      = edge->from;

                while (child_region != parent_region) {
                    // make everyone an exit node
                    regions_data_[child_region->id].exits.push_back(
                        {node_ptr->id(), edge->id()});

                    node_ptr     = regions_data_[child_region->id].node_ptr;
                    child_region = &child_region->parent();
                }

                auto& ge = get_editor(*parent_region);
                ge.edit_edge(*edge, *node_ptr, *edge->to);
            } else {
                // The nodes connect through their parent
                const auto& closest_ancestor =
                    get_closest_ancestor(*to_region, *from_region);

                const auto* from_ptr = edge->from;
                while (from_region != &closest_ancestor) {
                    // make everyone an exit node
                    regions_data_[from_region->id].exits.push_back(
                        {from_ptr->id(), edge->id()});

                    from_ptr    = regions_data_[from_region->id].node_ptr;
                    from_region = &from_region->parent();
                }

                const auto* to_ptr = edge->to;
                while (to_region != &closest_ancestor) {
                    // make everyone an entry node
                    regions_data_[to_region->id].entries.push_back(
                        {to_ptr->id(), edge->id()});

                    to_ptr    = regions_data_[to_region->id].node_ptr;
                    to_region = &to_region->parent();
                }

                auto& ge = get_editor(closest_ancestor);
                ge.edit_edge(*edge, *from_ptr, *to_ptr);
            }
        }
    }
}

void Layout::init_regions() {
    // Creates the empty region structures
    regions_data_.reserve(sese_->regions.nodes.size());
    for (const auto& region : sese_->regions.nodes) {
        regions_data_.emplace_back(g_);
    }

    create_region_subgraphs();
    create_region_nodes();
    edit_region_subgraph();

    for (size_t i = 0; i < regions_data_.size(); ++i) {
        const auto& region = regions_data_[i];
        auto& ge           = region.subgraph->editor();

        if (region.entries.empty()) {
            auto& region = sese_->get_region(g_.root());
            assert(region.id == i);
            ge.make_root(*g_.root());
        } else {
            assert(region.entries.size() == 1);
            ge.make_root(region.entries.front().node);
        }
    }
}

auto Layout::get_x(NodeId node) const -> float {
    return xs_.get(node);
}

auto Layout::get_y(NodeId node) const -> float {
    return ys_.get(node);
}

auto Layout::get_width(NodeId node) const -> float {
    return widths_.get(node);
}

auto Layout::get_height(NodeId node) const -> float {
    return heights_.get(node);
}

auto Layout::get_waypoints(EdgeId edge) const -> const std::vector<Point>& {
    return waypoints_.get(edge);
}

auto Layout::get_region_node(const SESE::SESERegion& r) const -> const Node* {
    return regions_data_[r.id].node_ptr;
}

auto Layout::get_editor(const SESE::SESERegion& r) -> SubGraphEditor& {
    return regions_data_[r.id].subgraph->editor();
}

// NOLINTNEXTLINE(misc-no-recursion)
void Layout::compute_layout(const SESE::SESERegion& r) {
    auto& region = regions_data_[r.id];
    if (region.was_layout) {
        return;
    }

    region.was_layout = true;

    for (const auto* child_region : r.children()) {
        const auto& node = get_region_node(*child_region);

        compute_layout(*child_region);
        widths_[node]  = regions_data_[child_region->id].width;
        heights_[node] = regions_data_[child_region->id].height;
    }

    auto sugiyama =
        SugiyamaAnalysis(*region.subgraph, heights_, widths_, start_x_offset_,
                         end_x_offset_, region.entries, region.exits, settings);

    for (const auto* node : region.subgraph->nodes()) {
        auto x = sugiyama.xs_[node];
        auto y = sugiyama.ys_[node];

        xs_[node] = x;

        ys_[node] = y;
    }

    for (const auto* edge : region.subgraph->edges()) {
        waypoints_[edge] = sugiyama.waypoints_[edge];
    }

    region.height = sugiyama.get_graph_height();
    region.width  = sugiyama.get_graph_width();

    region.io_waypoints = sugiyama.get_io_waypoints();

    for (auto entry_pair : region.entries) {
        end_x_offset_[entry_pair.edge] =
            region.io_waypoints[entry_pair].front().x;
    }

    for (auto exit_pair : region.exits) {
        start_x_offset_[exit_pair.edge] =
            region.io_waypoints[exit_pair].back().x;
    }

    heights_[region.node_ptr] = region.height;
    widths_[region.node_ptr]  = region.width;

    if (sugiyama.has_top_loop_) {
        translate_region(r, {.x = 0, .y = -2 * settings.Y_GUTTER});
    }
}

void Layout::translate_region(const SESE::SESERegion& r, const Point& v) {
    auto& region = regions_data_[r.id];

    for (const auto* node : region.subgraph->nodes()) {
        auto node_x = Layout::get_x(*node);
        auto node_y = Layout::get_y(*node);

        xs_[node] = node_x + v.x;
        ys_[node] = node_y + v.y;
    }

    for (const auto* edge : region.subgraph->edges()) {
        auto& waypoints = waypoints_[edge];

        for (auto& waypoint : waypoints) {
            waypoint += v;
        }
    }

    for (auto& kv : region.io_waypoints) {
        for (auto& waypoint : kv.second) {
            waypoint += v;
        }
    }
}

// NOLINTNEXTLINE (misc-no-recursion)
void Layout::translate_region(const SESE::SESERegion& r) {
    if (!r.is_root()) {
        const auto& node = get_region_node(r);

        auto v = Point{.x = Layout::get_x(*node), .y = Layout::get_y(*node)};
        translate_region(r, v);
    }

    // Propagate to children
    for (const auto* child_region : r.children()) {
        translate_region(*child_region);
    }
}

Layout::RegionData::RegionData(Graph& g)
    : subgraph{std::make_unique<SubGraph>(g)}, node_ptr{nullptr} {}
