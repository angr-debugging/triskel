#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <span>

namespace triskel {

template <typename Tag>
struct ID {
    ID() : value(InvalidID.value) {}

    explicit ID(size_t value) : value{value} {}
    explicit operator size_t() const { return value; }

    static const ID InvalidID;

    auto operator==(ID other) const -> bool { return other.value == value; }

    friend auto operator<=>(ID lhs, ID rhs) -> std::strong_ordering {
        return lhs.value <=> rhs.value;
    }

    [[nodiscard]] auto is_valid() { return *this != InvalidID; }
    [[nodiscard]] auto is_invalid() { return *this == InvalidID; }

   private:
    size_t value;
};

template <typename Tag>
const ID<Tag> ID<Tag>::InvalidID{static_cast<size_t>(-1)};

template <typename Tag>
auto format_as(const ID<Tag>& id) -> std::string {
    return std::to_string(static_cast<size_t>(id));
}

}  // namespace triskel

template <typename Tag>
struct std::hash<triskel::ID<Tag>> {
    auto operator()(triskel::ID<Tag> k) const -> std::size_t {
        return std::hash<size_t>{}(static_cast<size_t>(k));
    }
};

namespace triskel {

/// @brief A struct with and id
template <typename Tag>
struct Identifiable {
    virtual ~Identifiable() = default;

    [[nodiscard]] virtual auto id() const -> ID<Tag> = 0;

    auto operator==(const Identifiable& other) const -> bool {
        return id() == other.id();
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator ID<Tag>() const { return id(); }
};

struct NodeTag {};
using NodeId = ID<NodeTag>;
static_assert(std::is_trivially_copyable_v<NodeId>);
static_assert(std::strict_weak_order<std::ranges::less, NodeId, NodeId>);

struct EdgeTag {};
using EdgeId = ID<EdgeTag>;
static_assert(std::is_trivially_copyable_v<EdgeId>);

struct Edge;
struct Node;
struct IGraph;
struct IGraphEditor;

template <typename T>
using Container = std::span<T>;

template <typename K, typename V>
using Map = std::unordered_map<K, V>;

using NodeMap = Map<NodeId, std::unique_ptr<Node>>;
using EdgeMap = Map<EdgeId, std::unique_ptr<Edge>>;

struct GraphData {
    NodeId root = NodeId::InvalidID;

    NodeMap nodes;
    EdgeMap edges;
};

struct Node : public Identifiable<NodeTag> {
    explicit Node(NodeId id) : id_{id} {}

    Node(const Node&)                    = delete;
    auto operator=(const Node&) -> Node& = delete;

    ~Node() override = default;

    [[nodiscard]] auto id() const -> NodeId final;

    [[nodiscard]] auto children_count() const -> size_t;

    [[nodiscard]] auto parent_count() const -> size_t;

    [[nodiscard]] auto neighbor_count() const -> size_t;

    [[nodiscard]] auto edges() const -> Container<Edge*>;

    [[nodiscard]] auto child_edges() const -> Container<Edge*>;

    [[nodiscard]] auto parent_edges() const -> Container<Edge*>;

    NodeId id_;

    // To return spans
    mutable std::vector<Edge*> edges_;

    // The first half are parent edges, the second half are child edges
    size_t separator_ = 0;
};

struct Edge : public Identifiable<EdgeTag> {
    explicit Edge(EdgeId id) : id_{id} {};
    ~Edge() override = default;

    Edge(const Edge&)                    = delete;
    auto operator=(const Edge&) -> Edge& = delete;

    [[nodiscard]] auto id() const -> EdgeId final;

    /// @brief Returns the other side of the edge
    [[nodiscard]] auto other(NodeId n) const -> Node*;

    EdgeId id_;

    Node* from;
    Node* to;

    /// @brief Adds the edge to each of its nodes
    void link();

    /// @brief Removes the edge from each of its nodes
    void unlink();
};

/// @brief An interface for a graph
struct IGraph {
    virtual ~IGraph() = default;

    /// @brief The root of this graph
    [[nodiscard]] virtual auto root() const -> Node* = 0;

    /// @brief The nodes in this graph
    [[nodiscard]] virtual auto nodes() const -> Container<Node*> = 0;

    /// @brief The edges in this graph
    [[nodiscard]] virtual auto edges() const -> Container<Edge*> = 0;

    /// @brief Turns a NodeId into a Node
    [[nodiscard]] virtual auto get_node(NodeId id) const -> Node* = 0;

    /// @brief Turns an EdgeId into an Edge
    [[nodiscard]] virtual auto get_edge(EdgeId id) const -> Edge* = 0;

    /// @brief The greatest id in this graph
    [[nodiscard]] virtual auto max_node_id() const -> size_t = 0;

    /// @brief The greatest id in this graph
    [[nodiscard]] virtual auto max_edge_id() const -> size_t = 0;

    /// @brief The number of nodes in this graph
    [[nodiscard]] virtual auto node_count() const -> size_t = 0;

    /// @brief The number of edges in this graph
    [[nodiscard]] virtual auto edge_count() const -> size_t = 0;

    /// @brief Gets the editor attached to this graph
    [[nodiscard]] virtual auto editor() -> IGraphEditor& = 0;

    /// @brief Does this subgraph have this node
    [[nodiscard]] virtual auto contains(NodeId node) -> bool = 0;

    /// @brief Does this subgraph have this edge
    [[nodiscard]] virtual auto contains(EdgeId edge) -> bool = 0;
};

auto format_as(const Node& n) -> std::string;
auto format_as(const Edge& e) -> std::string;
auto format_as(const IGraph& g) -> std::string;
struct IGraphEditor {
    virtual ~IGraphEditor() = default;

    // ==========
    // Nodes
    // ==========
    /// @brief Adds a node to the graph
    virtual auto make_node() -> Node* = 0;

    /// @brief Removes a node from the graph
    /// This will also remove all the edges that contain this node
    virtual void remove_node(NodeId node) = 0;

    // ==========
    // Edges
    // ==========
    /// @brief Create a new edge between two nodes
    virtual auto make_edge(NodeId from, NodeId to) -> Edge* = 0;

    /// @brief Modifies the start and end point of an edge
    virtual void edit_edge(EdgeId edge, NodeId new_from, NodeId new_to) = 0;

    /// @brief Removes an edge from the graph
    virtual void remove_edge(EdgeId edge) = 0;

    // ==========
    // Version Control
    // ==========
    /// @brief Creates a new edit frame
    virtual void push() = 0;

    /// @brief Removes all changes from the current frame
    virtual void pop() = 0;

    /// @brief Writes all changes to the graph, erasing the modification
    /// frames
    virtual void commit() = 0;
};

}  // namespace triskel