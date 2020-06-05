#pragma once

#include "geometrycentral/surface/halfedge_element_types.h"
#include "geometrycentral/utilities/mesh_data.h"
#include "geometrycentral/utilities/utilities.h"

#include <list>
#include <memory>
#include <vector>

// NOTE: ipp includes at bottom of file

namespace geometrycentral {
namespace surface {

// ==========================================================
// ===================    Containers    =====================
// ==========================================================

template <typename T>
using VertexData = MeshData<Vertex, T>;
template <typename T>
using FaceData = MeshData<Face, T>;
template <typename T>
using EdgeData = MeshData<Edge, T>;
template <typename T>
using HalfedgeData = MeshData<Halfedge, T>;
template <typename T>
using CornerData = MeshData<Corner, T>;
template <typename T>
using BoundaryLoopData = MeshData<BoundaryLoop, T>;


// ==========================================================
// ===================    Halfedge Mesh   ===================
// ==========================================================

class HalfedgeMesh {

public:
  HalfedgeMesh();

  // Build a halfedge mesh from polygons, with a list of 0-indexed vertices incident on each face, in CCW order.
  // Assumes that the vertex listing in polygons is dense; all indices from [0,MAX_IND) must appear in some face.
  // (some functions, like in meshio.h preprocess inputs to strip out unused indices).
  // The output will preserve the ordering of vertices and faces.
  HalfedgeMesh(const std::vector<std::vector<size_t>>& polygons);

  // Build a halfedge mesh from connectivity information (0-indexed as always)
  // - `polygons` is the usual vertex indices for each face
  // - `twins` is indices for the halfedge twin pointers. For each halfedge, holds the index of the twin face and
  // halfedge within that face. In each face, the 0'th halfedge goes from vert 0-->1. Use INVALID_IND for boundary.
  HalfedgeMesh(const std::vector<std::vector<size_t>>& polygons,
               const std::vector<std::vector<std::tuple<size_t, size_t>>>& twins, bool allowVertexNonmanifold = false);

  ~HalfedgeMesh();


  // Number of mesh elements of each type
  size_t nHalfedges() const;
  size_t nInteriorHalfedges() const;
  size_t nCorners() const;
  size_t nVertices() const;
  size_t nInteriorVertices(); // warning: O(n)
  size_t nEdges() const;
  size_t nFaces() const;
  size_t nBoundaryLoops() const;
  size_t nExteriorHalfedges() const;

  // Methods for range-based for loops
  // Example: for(Vertex v : mesh.vertices()) { ... }
  HalfedgeSet halfedges();
  HalfedgeInteriorSet interiorHalfedges();
  HalfedgeExteriorSet exteriorHalfedges();
  CornerSet corners();
  VertexSet vertices();
  EdgeSet edges();
  FaceSet faces();
  BoundaryLoopSet boundaryLoops();

  // Methods for accessing elements by index
  // only valid when the  mesh is compressed
  Halfedge halfedge(size_t index);
  Corner corner(size_t index);
  Vertex vertex(size_t index);
  Edge edge(size_t index);
  Face face(size_t index);
  BoundaryLoop boundaryLoop(size_t index);

  // === Methods that mutate the mesh.
  // Note that all of these methods retain the validity of any MeshData<> containers, as well as any element handles
  // (like Vertex). The only thing that can happen is that deletions may make the mesh no longer be _compressed_ (i.e.
  // the index space may have gaps in it). This can be remedied by calling compress(), which _does_ invalidate
  // non-dynamic element handles (but still keeps MeshData<> containers valid).

  // Flip an edge. Unlike all the other mutation routines, this _does not_ invalidate pointers. Edge is rotated
  // clockwise. Return true if the edge was actually flipped (can't flip boundary or non-triangular edges).
  bool flip(Edge e);

  // Adds a vertex along an edge, increasing degree of faces. Returns ptr along the edge, with he.vertex() as new
  // vertex and he.edge().halfedge() == he. Preserves canonical direction of edge.halfedge() for both halves of new
  // edge. The original edge is repurposed as one of the two new edges (same for original halfedges).
  Halfedge insertVertexAlongEdge(Edge e);

  // Split an edge, also splitting adjacent faces. Returns new halfedge which points away from the new vertex (so
  // he.vertex() is new vertex), and is the same direction as e.halfedge() on the original edge. The halfedge
  // direction of the other part of the new split edge is also preserved.
  Halfedge splitEdgeTriangular(Edge e);

  // Add vertex inside face and triangulate. Returns new vertex.
  Vertex insertVertex(Face f);

