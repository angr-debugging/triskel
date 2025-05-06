from __future__ import annotations
import typing

__all__ = [
    "CFGLayout",
    "Default",
    "EdgeType",
    "ExportingRenderer",
    "F",
    "LayoutBuilder",
    "Point",
    "Renderer",
    "T",
    "git_version",
    "make_layout_builder",
    "make_png_renderer",
    "make_svg_renderer",
]

class CFGLayout:
    def get_coords(self, node: int) -> Point:
        """
        Gets the x and y coordinate of a node
        """

    def get_waypoints(self, edge: int) -> list[Point]:
        """
        Gets the waypoints of an edge
        """

    def get_height(self) -> float:
        """
        Gets height of the graph
        """

    def get_width(self) -> float:
        """
        Gets width of the graph
        """

    def get_node_height(self, node: int) -> float:
        """
        Gets height of the graph
        """

    def get_node_width(self, node: int) -> float:
        """
        Gets width of the graph
        """

    def save(self, renderer: ExportingRenderer, path: str) -> None:
        """
        Generate an image of the graph
        """

class EdgeType:
    """
    Members:

      Default

      T

      F
    """

    Default: typing.ClassVar[EdgeType]  # value = <EdgeType.Default: 0>
    F: typing.ClassVar[EdgeType]  # value = <EdgeType.F: 2>
    T: typing.ClassVar[EdgeType]  # value = <EdgeType.T: 1>
    __members__: typing.ClassVar[
        dict[str, EdgeType]
    ]  # value = {'Default': <EdgeType.Default: 0>, 'T': <EdgeType.T: 1>, 'F': <EdgeType.F: 2>}
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: int) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class ExportingRenderer(Renderer):
    pass

class LayoutBuilder:
    def build(self) -> CFGLayout:
        """
        Builds the layout
        """

    def graphviz(self) -> str:
        """
        Dot representation for debugging
        """

    @typing.overload
    def make_edge(self, origin: int, destination: int) -> int:
        """
        Creates an edge from `origin` to `destination`
        """

    @typing.overload
    def make_edge(self, origin: int, destination: int, type: EdgeType) -> int:
        """
        Creates an edge from `origin` to `destination` with type `type`
        (This is used to color the edge)
        """

    @typing.overload
    def make_node(self) -> int:
        """
        Creates a new node
        """

    @typing.overload
    def make_node(self, width: float, height: float) -> int:
        """
        Creates a new node with a width and height
        """

    @typing.overload
    def make_node(self, label: str) -> int:
        """
        Creates a new node with a label
        """

    @typing.overload
    def make_node(self, renderer: Renderer, label: str) -> int:
        """
        Creates a new node using a renderer to determine the size of labels
        """

    def measure_nodes(self, renderer: Renderer) -> None:
        """
        Calculates the dimension of each node using the renderer
        """

    def set_edge_height(self, edge_height: float) -> None:
        """
        Change settings for edge height
        """

    def set_padding(self, padding: float) -> None:
        """
        Change settings for padding
        """

    def set_x_gutter(self, x_gutter: float) -> None:
        """
        Change settings for X gutter
        """

    def set_y_gutter(self, y_gutter: float) -> None:
        """
        Change settings for Y gutter
        """

class Point:
    x: float
    y: float
    def __init__(self, x: float, y: float) -> None:
        """
        Creates a new point with given coordinates
        """

class Renderer:
    pass

def git_version() -> str:
    """
    Retrieve version information on this build
    """

def make_layout_builder() -> LayoutBuilder:
    """
    Creates a new layout builder
    """

def make_png_renderer() -> ExportingRenderer:
    """
    Creates a renderer for making PNG images
    """

def make_svg_renderer() -> ExportingRenderer:
    """
    Creates a renderer for making SVG images
    """

EdgeTypeDefault: EdgeType  # value = <EdgeType.Default: 0>
EdgeTypeFalse: EdgeType  # value = <EdgeType.F: 2>
EdgeTypeTrue: EdgeType  # value = <EdgeType.T: 1>
