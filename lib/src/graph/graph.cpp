#include "triskel/graph/graph.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <stack>
#include <unordered_map>
#include <utility>
#include <variant>

#include <fmt/base.h>
#include <fmt/format.h>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/generator.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

// =============================================================================
// GraphEditor
// =============================================================================
GraphEditor::GraphEditor(Graph& g) : g_{g} {}

GraphEditor::~GraphEditor() {
    assert(frames.empty());
}

auto GraphEditor::make_node() -> Node* {
    auto id = NodeId{next_node_id_};
    ++next_node_id_;

    g_.data_.nodes[id] = std::make_unique<Node>(id);

    // Sets the root if it is not defined
    if (g_.data_.root == NodeId::InvalidID) {
        g_.data_.root = id;
    }

    g_.invalid_node_cache = true;
    frame().changes.push(Frame::MakeNode(id));
    return g_.get_node(id);
}

void GraphEditor::remove_node(NodeId id) {
    const auto* node = g_.get_node(id);

    fmt::print("Removing node: {}\n", *node);

    // Not handled
    assert(node != g_.root());

    auto edges = span_to_vec(node->edges());
    for (const auto* edge : edges) {
        fmt::print("Removing edge: {}\n", *edge);
        remove_edge(*edge);
    }

    auto n = std::move(g_.data_.nodes[id]);
    g_.data_.nodes.erase(id);
    frame().changes.push(Frame::RemoveNode(std::move(n)));

    g_.invalid_node_cache = true;
    g_.invalid_edge_cache = true;
}

auto GraphEditor::make_edge(EdgeId id, NodeId from, NodeId to) -> Edge* {
    g_.data_.edges[id] = std::make_unique<Edge>(id);

    auto* edge = g_.get_edge(id);

    edge->from_ = g_.data_.nodes[from].get();
    edge->to_   = g_.data_.nodes[to].get();

    edge->link();

    g_.invalid_edge_cache = true;

    return edge;
}

auto GraphEditor::make_edge(NodeId from, NodeId to) -> Edge* {
    auto id = EdgeId{next_edge_id_};
    ++next_edge_id_;

    auto* edge = make_edge(id, from, to);
    frame().changes.push(Frame::AddEdge(id));
    return edge;
}

void GraphEditor::remove_edge(EdgeId id) {
    auto edge = std::move(g_.data_.edges[id]);
    edge->unlink();

    frame().changes.push(Frame::RemoveEdge(std::move(edge)));
    g_.data_.edges.erase(id);

    g_.invalid_edge_cache = true;
}

void GraphEditor::edit_edge(EdgeId id, NodeId new_from, NodeId new_to) {
    auto& edge = *g_.data_.edges[id];
    // Save the old edge
    frame().changes.push(Frame::ModifyEdge(edge, edge.from_, edge.to_));
    edge.unlink();

    edge.from_ = g_.data_.nodes.at(new_from).get();
    edge.to_   = g_.data_.nodes.at(new_to).get();
    edge.link();
}

auto GraphEditor::frame() -> Frame& {
    assert(!frames.empty() &&
           "Using the graph editor without a frame. You need to call `push` "
           "before using the editor");
    return frames.top();
}

void GraphEditor::push() {
    frames.emplace();
}

void GraphEditor::pop() {
    auto& f = frames.top();

    while (!f.changes.empty()) {
        auto& change = f.changes.top();
        std::visit([this](auto& change) { change.revert(this->g_); }, change);
        f.changes.pop();
    }

    frames.pop();
}

void GraphEditor::commit() {
    // Deletes all changes
    frames = std::stack<Frame>();
}

// =============================================================================
// Graph
// =============================================================================

Graph::Graph() : editor_{*this} {}

auto Graph::max_node_id() const -> size_t {
    return editor_.next_node_id_;
}

auto Graph::max_edge_id() const -> size_t {
    return editor_.next_edge_id_;
}
auto Graph::editor() -> IGraphEditor& {
    return editor_;
}