  // The workhorse version of connectVertices(). heA.vertex() will be connected to heB.vertex().
  // Returns new halfedge with vA at tail. he.twin().face() is the new face.
  Halfedge connectVertices(Halfedge heA, Halfedge heB);

  // Given a non-boundary edge e, cuts the mesh such that there are two copies of e with a boundary between them.
  // As necessary, either creates a new boundary loop or merges adjacent boundary loops.
  // Returns returns the halfedges along the cut edge which exist where {e.halfedge(), e.halfedge().twin()} were (which
  // may or may not be new)
  std::tuple<Halfedge, Halfedge> separateEdge(Edge e);

  // Collapse an edge. Returns the vertex adjacent to that edge which still exists. Returns Vertex() if not
  // collapsible. Assumes triangular simplicial complex as input (at least in neighborhood of collapse).
  Vertex collapseEdge(Edge e);

  // Removes a vertex, leaving a high-degree face. If the input is a boundary vertex, preserves an edge along the
  // boundary. Return Face() if impossible.
  Face removeVertex(Vertex v);

  // Make the edge a mirror image of itself, switching the side the two halfedges are on.
  Halfedge switchHalfedgeSides(Edge e);

  // Removes an edge, unioning two faces. Input must not be a boundary edge. Returns Face() if impossible.
  Face removeEdge(Edge e);

  // Remove a face along the boundary. Currently does not know how to remove ears or whole components.
  bool removeFaceAlongBoundary(Face f);

  // Triangulate in a face, returns all subfaces
  std::vector<Face> triangulate(Face face);


  // Methods for obtaining canonical indices for mesh elements
  // When the mesh is compressed, these will be equivalent to `vertex.getIndex()`, etc. However, even when the mesh is
  // not compressed they will still provide a dense enumeration. Of course in some situations, custom indices might
  // instead be needed, this is just a default dense enumeration.
  VertexData<size_t> getVertexIndices();
  VertexData<size_t> getInteriorVertexIndices();
  HalfedgeData<size_t> getHalfedgeIndices();
  CornerData<size_t> getCornerIndices();
  EdgeData<size_t> getEdgeIndices();
  FaceData<size_t> getFaceIndices();
  BoundaryLoopData<size_t> getBoundaryLoopIndices();

  // == Utility functions
  bool hasBoundary() const;        // does the mesh have boundary? (aka not closed)
  bool isTriangular();             // returns true if and only if all faces are triangles [O(n)]
  int eulerCharacteristic() const; // compute the Euler characteristic [O(1)]
  int genus() const;               // compute the genus [O(1)]
  size_t nConnectedComponents();   // compute number of connected components [O(n)]
  void printStatistics() const;    // print info about element counts to std::cout

  std::vector<std::vector<size_t>> getFaceVertexList();
  std::unique_ptr<HalfedgeMesh> copy() const;

  // Compress the mesh
  bool isCompressed() const;
  void compress();

  // == Callbacks that will be invoked on mutation to keep containers/iterators/etc valid.

  // Expansion callbacks
  // Argument is the new size of the element list. Elements up to this index may now be used (but _might_ not be
  // immediately).
  std::list<std::function<void(size_t)>> vertexExpandCallbackList;
  std::list<std::function<void(size_t)>> faceExpandCallbackList;
  std::list<std::function<void(size_t)>> edgeExpandCallbackList;
  std::list<std::function<void(size_t)>> halfedgeExpandCallbackList;

  // Compression callbacks
  // Argument is a permutation to a apply, such that d_new[p[i]] = d_old[i]. The size of the list is the new capacity
  // for the buffer (as in ___ExpandCallbackList), which may or may not be the same size as the current capacity. Any
  // elements with p[i] == INVALID_IND are unused in the new index space.
  std::list<std::function<void(const std::vector<size_t>&)>> vertexPermuteCallbackList;
  std::list<std::function<void(const std::vector<size_t>&)>> facePermuteCallbackList;
  std::list<std::function<void(const std::vector<size_t>&)>> edgePermuteCallbackList;
  std::list<std::function<void(const std::vector<size_t>&)>> halfedgePermuteCallbackList;

  // Mesh delete callbacks
  // (this unfortunately seems to be necessary; objects which have registered their callbacks above
  // need to know not to try to de-register them if the mesh has been deleted)
  std::list<std::function<void()>> meshDeleteCallbackList;

