#include "triskel/graph/graph.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <stack>
#include <unordered_map>
#include <utility>
#include <variant>

#include "triskel/graph/igraph.hpp"
#include "triskel/utils/generator.hpp"

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

// =============================================================================
// Changes
// =============================================================================

void GraphEditor::Frame::MakeNode::revert(GraphEditor& editor) {
    editor.next_node_id_--;
    editor.g_.data_.nodes.erase(id);
}

void GraphEditor::Frame::RemoveNode::revert(GraphEditor& editor) {
    editor.g_.data_.nodes[node->id] = std::move(node);
}

void GraphEditor::Frame::AddEdge::revert(GraphEditor& editor) {
    editor.next_edge_id_--;
    auto& edge = editor.g_.data_.edges[id];
    edge->unlink();
    editor.g_.data_.edges.erase(id);
}

void GraphEditor::Frame::RemoveEdge::revert(GraphEditor& editor) {
    edge->link();
    editor.g_.data_.edges[edge->id] = std::move(edge);
}

void GraphEditor::Frame::ModifyEdge::revert(GraphEditor& editor) {
    auto& new_edge = editor.g_.data_.edges[edge.id];
    new_edge->unlink();

    *new_edge = edge;
    new_edge->link();
}

// =============================================================================
// GraphEditor
// =============================================================================
GraphEditor::GraphEditor(Graph& g) : g_{g} {}

GraphEditor::~GraphEditor() {
    assert(frames.empty());
}

auto GraphEditor::make_node() -> Node {
    auto id = NodeId{next_node_id_};
    ++next_node_id_;

    g_.data_.nodes[id] = std::make_unique<NodeData>(id);

    // Sets the root if it is not defined
    if (g_.data_.root == NodeId::InvalidID) {
        g_.data_.root = id;
    }

    frame().changes.push(Frame::MakeNode(id));
    return g_.get_node(id);
}

void GraphEditor::remove_node(NodeId id) {
    const auto& node = g_.get_node(id);

    // Not handled
    assert(node != g_.root());

    auto edges = gen_to_v(node.edges());
    for (const auto& edge : edges) {
        remove_edge(edge);
    }

    auto n = std::move(g_.data_.nodes[id]);
    g_.data_.nodes.erase(id);
    frame().changes.push(Frame::RemoveNode(std::move(n)));
}

auto GraphEditor::make_edge(EdgeId id, NodeId from, NodeId to) -> EdgeData& {
    g_.data_.edges[id] = std::make_unique<EdgeData>(id);

    auto& edge = *g_.data_.edges[id];

    edge.from = g_.data_.nodes[from].get();
    edge.to   = g_.data_.nodes[to].get();

    edge.link();

    return edge;
}

auto GraphEditor::make_edge(NodeId from, NodeId to) -> Edge {
    auto id = EdgeId{next_edge_id_};
    ++next_edge_id_;

    auto& edge = make_edge(id, from, to);
    frame().changes.push(Frame::AddEdge(id));
    return {edge};
}

void GraphEditor::remove_edge(EdgeId id) {
    auto edge = std::move(g_.data_.edges[id]);
    edge->unlink();

    frame().changes.push(Frame::RemoveEdge(std::move(edge)));
    g_.data_.edges.erase(id);
}

void GraphEditor::edit_edge(EdgeId id, NodeId new_from, NodeId new_to) {
    auto& edge = g_.data_.edges[id];
    edge->unlink();

    // Save the old edge
    frame().changes.push(Frame::ModifyEdge(*edge));

    edge->from = g_.data_.nodes.at(new_from).get();
    edge->to   = g_.data_.nodes.at(new_to).get();
    edge->link();
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
        std::visit([this](auto& change) { change.revert(*this); }, change);
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