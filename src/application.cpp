#include "application.h"

#include "dynamic_scene/ambient_light.h"
#include "dynamic_scene/environment_light.h"
#include "dynamic_scene/directional_light.h"
#include "dynamic_scene/area_light.h"
#include "dynamic_scene/point_light.h"
#include "dynamic_scene/spot_light.h"
#include "dynamic_scene/sphere.h"
#include "dynamic_scene/mesh.h"
#include "dynamic_scene/widgets.h"

#include "GLFW/glfw3.h"

#include <sstream>
using namespace std;

using Collada::CameraInfo;
using Collada::LightInfo;
using Collada::MaterialInfo;
using Collada::PolymeshInfo;
using Collada::SceneInfo;
using Collada::SphereInfo;

namespace CMU462 {

  Application::Application(AppConfig config)
  {
    scene = nullptr;

    pathtracer = new PathTracer (
        config.pathtracer_ns_aa,
        config.pathtracer_max_ray_depth,
        config.pathtracer_ns_area_light,
        config.pathtracer_ns_diff,
        config.pathtracer_ns_glsy,
        config.pathtracer_ns_refr,
        config.pathtracer_num_threads,
        config.pathtracer_envmap
        );
  }

  Application::~Application()
  {
    if( pathtracer != nullptr ) delete pathtracer;
    if( scene != nullptr ) delete scene;
  }

  void Application::init()
  {
    if( scene != nullptr )
    {
      delete scene;
      scene = nullptr;
    }

    textManager.init(use_hdpi);
    text_color = Color(1.0, 1.0, 1.0);

    // Setup all the basic internal state to default values,
    // as well as some basic OpenGL state (like depth testing
    // and lighting).

    // Set the integer bit vector representing which keys are down.
    leftDown   = false;
    rightDown  = false;
    middleDown = false;

    show_coordinates = true;
    show_hud = true;

    // Lighting needs to be explicitly enabled.
    glEnable(GL_LIGHTING);

    // Enable anti-aliasing and circular points.
    glEnable( GL_LINE_SMOOTH );
    // glEnable( GL_POLYGON_SMOOTH ); // XXX causes cracks!
    glEnable(GL_POINT_SMOOTH);
    glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
    // glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );
    glHint(GL_POINT_SMOOTH_HINT,GL_NICEST);

    // Initialize styles (colors, line widths, etc.) that will be used
    // to draw different types of mesh elements in various situations.
    initialize_style();

    mode = MODEL_MODE;
    action = Action::Navigate;
    scene = nullptr;

    // Make a dummy camera so resize() doesn't crash before the scene has been
    // loaded.
    // NOTE: there's a chicken-and-egg problem here, because load()
    // requires init, and init requires init_camera (which is only called by
    // load()).
    screenW = screenH = 600; // Default value
    CameraInfo cameraInfo;
    cameraInfo.hFov = 20;
    cameraInfo.vFov = 28;
    cameraInfo.nClip = 0.1;
    cameraInfo.fClip = 100;
    camera.configure(cameraInfo, screenW, screenH);
    canonicalCamera.configure(cameraInfo, screenW, screenH);
  }

  void Application::initialize_style() {
    // Colors.
    defaultStyle.halfedgeColor = Color( 0.3, 0.3, 0.3, 1.0 );
    hoverStyle.halfedgeColor = Color( 0.8, 0.8, 1.0, 1.0 );
    selectStyle.halfedgeColor = Color( 0.4, 0.4, 0.8, 1.0 );

    defaultStyle.faceColor = Color( 0.2, 0.2, 0.2, 1.0 );
    hoverStyle.faceColor = Color( 0.8, 0.8, 1.0, 1.0 );
    selectStyle.faceColor = Color( 0.4, 0.4, 0.8, 1.0 );

    defaultStyle.edgeColor = Color( 0.3, 0.3, 0.3, 1.0 );
    hoverStyle.edgeColor = Color( 0.8, 0.8, 1.0, 1.0 );
    selectStyle.edgeColor = Color( 0.4, 0.4, 0.8, 1.0 );

    defaultStyle.vertexColor = Color( 0.3, 0.3, 0.3, 1.0 );
    hoverStyle.vertexColor = Color( 0.8, 0.8, 1.0, 1.0 );
    selectStyle.vertexColor = Color( 0.4, 0.4, 0.8, 1.0 );

    // Primitive sizes.
    defaultStyle.strokeWidth = 1.0;
    hoverStyle.strokeWidth = 4.0;
    selectStyle.strokeWidth = 4.0;

    defaultStyle.vertexRadius = 8.0;
    hoverStyle.vertexRadius = 16.0;
    selectStyle.vertexRadius = 16.0;
  }