  // Check capacity. Needed when implementing expandable containers for mutable meshes to ensure the contain can
  // hold a sufficient number of elements before the next resize event.
  size_t nHalfedgesCapacity() const;
  size_t nVerticesCapacity() const;
  size_t nEdgesCapacity() const;
  size_t nFacesCapacity() const;
  size_t nBoundaryLoopsCapacity() const;


  // == Debugging, etc

  // Performs a sanity checks on halfedge structure; throws on fail
  void validateConnectivity();


protected:
  // = Core arrays which hold the connectivity
  // Note: it should always be true that heFace.size() == nHalfedgesCapacityCount, but any elements after
  // nHalfedgesFillCount will be valid indices (in the std::vector sense), but contain uninitialized data. Similarly,
  // any std::vector<> indices corresponding to deleted elements will hold meaningless values.
  std::vector<size_t> heNextArr;    // he.next()
  std::vector<size_t> heVertexArr;  // he.vertex()
  std::vector<size_t> heFaceArr;    // he.face()
  std::vector<size_t> vHalfedgeArr; // v.halfedge()
  std::vector<size_t> fHalfedgeArr; // f.halfedge()
  // (note: three more of these below for when not using implicit twin)

  // Does this mesh use the implicit-twin convention in its connectivity arrays?
  //
  // If true, heSibling, heEdge, and eHalfedge are all empty arrays, and these values are computed implicitly from
  // arithmetic on the indices. A consequence is that the mesh can only represent edge-manifold, oriented surfaces.
  //
  // If false, the above arrays are all populated and used for connectivty. As a consequence, the resulting mesh might
  // not be manifold/oriented, and extra care is needed for some routines.
  bool usesImplictTwin() const;
  bool useImplicitTwinFlag = false;

  // (see note above about implicit twin)
  std::vector<size_t> heSiblingArr; // he.sibling() and he.twin()
  std::vector<size_t> heEdgeArr;    // he.edge()
  std::vector<size_t> eHalfedgeArr; // e.halfedge()

  // Element connectivity
  size_t heNext(size_t iHe) const;    // he.vertex()
  size_t heTwin(size_t iHe) const;    // he.twin()
  size_t heSibling(size_t iHe) const; // he.sibling()
  size_t heEdge(size_t iHe) const;    // he.edge()
  size_t heVertex(size_t iHe) const;  // he.vertex()
  size_t heFace(size_t iHe) const;    // he.face()
  size_t eHalfedge(size_t iE) const;  // e.halfedge()
  size_t vHalfedge(size_t iV) const;  // v.halfedge()
  size_t fHalfedge(size_t iF) const;  // f.halfedge()

  // Implicit connectivity relationships
  static size_t heTwinImplicit(size_t iHe);   // he.twin()
  static size_t heEdgeImplicit(size_t iHe);   // he.edge()
  static size_t eHalfedgeImplicit(size_t iE); // e.halfedge()

  // Other implicit relationships
  bool heIsInterior(size_t iHe) const;
  bool faceIsBoundaryLoop(size_t iF) const;
  size_t faceIndToBoundaryLoopInd(size_t iF) const;
  size_t boundaryLoopIndToFaceInd(size_t iF) const;

  // Auxilliary arrays which cache other useful information

  // Track element counts (can't rely on rawVertices.size() after deletions have made the list sparse). These are the
  // actual number of valid elements, not the size of the buffer that holds them.
  size_t nHalfedgesCount = 0;
  size_t nInteriorHalfedgesCount = 0;
  size_t nEdgesCount = 0;
  size_t nVerticesCount = 0;
  size_t nFacesCount = 0;
  size_t nBoundaryLoopsCount = 0;

  // == Track the capacity and fill size of our buffers.
  // These give the capacity of the currently allocated buffer.
  // Note that this is _not_ defined to be std::vector::capacity(), it's the largest size such that arr[i] is legal (aka
  // arr.size()).
  size_t nVerticesCapacityCount = 0;
  size_t nHalfedgesCapacityCount = 0; // will always be even if implicit twin
  size_t nEdgesCapacityCount = 0;     // will always be even if implicit twin
  size_t nFacesCapacityCount = 0;     // capacity for faces _and_ boundary loops

  // These give the number of filled elements in the currently allocated buffer. This will also be the maximal index of
  // any element (except the weirdness of boundary loop faces). As elements get marked dead, nVerticesCount decreases
  // but nVertexFillCount does not (etc), so it denotes the end of the region in the buffer where elements have been
  // stored.
  size_t nVerticesFillCount = 0;
  size_t nHalfedgesFillCount = 0; // must always be even if implicit twin
  size_t nEdgesFillCount = 0;
  size_t nFacesFillCount = 0;         // where the real faces stop, and empty/boundary loops begin
  size_t nBoundaryLoopsFillCount = 0; // remember, these fill from the back of the face buffer

