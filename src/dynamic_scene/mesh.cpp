#include "mesh.h"
#include "widgets.h"

#include <cassert>
#include <sstream>

#include "../static_scene/object.h"

using std::ostringstream;

namespace CMU462 { namespace DynamicScene {

// For use in choose_hovered_subfeature.
static const double low_threshold  = .1;
static const double mid_threshold  = .2;
static const double high_threshold = 1.0 - low_threshold;

Mesh::Mesh( Collada::PolymeshInfo& polyMesh, const Matrix4x4& transform)
{
   // Build halfedge mesh from polygon soup
   vector< vector<size_t> > polygons;
   for (const Collada::Polygon& p : polyMesh.polygons) {
      polygons.push_back(p.vertex_indices);
   }
   vector<Vector3D> vertices = polyMesh.vertices; // DELIBERATE COPY.
   for (int i = 0; i < vertices.size(); i++) {
      vertices[i] = (transform * Vector4D(vertices[i], 1)).projectTo3D();
   }

   mesh.build(polygons, vertices);  
   if (polyMesh.material) {
      bsdf = polyMesh.material->bsdf;
   } else {
      bsdf = new DiffuseBSDF(Spectrum(0.5f,0.5f,0.5f));
   }
}

void Mesh::draw() {

   glDisable( GL_BLEND );

  // TODO: fix drawing with BSDF
  // DiffuseBSDF* diffuse = dynamic_cast<DiffuseBSDF*>(bsdf);
  // if (diffuse) {
  //   glBindTexture(GL_TEXTURE_2D, 0);
  //   glMaterialfv(GL_FRONT, GL_DIFFUSE, &diffuse->albedo.r);
  // }

	// Enable lighting for faces
  glEnable(GL_LIGHTING);
  draw_faces();

   glEnable( GL_BLEND );
   glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

  // Edges are drawn with flat shading.
  glDisable(GL_LIGHTING);
  draw_edges();

  // ONLY draw a vertex/half-edge if it's selected.
  if( scene )
  {
     draw_feature_if_needed( &scene->selected );
     draw_feature_if_needed( &scene->hovered );
  }
  glEnable(GL_LIGHTING);
  glDisable( GL_BLEND );
}

void Mesh::draw_faces() const {

   GLfloat white[4] = { 1., 1., 1., 1. };
   glEnable( GL_LIGHTING );
   glEnable( GL_LIGHT0 );
   glLightfv( GL_LIGHT0, GL_DIFFUSE, white );
   glMaterialfv( GL_FRONT_AND_BACK, GL_DIFFUSE, white );

  for (FaceCIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++) {

    // Prevent z fighting (faces bleeding into edges and points).
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);

    DrawStyle* style = get_draw_style(elementAddress(f));
    if (style != defaultStyle) {
      glDisable(GL_LIGHTING);
      style->style_face();
    }

    glBegin(GL_POLYGON);
    Vector3D normal(f->normal());
    glNormal3dv(&normal.x);
    HalfedgeCIter h = f->halfedge();
    do {
      glVertex3dv(&h->vertex()->position.x);
      h = h->next();
    } while (h != f->halfedge());
    glEnd();

    if (style != defaultStyle) {
      glEnable(GL_LIGHTING);
    }
  }
}

void Mesh::draw_edges() const
{

   // Draw seleted edge
   Edge* ep = scene->hovered.element->getEdge();
   if( ep )
   {
      EdgeIter e = ep->halfedge()->edge();

      DrawStyle *style = get_draw_style(elementAddress(e));
      style->style_edge();

      glBegin(GL_LINES);
      glVertex3dv(&e->halfedge()->vertex()->position.x);
      glVertex3dv(&e->halfedge()->twin()->vertex()->position.x);
      glEnd();
   }

   // Draw all edges
   defaultStyle->style_edge();
   glBegin(GL_LINES);
   for( EdgeCIter e = mesh.edgesBegin(); e != mesh.edgesEnd(); e++ )
   {
      glVertex3dv(&e->halfedge()->vertex()->position.x);
      glVertex3dv(&e->halfedge()->twin()->vertex()->position.x);
   }
   glEnd();

   defaultStyle->style_reset();
}

void Mesh::draw_feature_if_needed( Selection *s ) const
{

  if( s->element == NULL ) return;

  glDisable(GL_DEPTH_TEST);

  const Vertex *v = s->element->getVertex();
  if (v != nullptr) {
    draw_vertex( v );
  }
  const Halfedge *h = s->element->getHalfedge();
  if (h != nullptr) {
    draw_halfedge_arrow( h );
  }

  glEnable(GL_DEPTH_TEST);
}

void Mesh::draw_vertex(const Vertex *v) const {
  get_draw_style(v)->style_vertex();
  glBegin(GL_POINTS);
  glVertex3d(v->position.x, v->position.y, v->position.z);
  glEnd();
  get_draw_style(v)->style_reset();
}

void Mesh::draw_halfedge_arrow(const Halfedge *h) const {
  get_draw_style(h)->style_halfedge();

  const Vector3D& p0 = h->vertex()->position;
  const Vector3D& p1 = h->next()->vertex()->position;
  const Vector3D& p2 = h->next()->next()->vertex()->position;

  const Vector3D& e01 = p1-p0;
  const Vector3D& e12 = p2-p1;
  const Vector3D& e20 = p0-p2;

  const Vector3D& u = (e01 - e20) / 2;
  const Vector3D& v = (e12 - e01) / 2;

  const Vector3D& a = p0 + u / 5;
  const Vector3D& b = p1 + v / 5;

  const Vector3D& s = (b-a) / 5;
  const Vector3D& t = cross(h->face()->normal(), s);
  double theta = PI * 5 / 6;
  const Vector3D& c = b + cos(theta) * s + sin(theta) * t;

  glBegin(GL_LINE_STRIP);
  glVertex3dv(&a.x);
  glVertex3dv(&b.x);
  glVertex3dv(&c.x);
  glEnd();

  get_draw_style(h)->style_reset();
}

DrawStyle *Mesh::get_draw_style( const HalfedgeElement *element ) const
{
   if( scene && element == scene->selected.element )
   {
      return selectedStyle;
   }

   if( scene && element == scene->hovered.element )
   {
      return hoveredStyle;
   }

   return defaultStyle;
}

void Mesh::set_draw_styles(DrawStyle *defaultStyle, DrawStyle *hoveredStyle,
                           DrawStyle *selectedStyle) {
  this->defaultStyle = defaultStyle;
  this->hoveredStyle = hoveredStyle;
  this->selectedStyle = selectedStyle;
}

BBox Mesh::get_bbox() {
  BBox bbox;
  for (VertexIter it = mesh.verticesBegin(); it != mesh.verticesEnd(); it++) {
    bbox.expand(it->position);
  }
  return bbox;
}

Info Mesh::getInfo()
{
   Info info;

   if( !scene || !scene->selected.element )
   {
      info.push_back( "MESH" );
   }
   else
   {
      info = scene->selected.element->getInfo();
   }

   return info;
}

void Mesh::_bevel_selection( double inset, double shift ) {

   if( scene == nullptr ) return;
   HalfedgeElement *element = scene->selected.element;
   if (element == nullptr) return;
   Vertex *v = element->getVertex();
   Edge *e = element->getEdge();
   Face *f = element->getFace();

   if (v != nullptr) {
      mesh._bevel_vtx_reposition_with_dist(beveledVertexPos, bevelVertices, inset);
   } else if (e != nullptr) {
      mesh._bevel_edge_reposition_with_dist(beveledEdgePos, bevelVertices, inset);
   } else if (f != nullptr) {
      mesh._bevel_fc_reposition_with_dist(beveledFacePos, bevelVertices, shift, inset);
   } else {
      return;
   }

   scene->hovered.clear();
   scene->elementTransform->target.clear();
}

void Mesh::collapse_selected_element() {
   if( scene == nullptr ) return;
   HalfedgeElement *element = scene->selected.element;
   if (element == nullptr) return;
   Edge *edge = element->getEdge();
   Face *face = element->getFace();
   VertexIter v;

   if (edge != nullptr)
   {
      v = mesh.collapseEdge(edge->halfedge()->edge());
   }
   else if( face != nullptr )
   {
      v = mesh.collapseFace(face->halfedge()->face());
   }
   else
   {
      return;
   }

   scene->selected.element = elementAddress( v );
   scene->hovered.clear();
   scene->elementTransform->target.clear();
}

void Mesh::flip_selected_edge() {
   if( scene == nullptr ) return;
   HalfedgeElement *element = scene->selected.element;
   if (element == nullptr) return;
   Edge *edge = element->getEdge();
   if (edge == nullptr) return;
   EdgeIter e = mesh.flipEdge(edge->halfedge()->edge());
   scene->selected.element = elementAddress( e );
   scene->hovered.clear();
   scene->elementTransform->target.clear();
}

void Mesh::split_selected_edge() {
   if( scene == nullptr ) return;
   HalfedgeElement *element = scene->selected.element;
   if (element == nullptr) return;
   Edge *edge = element->getEdge();
   if (edge == nullptr) return;
   VertexIter v = mesh.splitEdge(edge->halfedge()->edge());
   scene->selected.element = elementAddress( v );
   scene->hovered.clear();
   scene->elementTransform->target.clear();
}

void Mesh::erase_selected_element() {
   if( scene == nullptr ) return;
   HalfedgeElement *element = scene->selected.element;
   if (element == nullptr) return;
   Vertex* vertex = element->getVertex();
   Edge* edge = element->getEdge();
   FaceIter f;
   if (edge != nullptr)
   {
      f = mesh.eraseEdge(edge->halfedge()->edge());
   }
   else if( vertex != nullptr )
   {
      f = mesh.eraseVertex(vertex->halfedge()->vertex());
   }
   else
   {
      return;
   }
   scene->selected.clear();
   scene->selected.object = this;
   scene->selected.element = elementAddress( f );
   scene->hovered.clear();
   scene->elementTransform->target.clear();
}


void Mesh::bevel_selected_element() {
   if( scene == nullptr ) return;
   HalfedgeElement *element = scene->selected.element;
   if (element == nullptr) return;
   Face   *f = element->getFace();
   Edge   *e = element->getEdge();
   Vertex *v = element->getVertex();
   FaceIter _newf;
   if (f != nullptr) {

      // Store the original vertex positions for the beveled face;
      // we will adjust the positions relative to these positions.
      beveledFacePos.clear();
      HalfedgeIter tmp = f->halfedge();
      do {
         beveledFacePos.push_back( tmp->vertex()->position );
         tmp = tmp->next();
      } while (tmp != f->halfedge());


      // HalfedgeIter h = f->halfedge();
      // Vector3D center = Vector3D(0., 0., 0.);
      // HalfedgeIter hprev = h;
      // do {
      //   center += h->vertex()->position;
      //   hprev = h;
      //   h = h->next();
      // } while (h != f->halfedge());
      // center /= deg;


      _newf = mesh.bevelFace(f->halfedge()->face());
      scene->selected.element = elementAddress(_newf);
      scene->hovered.clear();
      scene->elementTransform->target.clear();
   } else if (e != nullptr) {
      VertexIter v0 = e->halfedge()->vertex();
      VertexIter v1 = e->halfedge()->twin()->vertex();
      _newf = mesh.bevelEdge(e->halfedge()->edge());
      scene->selected.element = elementAddress(_newf->halfedge()->edge());
      scene->hovered.clear();
      scene->elementTransform->target.clear();
   } else if (v != nullptr) {
      beveledVertexPos = v->position;
      _newf = mesh.bevelVertex(v->halfedge()->vertex());
      scene->selected.element = elementAddress(_newf->halfedge()->vertex());
      scene->hovered.clear();
      scene->elementTransform->target.clear();
   } else {
      return;
   }
   // handle n-gons generated with this new face
   vector<FaceIter> fcs;
   // new face
   HalfedgeElement* el = elementAddress(_newf);
   Face* newf = el->getFace();
   if (newf->degree() > 3) {
      fcs.push_back(_newf);
   }
   // adjacent faces
   FaceIter tmp = _newf;
   HalfedgeIter h = _newf->halfedge();
   // clear out stored data from previous bevel
   bevelVertices.clear();
   beveledEdgePos.clear();
   do {
      // prepare polygon faces to split
      FaceIter _adjf = h->twin()->face();
      HalfedgeElement *element = elementAddress(_adjf);
      Face* adjf = element->getFace();
      if (adjf->degree() > 3) {
         fcs.push_back(_adjf);
      }
      // prepare vertices to reposition
      bevelVertices.push_back( h->twin()->next() );
      if (e != nullptr) { // if bevel edge, store original endpoints positions
         beveledEdgePos.push_back(h->vertex()->position);
      }
      h = h->next();
   } while (h != _newf->halfedge());
   // scene->selected.clear();
   scene->elementTransform->target.clear();
}

void Mesh::triangulate()
{
   mesh.triangulate();
}

void Mesh::upsample()
{
   for( FaceCIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++ )
   {
      if( f->degree() != 3 )
      {
         cerr << "Warning: refusing to apply Loop subdivision to surface with non-triangular faces!" << endl;
         cerr << "(Try triangulating first.)" << endl;
         return;
      }
   }
   resampler.upsample(mesh);
   scene->selected.clear();
   scene->hovered.clear();
   scene->elementTransform->target.clear();
}

void Mesh::downsample() {
  resampler.downsample(mesh);
   scene->selected.clear();
   scene->hovered.clear();
   scene->elementTransform->target.clear();
}

void Mesh::resample() {
  resampler.resample(mesh);
   scene->selected.clear();
   scene->hovered.clear();
   scene->elementTransform->target.clear();
}

void Mesh::newPickElement( int& pickID, HalfedgeElement* e )
{
   unsigned char R, G, B;
   IndexToRGB( pickID, R, G, B );
   glColor3ub( R, G, B );
   idToElement[pickID] = e;
   pickID++;
}

void Mesh::draw_pick( int& pickID )
{
   idToElement.clear();

   for( FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++ )
   {
      Vector3D c = f->centroid();

      HalfedgeIter h = f->halfedge();
      do
      {

         HalfedgeIter h1 = h;
         HalfedgeIter h2 = h->next();

         Vector3D a1, b1, p1, q1, r1;
         Vector3D a2, b2, p2, q2, r2;

         h1->getPickPoints( a1, b1, p1, q1, r1 );
         h2->getPickPoints( a2, b2, p2, q2, r2 );

         glBegin( GL_TRIANGLES );

         // vertex
         newPickElement( pickID, elementAddress(h2->vertex()) );
         glVertex3dv( &a1.x );
         glVertex3dv( &p1.x );
         glVertex3dv( &r1.x );

         // face
         newPickElement( pickID, elementAddress(f) );
         glVertex3dv( &b1.x );
         glVertex3dv( &b2.x );
         glVertex3dv( &c.x );

         glEnd();

         glBegin( GL_QUADS );

         // edge
         newPickElement( pickID, elementAddress(h2->edge()) );
         glVertex3dv( &p1.x );
         glVertex3dv( &r2.x );
         glVertex3dv( &q2.x );
         glVertex3dv( &q1.x );

         // halfedge
         newPickElement( pickID, elementAddress(h2) );
         glVertex3dv( &q1.x );
         glVertex3dv( &q2.x );
         glVertex3dv( &b2.x );
         glVertex3dv( &b1.x );

         glEnd();

         h = h->next();
      }
      while( h != f->halfedge() );
   }
}

void Mesh::setSelection( int pickID, Selection& selection )
{
   // Set the selection to the element specified by the given picking ID;
   // these values were generated in Mesh::draw_pick.
   if( idToElement.find(pickID) != idToElement.end() )
   {
      selection.clear();
      selection.object = this;
      selection.element = idToElement[ pickID ];
   }
}

void Mesh::drag( double x, double y, double dx, double dy, const Matrix4x4& modelViewProj )
{
   cerr << "NO MESH DRAG" << endl;
}

BSDF* Mesh::get_bsdf() {
  return bsdf;
}

StaticScene::SceneObject *Mesh::get_static_object() {
  return new StaticScene::Mesh(mesh, bsdf);
}


} // namespace DynamicScene
} // namespace CMU462