  void Application::update_style() {

    float view_distance = (camera.position() - camera.view_point()).norm();
    float scale_factor = canonical_view_distance / view_distance;

    hoverStyle.strokeWidth = 2.0 * scale_factor;
    selectStyle.strokeWidth = 2.0 * scale_factor;

    hoverStyle.vertexRadius = 8.0 * scale_factor;
    selectStyle.vertexRadius = 8.0 * scale_factor;
  }

  void Application::render() {
    glClearColor( 0., 0., 0., 0. );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    update_gl_camera();

    switch (mode) {
      case MODEL_MODE:
        if (show_coordinates) draw_coordinates();
        scene->render_in_opengl();
        if (show_hud) draw_hud();
        break;
      case VISUALIZE_MODE:
        if (show_coordinates) draw_coordinates();
      case RENDER_MODE:
        pathtracer->update_screen();
        break;
    }

    draw_action();
  }

  void Application::update_gl_camera() {

    // Call resize() every time we draw, since it doesn't seem
    // to get called by the Viewer upon initial window creation
    // (this should probably be fixed!).
    GLint view[4];
    glGetIntegerv(GL_VIEWPORT, view);
    if (view[2] != screenW || view[3] != screenH) {
      resize(view[2], view[3]);
    }

    // Control the camera to look at the mesh.
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const Vector3D& c = camera.position();
    const Vector3D& r = camera.view_point();
    const Vector3D& u = camera.up_dir();

    gluLookAt(c.x, c.y, c.z,
        r.x, r.y, r.z,
        u.x, u.y, u.z);
  }

  void Application::resize(size_t w, size_t h) {
    screenW = w;
    screenH = h;
    camera.set_screen_size(w, h);
    textManager.resize(w, h);
    set_projection_matrix();
    if (mode != MODEL_MODE) {
      pathtracer->set_frame_size(w, h);
    }
  }

