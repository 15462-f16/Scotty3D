#ifndef CMU462_DYNAMICSCENE_MESH_H
#define CMU462_DYNAMICSCENE_MESH_H

#include "scene.h"

#include "../collada/polymesh_info.h"
#include "../halfEdgeMesh.h"
#include "../meshEdit.h"

#include <map>

namespace CMU462 { namespace DynamicScene {


class Mesh : public SceneObject {
 public:

  Mesh( Collada::PolymeshInfo& polyMesh,
        const Matrix4x4& transform );

  ~Mesh();

  void set_draw_styles(DrawStyle *defaultStyle, DrawStyle *hoveredStyle,
                       DrawStyle *selectedStyle);
  virtual void draw();

  BBox get_bbox();

  virtual Info getInfo();

  void _bevel_selection( double inset, double shift );

  virtual void drag( double x, double y, double dx, double dy, const Matrix4x4& modelViewProj );

  BSDF *get_bsdf();
  StaticScene::SceneObject *get_static_object();

  void collapse_selected_edge();
  void flip_selected_edge();
  void split_selected_edge();
  void erase_selected_element();
  void bevel_selected_element();
  void upsample();
  void downsample();
  void resample();
  void triangulate();

  HalfedgeMesh mesh;

  /**
   * Rather than drawing the object geometry for display, this method draws the
   * object with unique colors that can be used to determine which object was
   * selected or "picked" by the cursor.  The parameter pickID is the lowest
   * consecutive integer that has so far not been used by any other object as
   * a picking ID.  (Draw colors are then derived from these IDs.)  This data
   * will be used by Scene::update_selection to make the final determination
   * of which object (and possibly element within that object) was picked.
   */
  virtual void draw_pick( int& pickID );

  /** Assigns attributes of the selection based on the ID of the
   * object that was picked.  Can assume that pickID was one of
   * the IDs generated during this object's call to draw_pick().
   */
  virtual void setSelection( int pickID, Selection& selection );

 private:

  // Helpers for draw().
  void draw_faces() const;
  void draw_edges() const;
  void draw_feature_if_needed( Selection* s ) const;
  void draw_vertex(const Vertex *v) const;
  void draw_halfedge_arrow(const Halfedge *h) const;
  DrawStyle *get_draw_style(const HalfedgeElement *element) const;

  // a vector of halfedges whose vertices are newly created with bevel
  // on scroll, reposition vertices referenced from these halfedges
  vector<HalfedgeIter> bevelVertices;
  // original position of beveled vertex
  Vector3D beveledVertexPos;
  // original positions of beveled edge, corresponding to bevelVertices
  vector<Vector3D> beveledEdgePos;
  // original vertex positions for face currently being beveled
  vector<Vector3D> beveledFacePos;

  DrawStyle *defaultStyle, *hoveredStyle, *selectedStyle;

  MeshResampler resampler;
  
  // map from picking IDs to mesh elements, generated during draw_pick
  // and used by setSelection
  std::map<int,HalfedgeElement*> idToElement;

  // Assigns the next consecutive pickID to the given element, and
  // sets the GL color accordingly.  Also increments pickID.
  void newPickElement( int& pickID, HalfedgeElement* e );

  // material
  BSDF* bsdf;
};

} // namespace DynamicScene
} // namespace CMU462

#endif // CMU462_DYNAMICSCENE_MESH_H