  // The mesh is _compressed_ if all of the index spaces are dense. E.g. if thare are |V| vertices, then the vertices
  // are densely indexed from 0 ... |V|-1 (and likewise for the other elements). The mesh can become not-compressed as
  // deletions mark elements with tombstones--this is how we support constant time deletion.
  // Call compress() to re-index and return to usual dense indexing.
  bool isCompressedFlag = true;

  uint64_t modificationTick = 1; // Increment every time the mesh is mutated in any way. Used to track staleness.

  // Hide copy and move constructors, we don't wanna mess with that
  HalfedgeMesh(const HalfedgeMesh& other) = delete;
  HalfedgeMesh& operator=(const HalfedgeMesh& other) = delete;
  HalfedgeMesh(HalfedgeMesh&& other) = delete;
  HalfedgeMesh& operator=(HalfedgeMesh&& other) = delete;

  // Used to resize the halfedge mesh. Expands and shifts vectors as necessary.
  Vertex getNewVertex();
  Halfedge getNewEdgeTriple(bool onBoundary); // Create an edge and two halfedges.
                                              // Returns e.halfedge() from the newly created edge
  Halfedge getNewHalfedge(bool isInterior);
  Edge getNewEdge();
  Face getNewFace();
  BoundaryLoop getNewBoundaryLoop();
  void expandFaceStorage(); // helper used in getNewFace() and getNewBoundaryLoop()

  // Detect dead elements
  bool vertexIsDead(size_t iV) const;
  bool halfedgeIsDead(size_t iHe) const;
  bool edgeIsDead(size_t iE) const;
  bool faceIsDead(size_t iF) const;

  // Deletes leave tombstones, which can be cleaned up with compress().
  // Note that these routines merely mark the element as dead. The caller should hook up connectivity to exclude these
  // elements before invoking.
  void deleteEdgeTriple(Halfedge he);
  void deleteElement(Vertex v);
  void deleteElement(Face f);
  void deleteElement(BoundaryLoop bl);

  // Construction subroutines
  void constructHalfedgeMeshManifold(const std::vector<std::vector<size_t>>& polygons);
  void constructHalfedgeMeshNonmanifold(const std::vector<std::vector<size_t>>& polygons);

  // Compression helpers
  void compressHalfedges();
  void compressEdges();
  void compressFaces();
  void compressVertices();

  // Helpers for mutation methods
  void ensureVertexHasBoundaryHalfedge(Vertex v); // impose invariant that v.halfedge is start of half-disk
  bool ensureEdgeHasInteriorHalfedge(Edge e);     // impose invariant that e.halfedge is interior
  Vertex collapseEdgeAlongBoundary(Edge e);

  // Iterating around the neighbors of a nonmanifold vertex is one of the few operations not efficiently supported by
  // even this souped-up halfedge mesh. For nonmanifold meshes, these helper arrays allow us to iterate efficiently.
  // For vertex iV, vertexIterationCacheHeIndex holds the indices of the halfedges outgoing from the vertex, in the
  // range from vertexIterationCacheVertexStart[iV] to vertexIterationCacheVertexStart[iV+1]
  void ensureVertexIterationCachePopulated() const;
  void populateVertexIterationCache() const;
  uint64_t vertexIterationCacheTick = 0; // used to keep cache fresh
  std::vector<size_t> vertexIterationCacheHeIndex;
  std::vector<size_t> vertexIterationCacheVertexStart;

  // Elements need direct access in to members to traverse
  friend class Vertex;
  friend class Halfedge;
  friend class Corner;
  friend class Edge;
  friend class Face;
  friend class BoundaryLoop;

  friend struct VertexRangeF;
  friend struct HalfedgeRangeF;
  friend struct HalfedgeInteriorRangeF;
  friend struct HalfedgeExteriorRangeF;
  friend struct CornerRangeF;
  friend struct EdgeRangeF;
  friend struct FaceRangeF;
  friend struct BoundaryLoopRangeF;
  friend struct VertexNeighborIteratorState;
};

} // namespace surface
} // namespace geometrycentral

// clang-format off
// preserve ordering
#include "geometrycentral/surface/halfedge_logic_templates.ipp"
#include "geometrycentral/surface/halfedge_element_types.ipp"
#include "geometrycentral/surface/halfedge_mesh.ipp"
// clang-format on