  void Application::set_projection_matrix() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(camera.v_fov(),
        camera.aspect_ratio(),
        camera.near_clip(),
        camera.far_clip());
  }

  string Application::name() {
    return "Scotty3D";
  }

  string Application::info() {
    switch (mode) {
      case MODEL_MODE:
        return "MeshEdit";
        break;
      case RENDER_MODE:
      case VISUALIZE_MODE:
        return "PathTracer";
        break;
    }
  }

  void Application::load(SceneInfo* sceneInfo) {

    vector<Collada::Node>& nodes = sceneInfo->nodes;
    vector<DynamicScene::SceneLight *> lights;
    vector<DynamicScene::SceneObject *> objects;

    // save camera position to update camera control later
    CameraInfo *c;
    Vector3D c_pos = Vector3D();
    Vector3D c_dir = Vector3D();

    int len = nodes.size();
    for (int i = 0; i < len; i++) {
      Collada::Node& node = nodes[i];
      Collada::Instance *instance = node.instance;
      const Matrix4x4& transform = node.transform;

      switch(instance->type) {
        case Collada::Instance::CAMERA:
          c = static_cast<CameraInfo*>(instance);
          c_pos = (transform * Vector4D(c_pos,1)).to3D();
          c_dir = (transform * Vector4D(c->view_dir,1)).to3D().unit();
          init_camera(*c, transform);
          break;
        case Collada::Instance::LIGHT:
          {
            lights.push_back(
                init_light(static_cast<LightInfo&>(*instance), transform));
            break;
          }
        case Collada::Instance::SPHERE:
          objects.push_back(
              init_sphere(static_cast<SphereInfo&>(*instance), transform));
          break;
        case Collada::Instance::POLYMESH:
          objects.push_back(
              init_polymesh(static_cast<PolymeshInfo&>(*instance), transform));
          break;
        case Collada::Instance::MATERIAL:
          init_material(static_cast<MaterialInfo&>(*instance));
          break;
      }
    }

    scene = new DynamicScene::Scene(objects, lights);

    const BBox& bbox = scene->get_bbox();
    if (!bbox.empty()) {

      Vector3D target = bbox.centroid();
      canonical_view_distance = bbox.extent.norm() / 2 * 1.5;

      double view_distance = canonical_view_distance * 2;
      double min_view_distance = canonical_view_distance / 10.0;
      double max_view_distance = canonical_view_distance * 20.0;

      canonicalCamera.place(target,
          acos(c_dir.y),
          atan2(c_dir.x, c_dir.z),
          view_distance,
          min_view_distance,
          max_view_distance);

      camera.place(target,
          acos(c_dir.y),
          atan2(c_dir.x, c_dir.z),
          view_distance,
          min_view_distance,
          max_view_distance);

      set_scroll_rate();
    }

    // set default draw styles for meshEdit -
    scene->set_draw_styles(&defaultStyle, &hoverStyle, &selectStyle);

    // cerr << "==================================" << endl;
    // cerr << "CAMERA" << endl;
    // cerr << "      hFov: " << camera.hFov << endl;
    // cerr << "      vFov: " << camera.vFov << endl;
    // cerr << "        ar: " << camera.ar << endl;
    // cerr << "     nClip: " << camera.nClip << endl;
    // cerr << "     fClip: " << camera.fClip << endl;
    // cerr << "       pos: " << camera.pos << endl;
    // cerr << " targetPos: " << camera.targetPos << endl;
    // cerr << "       phi: " << camera.phi << endl;
    // cerr << "     theta: " << camera.theta << endl;
    // cerr << "         r: " << camera.r << endl;
    // cerr << "      minR: " << camera.minR << endl;
    // cerr << "      maxR: " << camera.maxR << endl;
    // cerr << "       c2w: " << camera.c2w << endl;
    // cerr << "   screenW: " << camera.screenW << endl;
    // cerr << "   screenH: " << camera.screenH << endl;
    // cerr << "screenDist: " << camera.screenDist<< endl;
    // cerr << "==================================" << endl;
  }

  void Application::init_camera(CameraInfo& cameraInfo,
      const Matrix4x4& transform) {
    camera.configure(cameraInfo, screenW, screenH);
    canonicalCamera.configure(cameraInfo, screenW, screenH);
    set_projection_matrix();
  }

  void Application::reset_camera() {
    camera.copy_placement(canonicalCamera);
  }

  DynamicScene::SceneLight *Application::init_light(LightInfo& light,
      const Matrix4x4& transform) {
    switch(light.light_type) {
      case Collada::LightType::NONE:
        break;
      case Collada::LightType::AMBIENT:
        return new DynamicScene::AmbientLight(light);
      case Collada::LightType::DIRECTIONAL:
        return new DynamicScene::DirectionalLight(light, transform);
      case Collada::LightType::AREA:
        return new DynamicScene::AreaLight(light, transform);
      case Collada::LightType::POINT:
        return new DynamicScene::PointLight(light, transform);
      case Collada::LightType::SPOT:
        return new DynamicScene::SpotLight(light, transform);
      default:
        break;
    }
    return nullptr;
  }

  /**
   * The transform is assumed to be composed of translation, rotation, and
   * scaling, where the scaling is uniform across the three dimensions; these
   * assumptions are necessary to ensure the sphere is still spherical. Rotation
   * is ignored since it's a sphere, translation is determined by transforming the
   * origin, and scaling is determined by transforming an arbitrary unit vector.
   */
  DynamicScene::SceneObject *Application::init_sphere(
      SphereInfo& sphere, const Matrix4x4& transform) {
    const Vector3D& position = (transform * Vector4D(0, 0, 0, 1)).projectTo3D();
    double scale = (transform * Vector4D(1, 0, 0, 0)).to3D().norm();
    return new DynamicScene::Sphere(sphere, position, scale);
  }

  DynamicScene::SceneObject *Application::init_polymesh(
      PolymeshInfo& polymesh, const Matrix4x4& transform) {
    return new DynamicScene::Mesh(polymesh, transform);
  }

  void Application::set_scroll_rate() {
    scroll_rate = canonical_view_distance / 10;
  }

  void Application::init_material(MaterialInfo& material) {
    // TODO : Support Materials.
  }

  void Application::cursor_event( float x, float y )
  {
    if (leftDown && !middleDown && !rightDown) {
      mouse1_dragged(x, y);
    } else if (!leftDown && !middleDown && rightDown) {
      mouse2_dragged(x, y);
    } else if (!leftDown && !middleDown && !rightDown) {
      mouse_moved(x, y);
    }

    mouseX = x;
    mouseY = y;
  }

  void Application::scroll_event(float offset_x, float offset_y) {

    //update_style();

    switch(mode) {
      case MODEL_MODE:
        switch( action )
        {
          case Action::Navigate:
          case Action::Edit:
          case Action::Bevel:
            camera.move_forward(-offset_y * scroll_rate);
            break;
          default:
            break;
        }
        //scene->_bevel_selection(offset_y * scroll_rate);
        break;
      case VISUALIZE_MODE:
        camera.move_forward(-offset_y * scroll_rate);
        break;
      case RENDER_MODE:
        break;
    }
  }

  void Application::mouse_event(int key, int event, unsigned char mods) {
    switch(event) {
      case EVENT_PRESS:
        switch(key) {
          case MOUSE_LEFT:
            mouse_pressed(LEFT);
            break;
          case MOUSE_RIGHT:
            mouse_pressed(RIGHT);
            break;
          case MOUSE_MIDDLE:
            mouse_pressed(MIDDLE);
            break;
        }
        break;
      case EVENT_RELEASE:
        switch(key) {
          case MOUSE_LEFT:
            mouse_released(LEFT);
            break;
          case MOUSE_RIGHT:
            mouse_released(RIGHT);
            break;
          case MOUSE_MIDDLE:
            mouse_released(MIDDLE);
            break;
        }
        break;
    }
  }

  void Application::char_event( unsigned int codepoint )
  {
    bool queued = false;

    switch (mode) {
      case RENDER_MODE:
        switch (codepoint) {
          case 'm': case 'M':
            to_model_mode();
            break;
          case 'v': case 'V':
            pathtracer->stop();
            pathtracer->start_visualizing();
            mode = VISUALIZE_MODE;
            break;
          case 'w': case 'W':
            pathtracer->save_image();
            break;
          case '+': case '=':
            pathtracer->stop();
            pathtracer->increase_area_light_sample_count();
            pathtracer->start_raytracing();
            break;
          case '-': case '_':
            pathtracer->stop();
            pathtracer->decrease_area_light_sample_count();
            pathtracer->start_raytracing();
            break;
          case '[': case ']':
            pathtracer->key_press(codepoint);
            break;
        }
        break;
      case VISUALIZE_MODE:
        switch(codepoint) {
          case 'm': case 'M':
            to_model_mode();
            break;
          case 'r': case 'R':
            pathtracer->stop();
            pathtracer->start_raytracing();
            mode = RENDER_MODE;
            break;
          case ' ':
            reset_camera();
            break;
          default:
            pathtracer->key_press(codepoint);
        }
        break;
      case MODEL_MODE:
        switch(codepoint) {
          // case 'r': case 'R':
          //    set_up_pathtracer();
          //    pathtracer->start_raytracing();
          //    mode = RENDER_MODE;
          //    break;
          // case 'v': case 'V':
          //    set_up_pathtracer();
          //    pathtracer->start_visualizing();
          //    mode = VISUALIZE_MODE;
          //    break;
          case 'u': case 'U':
            scene->upsample_selected_mesh();
            break;
          case 'd': case 'D':
            scene->downsample_selected_mesh();
            break;
          case 'i': case 'I':
            // i for isotropic.
            scene->resample_selected_mesh();
            break;
          case 'f': case 'F':
            scene->flip_selected_edge();
            break;
          case 'p':
            scene->split_selected_edge();
            break;
          case 'c': case 'C':
            scene->collapse_selected_edge();
            break;
          case 'n': case 'N':
            scene->selectNextHalfedge();
            break;
          case 't':
            scene->selectTwinHalfedge();
            break;
          case 'T':
            scene->triangulateSelection();
            break;
          case 's':
            // Catmull-Clark subdivision
            scene->subdivideSelection( true );
            break;
          case 'S':
            // linear subdivision
            scene->subdivideSelection( false );
            break;
          case 'h':
            scene->selectHalfedge();
            break;
          case ' ':
            to_navigate_action();
            break;
          case 'b':
            toggle_bevel_action();
            break;
          case 'e':
            cycle_edit_action();
            break;
          case 'w': case 'W':
            queueWrite();
            queued = true;
            break;
          case 'l': case 'L':
            queueLoad();
            queued = true;
            break;
          default:
            break;
        }
        break;
    }

    if( !queued )
    {
      executeFileOp( codepoint );
    }
    updateWidgets();
  }

  void Application::queueWrite()
  {
    readyWrite = true;
    cerr << "(Press a key 0-9 to write to a buffer)" << endl;
  }

  void Application::queueLoad()
  {
    readyLoad = true;
    cerr << "(Press a key 0-9 to load from a buffer)" << endl;
  }

  void Application::executeFileOp( int codepoint )
  {
    // If the user already pressed 'w' or 'l' (indicating that
    // they wanted to write or load a file) AND the next key
    // they pressed was a number, then we should go ahead and
    // write/load the file corresponding to the specified number.

    if( !readyLoad && !readyWrite ) return;

    // Convert Unicode codepoint to integer.  (Probably a more
    // clever way to do this, but often a foolproof solution is
    // (much) better than a clever one...)
    int index = -1;
    switch( codepoint ) {
      case '0': index = 0; break;
      case '1': index = 1; break;
      case '2': index = 2; break;
      case '3': index = 3; break;
      case '4': index = 4; break;
      case '5': index = 5; break;
      case '6': index = 6; break;
      case '7': index = 7; break;
      case '8': index = 8; break;
      case '9': index = 9; break;
      default:
                readyLoad = readyWrite = false;
                return;
                break;
    }

    stringstream filename;
    filename << "Scotty3D_buffer" << index << ".dae";

    if( readyWrite ) writeScene( filename.str().c_str() );
    else if( readyLoad  )  loadScene( filename.str().c_str() );

    readyLoad = readyWrite = false;
  }

  void Application::updateWidgets()
  {
    if( mode != MODEL_MODE || action != Action::Edit || scene->selected.object == nullptr )
    {
      scene->removeObject( scene->elementTransform );
      return;
    }
    else
    {
      // If we're in edit mode and the last key press resulted in some new
      // mesh element being selected, we should make this element the focus
      // of the transformation widget.
      if( mode == MODEL_MODE && action == Action::Edit )
      {
        if( scene->selected.object != scene->elementTransform &&
            scene->selected.element != nullptr )
        {
          scene->elementTransform->setTarget( scene->selected );
        }
      }
    }
  }

  void Application::keyboard_event( int key, int event, unsigned char mods )
  {
    switch( mode )
    {
      case RENDER_MODE:
        break;
      case VISUALIZE_MODE:
        break;
      case MODEL_MODE:
        switch( key )
        {
          case GLFW_KEY_BACKSPACE:
          case GLFW_KEY_DELETE:
            if( event == GLFW_PRESS )
            {
              scene->erase_selected_element();
            }
            break;
          case GLFW_KEY_TAB:
            if( event == GLFW_PRESS )
            {
              show_hud = !show_hud;
            }
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }

    if( lastEventWasModKey && event == GLFW_RELEASE )
    {
      scene->elementTransform->restoreLastMode();
    }

    lastEventWasModKey = false;
    if( mods )
    {
      lastEventWasModKey = true;

      switch( mods )
      {
        case GLFW_MOD_SHIFT:
          scene->elementTransform->setScale();
          break;
        case GLFW_MOD_CONTROL:
          scene->elementTransform->setTranslate();
          break;
        case GLFW_MOD_ALT:
          scene->elementTransform->setRotate();
          break;
        default:
          break;
      }
    }

    updateWidgets();
  }

  void Application::writeScene( const char* filename )
  {
    cerr << "Writing scene to file " << filename << endl;
    Collada::ColladaWriter::writeScene( *scene, filename );
  }

  void Application::loadScene( const char* filename )
  {
    cerr << "Loading scene from file " << filename << endl;

    Camera originalCamera = camera;
    Camera originalCanonicalCamera = canonicalCamera;

    Collada::SceneInfo *sceneInfo = new Collada::SceneInfo();
    if (Collada::ColladaParser::load(filename, sceneInfo) < 0)
    {
      cerr << "Warning: scene file failed to load." << endl;
      delete sceneInfo;
      return;
    }
    load(sceneInfo);

    camera = originalCamera;
    canonicalCamera = originalCanonicalCamera;
  }

  void Application::to_navigate_action()
  {
    action = Action::Navigate;
  }

  void Application::toggle_bevel_action()
  {
    if( action != Action::Bevel )
    {
      action = Action::Bevel;
    }
    else
    {
      action = Action::Navigate;
    }
  }

  void Application::cycle_edit_action()
  {
    if( action != Action::Edit )
    {
      action = Action::Edit;
      setupElementTransformWidget();
    }
    else
    {
      scene->elementTransform->cycleMode();
    }

    updateWidgets();
  }


  void Application::mouse_pressed(e_mouse_button b) {
    switch (b) {
      case LEFT:
        leftDown = true;
        if (mode == MODEL_MODE)
        {
          selectHovered();
          if( action == Action::Bevel )
          {
            scene->bevel_selected_element();
          }
        }
        break;
      case RIGHT:
        rightDown = true;
        break;
      case MIDDLE:
        middleDown = true;
        break;
    }

    updateWidgets();
  }

  void Application::selectHovered()
  {
    scene->selected = scene->hovered;

    scene->elementTransform->setClickPosition( Vector2D(mouseX,mouseY) );

    setupElementTransformWidget();

    scene->edited.clear();
  }

  void Application::setupElementTransformWidget()
  {
    if( scene->selected.element != nullptr )
    {
      scene->elementTransform->setTarget( scene->selected );
      scene->addObject( scene->elementTransform );
    }
  }

  void Application::mouse_released(e_mouse_button b)
  {
    switch (b)
    {
      case LEFT:
        leftDown = false;
        scene->elementTransform->updateGeometry();
        break;
      case RIGHT:
        rightDown = false;
        break;
      case MIDDLE:
        middleDown = false;
        break;
    }

    updateWidgets();
  }

  /*
     When in edit mode and there is a selection, move the selection.
     When in visualization mode, rotate.
     */
  void Application::mouse1_dragged(float x, float y) {
    if (mode == RENDER_MODE) {
      return;
    }
    float dx = (x - mouseX);
    float dy = (y - mouseY);

    if (mode == MODEL_MODE ) {

      switch( action )
      {
        case( Action::Navigate ):
          camera.rotate_by(dy * (PI / screenH), dx * (PI / screenW));
          break;
        case( Action::Edit ):
          if( scene->has_selection() ) {
            dragSelection( x, y, dx, dy, get_world_to_3DH() );
          }
          else // if nothing is selected, we may as well allow the user to rotate the view...
          {
            camera.rotate_by(dy * (PI / screenH), dx * (PI / screenW));
          }
          break;
        case( Action::Bevel ):
          if( scene->has_selection() ) {
            scene->update_bevel_amount( dx, dy );
          }
          else // if nothing is selected, we may as well allow the user to rotate the view...
          {
            camera.rotate_by(dy * (PI / screenH), dx * (PI / screenW));
          }
          break;
        default:
          break;
      }

    } else {
      camera.rotate_by(dy * (PI / screenH), dx * (PI / screenW));
    }

    updateWidgets();
  }

  /*
     When the mouse is dragged with the right button held down, translate.
     */
  void Application::mouse2_dragged(float x, float y) {
    if (mode == RENDER_MODE) return;
    float dx = (x - mouseX);
    float dy = (y - mouseY);

    // don't negate y because up is down.
    camera.move_by(-dx, dy, canonical_view_distance);

    updateWidgets();
  }

  void Application::mouse_moved(float x, float y) {
    if (mode != MODEL_MODE) return;
    y = screenH - y; // Because up is down.
    // Converts x from [0, w] to [-1, 1], and similarly for y.
    //Vector2D p(x * 2 / screenW - 1, y * 2 / screenH - 1);
    Vector2D p(x,y);
    update_gl_camera();
    scene->getHoveredObject( p );
  }

  void Application::to_model_mode() {
    if (mode == MODEL_MODE) return;
    pathtracer->stop();
    pathtracer->clear();
    mode = MODEL_MODE;
    mouse_moved(mouseX, mouseY);
  }

  void Application::set_up_pathtracer() {
    if (mode != MODEL_MODE) return;
    pathtracer->set_camera(&camera);
    pathtracer->set_scene(scene->get_static_scene());
    pathtracer->set_frame_size(screenW, screenH);

  }

  Matrix4x4 Application::get_world_to_3DH() {
    Matrix4x4 P, M;
    glGetDoublev(GL_PROJECTION_MATRIX, &P(0, 0));
    glGetDoublev(GL_MODELVIEW_MATRIX, &M(0, 0));
    return P * M;
  }

  void Application::dragSelection( float x, float y, float dx, float dy, const Matrix4x4& modelViewProj )
  {
    if( scene->selected.object == nullptr ) return;

    dx *=  2./screenW;
    dy *= -2./screenH;

    if( scene->selected.element )
    {
      scene->selected.element->translate( dx, dy, modelViewProj );
    }
    else
    {
      scene->selected.object->drag( x, y, dx, dy, modelViewProj );
    }
  }

  inline void Application::draw_string(float x, float y,
      string str, size_t size, const Color& c) {
    int line_index = textManager.add_line(( x * 2 / screenW) - 1.0,
        (-y * 2 / screenH) + 1.0,
        str, size, c);
    messages.push_back(line_index);
  }

  void Application::draw_coordinates() {

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE );
    glLineWidth( 2. );

    glBegin(GL_LINES);
    glColor4f(1.0f, 0.0f, 0.0f, 0.5f);
    glVertex3i(0,0,0);
    glVertex3i(1,0,0);

    glColor4f(0.0f, 1.0f, 0.0f, 0.5f);
    glVertex3i(0,0,0);
    glVertex3i(0,1,0);

    glColor4f(0.0f, 0.0f, 1.0f, 0.5f);
    glVertex3i(0,0,0);
    glVertex3i(0,0,1);

    glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
    for (int x = 0; x <= 8; ++x) {
      glVertex3i(x - 4, 0, -4);
      glVertex3i(x - 4, 0,  4);
    }
    for (int z = 0; z <= 8; ++z) {
      glVertex3i(-4, 0, z - 4);
      glVertex3i( 4, 0, z - 4);
    }
    glEnd();

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

  }

  void Application::draw_hud() {
    textManager.clear();
    messages.clear();

    const size_t size = 16;
    const float x0 = use_hdpi ? screenW - 300 * 2 : screenW - 300;
    const float y0 = use_hdpi ? 128 : 64;
    const int inc  = use_hdpi ? 48  : 24;
    float y = y0 + inc - size;

    // No selection --> no messages.
    if (!scene->has_selection()) {
      draw_string(x0, y, "No mesh feature is selected", size, text_color);
      y += inc;
    } else {
      DynamicScene::Info selectionInfo = scene->getSelectionInfo();
      for (const string& s : selectionInfo) {
        size_t split = s.find_first_of(":");
        if (split != string::npos) {
          split++;
          string s1 = s.substr(0,split);
          string s2 = s.substr(split);
          draw_string(x0, y, s1, size, text_color);
          draw_string(x0 + (use_hdpi ? 150 : 75 ), y, s2, size, text_color);
        } else {
          draw_string(x0, y, s, size, text_color);
        }
        y += inc;
      }
    }

    // -- First draw a lovely black rectangle.

    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0, 0, screenW, screenH);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screenW, screenH, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0, 0, -1);

    // -- Black with opacity .8;

    glColor4f(0.0, 0.0, 0.0, 0.8);

    float min_x = x0 - 32;
    float min_y = y0 - 32;
    float max_x = screenW;
    float max_y = y;

    float z = 0.0;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glBegin(GL_QUADS);

    glVertex3f(min_x, min_y, z);
    glVertex3f(min_x, max_y, z);
    glVertex3f(max_x, max_y, z);
    glVertex3f(max_x, min_y, z);
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glPopAttrib();

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    textManager.render();
  }


  void Application::draw_action()
  {
    if( mode != MODEL_MODE ) return;

    textManager.clear();
    messages.clear();

    const size_t size = 16;
    const float x0 = use_hdpi ? 48 : 24;
    const float y0 = use_hdpi ? 80 : 40;
    const int inc  = use_hdpi ? 48  : 24;
    float y = y0 + inc - size;

    stringstream actionString;
    actionString << "ACTION: ";
    switch( action )
    {
      case( Action::Navigate ):
        actionString << "Navigate";
        break;
      case( Action::Edit ):
        actionString << "Edit";
        break;
      case( Action::Bevel ):
        actionString << "Bevel";
        break;
      default:
        actionString << "(none)";
        break;
    };

    Color action_color(0.3, .7, 0.3);
    draw_string( x0, y, actionString.str(), size, action_color );

    // // No selection --> no messages.
    // if (!scene->has_selection()) {
    //   draw_string(x0, y, "No mesh feature is selected", size, text_color);
    //   y += inc;
    // } else {
    //   DynamicScene::SelectionInfo selectionInfo = scene->get_selection_info();
    //   for (const string& s : selectionInfo) {
    //     size_t split = s.find_first_of(":");
    //     if (split != string::npos) {
    //       split++;
    //       string s1 = s.substr(0,split);
    //       string s2 = s.substr(split);
    //       draw_string(x0, y, s1, size, text_color);
    //       draw_string(x0 + (use_hdpi ? 150 : 75 ), y, s2, size, text_color);
    //     } else {
    //       draw_string(x0, y, s, size, text_color);
    //     }
    //     y += inc;
    //   }
    // }

    // // -- First draw a lovely black rectangle.

    // glPushAttrib(GL_VIEWPORT_BIT);
    // glViewport(0, 0, screenW, screenH);

    // glMatrixMode(GL_PROJECTION);
    // glPushMatrix();
    // glLoadIdentity();
    // glOrtho(0, screenW, screenH, 0, 0, 1);

    // glMatrixMode(GL_MODELVIEW);
    // glPushMatrix();
    // glLoadIdentity();
    // glTranslatef(0, 0, -1);

    // // -- Black with opacity .8;

    // glColor4f(0.0, 0.0, 0.0, 0.8);

    // float min_x = x0 - 32;
    // float min_y = y0 - 32;
    // float max_x = screenW;
    // float max_y = y;

    // float z = 0.0;

    // glDisable(GL_DEPTH_TEST);
    // glDisable(GL_LIGHTING);

    // glBegin(GL_QUADS);

    // glVertex3f(min_x, min_y, z);
    // glVertex3f(min_x, max_y, z);
    // glVertex3f(max_x, max_y, z);
    // glVertex3f(max_x, min_y, z);
    // glEnd();

    // glMatrixMode(GL_PROJECTION);
    // glPopMatrix();

    // glMatrixMode(GL_MODELVIEW);
    // glPopMatrix();

    // glPopAttrib();

    // glEnable(GL_LIGHTING);
    // glEnable(GL_DEPTH_TEST);

    textManager.render();
  }



} // namespace CMU462
