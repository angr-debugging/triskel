#include "triskel/graph/subgraph.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <stack>
#include <utility>
#include <variant>

#include <fmt/core.h>
#include <fmt/printf.h>

#include "triskel/graph/frame.hpp"
#include "triskel/graph/igraph.hpp"
#include "triskel/utils/generator.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

// =============================================================================
// Subgraph Editor
// =============================================================================
SubGraphEditor::SubGraphEditor(SubGraph& sg)
    : sg_{sg}, editor_{sg.g_.editor()} {}

void SubGraphEditor::select_node(NodeId id, bool use_frame) {
    if (!sg_.contains(id)) {
        sg_.data_.nodes[id] = std::make_unique<Node>(id);

        if (use_frame) {
            frame().changes.push(Frame::MakeNode(id));
        }
    }

    select_edges(id, use_frame);

    if (sg_.data_.root == NodeId::InvalidID) {
        make_root(id);
    }

    sg_.invalid_node_cache = true;
}

void SubGraphEditor::unselect_node(NodeId id, bool use_frame) {
    if (sg_.contains(id)) {
        unselect_edges(id, use_frame);

        auto n = std::move(sg_.data_.nodes[id]);
        sg_.data_.nodes.erase(id);

        if (use_frame) {
            frame().changes.push(Frame::RemoveNode(std::move(n)));
        }

        sg_.invalid_node_cache = true;
    }
}

void SubGraphEditor::select_edge(const Edge* g_edge, bool use_frame) {
    sg_.data_.edges[*g_edge] = std::make_unique<Edge>(g_edge->id());

    auto& sg_edge = sg_.data_.edges[*g_edge];

    sg_edge->to   = sg_.data_.nodes[*g_edge->to].get();
    sg_edge->from = sg_.data_.nodes[*g_edge->from].get();

    sg_edge->link();

    if (use_frame) {
        frame().changes.push(Frame::AddEdge(*sg_edge));
    }

    sg_.invalid_edge_cache = true;
}

void SubGraphEditor::select_edges(NodeId node, bool use_frame) {
    // The node in the complete graph
    auto* n = sg_.g_.get_node(node);

    for (const auto* edge : n->edges()) {
        if (sg_.contains(*edge->other(node)) && !sg_.contains(*edge)) {
            select_edge(edge, use_frame);
        }
    }

    sg_.invalid_edge_cache = true;
}

void SubGraphEditor::unselect_edges(NodeId node, bool use_frame) {
    // The node in the complete graph
    auto* n = sg_.g_.get_node(node);

    // Copy the vector as we are modifying it
    const auto edges = span_to_vec(n->edges());
    for (auto* edge : edges) {
        if (sg_.contains(*edge->other(node))) {
            auto e = std::move(sg_.data_.edges[*edge]);
            e->unlink();
            if (use_frame) {
                frame().changes.push(Frame::RemoveEdge(std::move(e)));
            }
            sg_.data_.edges.erase(*edge);
        }
    }

    sg_.invalid_edge_cache = true;
}

void SubGraphEditor::make_root(NodeId node) {
    assert(sg_.contains(node));
    sg_.data_.root = node;
}

auto SubGraphEditor::make_node() -> Node* {
    auto* node = editor_.make_node();
    select_node(*node, true);

    // We want the object in the subgraph
    return sg_.get_node(*node);
}

void SubGraphEditor::remove_node(NodeId node) {
    assert(sg_.contains(node));
    unselect_node(node, true);
    editor_.remove_node(node);
}

auto SubGraphEditor::make_edge(NodeId from, NodeId to) -> Edge* {
    assert(sg_.contains(from) && sg_.contains(to));
    auto* edge = editor_.make_edge(from, to);
    select_edge(edge, true);

    // We want the object in the subgraph
    return sg_.get_edge(*edge);
}

void SubGraphEditor::edit_edge(EdgeId edge, NodeId new_from, NodeId new_to) {
    assert(sg_.contains(new_from));
    assert(sg_.contains(new_to));

    Edge* sg_edge;
    if (sg_.contains(edge)) {
        sg_edge = sg_.data_.edges[edge].get();
        frame().changes.push(
            Frame::ModifyEdge(*sg_edge, sg_edge->from, sg_edge->to));
        sg_edge->unlink();
    } else {
        sg_.data_.edges[edge] = std::make_unique<Edge>(edge);
        sg_edge               = sg_.data_.edges[edge].get();
        frame().changes.push(Frame::AddEdge(edge));
    }

    editor_.edit_edge(edge, new_from, new_to);

    sg_edge->to   = sg_.data_.nodes[new_to].get();
    sg_edge->from = sg_.data_.nodes[new_from].get();

    sg_edge->link();
}

void SubGraphEditor::remove_edge(EdgeId edge) {
    assert(sg_.contains(edge));

    auto e = std::move(sg_.data_.edges[edge]);
    e->unlink();
    frame().changes.push(Frame::RemoveEdge(std::move(e)));
    sg_.data_.edges.erase(edge);

    editor_.remove_edge(edge);
    sg_.invalid_edge_cache = true;
}

auto SubGraphEditor::frame() -> Frame& {
    assert(!frames.empty() &&
           "Using the graph editor without a frame. You need to call `push` "
           "before using the editor");
    return frames.top();
}

void SubGraphEditor::push() {
    editor_.push();
    frames.emplace();
}

void SubGraphEditor::pop() {
    auto& f = frames.top();

    while (!f.changes.empty()) {
        auto& change = f.changes.top();
        std::visit([this](auto& change) { change.revert(this->sg_); }, change);
        f.changes.pop();
    }

    frames.pop();

    editor_.pop();
}

void SubGraphEditor::commit() {
    editor_.commit();

    frames = std::stack<Frame>{};
}

// =============================================================================
// Subgraph
// =============================================================================
SubGraph::SubGraph(IGraph& g) : g_{g}, editor_{*this} {}

auto SubGraph::max_node_id() const -> size_t {
    return g_.max_node_id();
}

auto SubGraph::max_edge_id() const -> size_t {
    return g_.max_edge_id();
}

auto SubGraph::editor() -> SubGraphEditor& {
    return editor_;
}