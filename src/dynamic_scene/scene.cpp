#include "scene.h"
#include "../halfEdgeMesh.h"
#include "mesh.h"
#include "widgets.h"
#include <fstream>

using std::cout;
using std::endl;

namespace CMU462 { namespace DynamicScene {

Scene::Scene(std::vector<SceneObject *> _objects, std::vector<SceneLight *> _lights)
{
   for( int i = 0; i < _objects.size(); i++ )
   {
      _objects[i]->scene = this;
      objects.insert( _objects[i] );
   }

   for( int i = 0; i < _lights.size(); i++ )
   {
      lights.insert( _lights[i] );
   }

   elementTransform = new XFormWidget();
}

Scene::~Scene()
{
   if( elementTransform != nullptr )
   {
      delete elementTransform;
   }
}

BBox Scene::get_bbox() {
  BBox bbox;
  for (SceneObject *obj : objects) {
    bbox.expand(obj->get_bbox());
  }
  return bbox;
}

bool Scene::addObject( SceneObject* o )
{
   auto i = objects.find( o );
   if( i != objects.end() )
   {
      return false;
   }

   objects.insert( o );
   return true;
}

bool Scene::removeObject( SceneObject* o )
{
   auto i = objects.find( o );
   if( i == objects.end() )
   {
      return false;
   }

   objects.erase( o );
   return true;
}

void Scene::set_draw_styles(DrawStyle *defaultStyle, DrawStyle *hoveredStyle,
                             DrawStyle *selectedStyle) {
  for (SceneObject *obj : objects) {
    obj->set_draw_styles(defaultStyle, hoveredStyle, selectedStyle);
  }
}

void Scene::render_in_opengl()
{
   for (SceneObject *obj : objects)
   {
      obj->draw();
   }
}

void Scene::getHoveredObject( const Vector2D& p )
{
   // Set the background color to the maximum possible value---this value should be far
   // beyond the maximum pick index, since we have at most 2^(8+8+8) = 16,777,216 distinct IDs
   glClearColor( 1., 1., 1., 1. );

   // Clear any color values currently in the color buffer---we do not want to use these for
   // picking, since they represent, e.g., shading colors rather than pick IDs.  Also clear
   // the depth buffer so that we can use it to determine the closest object under the cursor.
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   // We want to draw the pick IDs as raw color values; shading functionality
   // like lighting and blending shouldn't interfere.
   glPushAttrib( GL_ALL_ATTRIB_BITS );
   glDisable( GL_LIGHTING );
   glDisable( GL_BLEND );

   // Keep track of the number of picking IDs used so far
   int pickID = 0;

   // Also keep track of the range of picking IDs used for each object;
   // in particular, IDs with value greater than or equal to pickRange[i]
   // and strictly less than pickRange[i+1] will belong to object i.
   vector<int> pickRange;
   pickRange.push_back( 0 ); // set lower end of range for first object to zero

   for( auto o : objects )
   {
      o->draw_pick( pickID );
      pickRange.push_back( pickID );
   }

   unsigned char color[4];
   glReadPixels( p.x, p.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color );

   int ID = RGBToIndex( color[0], color[1], color[2] );

   // By default, set hovered object to "none"
   hovered.clear();

   // Determine which object generated this pick ID
   int i = 0;
   for( auto o : objects )
   {
      // Call the object's method for setting the selection
      // based on the ID.  (This allows the object to set
      // the selection to an element within that particular
      // object type, e.g., for a mesh it can specify that a
      // particular vertex is selected, or for a camera it might
      // specify that a control handle was selected, etc.)
      o->setSelection( ID, hovered );

      i++;
   }

   // Restore any draw state that we disabled above.
   glPopAttrib();
}

bool Scene::has_selection() {
  return selected.object != nullptr;
}

bool Scene::has_hover() {
  return hovered.object != NULL;
}

void Scene::selectNextHalfedge()
{
   if( selected.element )
   {
      Halfedge* h = selected.element->getHalfedge();
      if( h )
      {
         selected.element = elementAddress( h->next() );
      }
   }
}

void Scene::selectTwinHalfedge()
{
   if( selected.element )
   {
      Halfedge* h = selected.element->getHalfedge();
      if( h )
      {
         selected.element = elementAddress( h->twin() );
      }
   }
}

void Scene::selectHalfedge()
{
   if( selected.element )
   {
      Face   *f = selected.element->getFace();
      Edge   *e = selected.element->getEdge();
      Vertex *v = selected.element->getVertex();
      HalfedgeIter h;
      if (f != nullptr) {
         h = f->halfedge();
      } else if ( e != nullptr) {
         h = e->halfedge();
      } else if (v != nullptr) {
         h = v->halfedge();
      } else {
         return;
      }
      selected.element = elementAddress(h);
   }
}

void Scene::triangulateSelection()
{
   Mesh* mesh = dynamic_cast<Mesh*>( selected.object );
   if( mesh )
   {
      mesh->mesh.triangulate();
      clearSelections();
   }
}

void Scene::subdivideSelection( bool useCatmullClark )
{
   if( selected.object == nullptr ) return;

   Mesh* mesh = dynamic_cast<Mesh*>( selected.object );
   if( mesh )
   {
      mesh->mesh.subdivideQuad( useCatmullClark );

      // Old elements are invalid
      clearSelections();

      // Select the subdivided mesh again, so that we can keep hitting
      // the same key to get multiple subdivisions (without having to
      // click on a mesh element).
      selected.object = mesh;
      selected.element = elementAddress( mesh->mesh.verticesBegin() );
   }
}

void Scene::clearSelections()
{
   hovered.clear();
   selected.clear();
   edited.clear();
   elementTransform->target.clear();
}

Info Scene::getSelectionInfo()
{
   Info info;

   if( !selected.object )
   {
      info.push_back( "(nothing selected)" );
      return info;
   }

   info = selected.object->getInfo();
   return info;
}

void Scene::collapse_selected_edge() {
   if( selected.object == nullptr || selected.element == nullptr ) return;
   Mesh* m = dynamic_cast<Mesh*>( selected.object );
   if( m ) m->collapse_selected_edge();
}

void Scene::flip_selected_edge() {
   if( selected.object == nullptr || selected.element == nullptr ) return;
   Mesh* m = dynamic_cast<Mesh*>( selected.object );
   if( m ) m->flip_selected_edge();
}

void Scene::split_selected_edge() {
   if( selected.object == nullptr || selected.element == nullptr ) return;
   Mesh* m = dynamic_cast<Mesh*>( selected.object );
   if( m ) m->split_selected_edge();
}

void Scene::erase_selected_element() {
   if( selected.object == nullptr || selected.element == nullptr ) return;
   Mesh* m = dynamic_cast<Mesh*>( selected.object );
   if( m ) m->erase_selected_element();
}

void Scene::upsample_selected_mesh() {
   if( selected.object == nullptr || selected.element == nullptr ) return;
   Mesh* m = dynamic_cast<Mesh*>( selected.object );
   if( m ) m->upsample();
   clearSelections();
}

void Scene::downsample_selected_mesh() {
   if( selected.object == nullptr || selected.element == nullptr ) return;
   Mesh* m = dynamic_cast<Mesh*>( selected.object );
   if( m ) m->downsample();
   clearSelections();
}

void Scene::resample_selected_mesh() {
   if( selected.object == nullptr || selected.element == nullptr ) return;
   Mesh* m = dynamic_cast<Mesh*>( selected.object );
   if( m ) m->resample();
   clearSelections();
}

StaticScene::Scene *Scene::get_static_scene() {
  std::vector<StaticScene::SceneObject *> staticObjects;
  std::vector<StaticScene::SceneLight *> staticLights;

  for (SceneObject *obj : objects) {
    staticObjects.push_back(obj->get_static_object());
  }
  for (SceneLight *light : lights) {
    staticLights.push_back(light->get_static_light());
  }

  return new StaticScene::Scene(staticObjects, staticLights);
}

void Scene::bevel_selected_element()
{
   // Don't re-bevel an element that we're already editing (i.e., beveling)
   if( edited.element == selected.element ) return;

   Mesh* mesh = dynamic_cast<Mesh*>( selected.object );
   if( mesh )
   {
      mesh->bevel_selected_element();
      edited = selected;
   }
}

void Scene::update_bevel_amount( float dx, float dy )
{
   Mesh* mesh = dynamic_cast<Mesh*>( edited.object );
   if( mesh )
   {
      mesh->_bevel_selection( dx/100., dy/100. );
   }
}


} // namespace DynamicScene
} // namespace CMU462
