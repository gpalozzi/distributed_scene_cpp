#ifndef _SCENE_DISTRIBUTED_H_
#define _SCENE_DISTRIBUTED_H_

#include "common.h"
#include "json.h"
#include "vmath.h"
#include "image.h"

class id_reference;

// forward declarations
struct BVHAccelerator;

// blinn-phong material
// textures are scaled by the respective coefficient and may be missing
struct Material {
    vec3f       ke = zero3f;            // emission coefficient
    vec3f       kd = one3f;             // diffuse coefficient
    vec3f       ks = zero3f;            // specular coefficient
    float       n = 10;                 // specular exponent
    vec3f       kr = zero3f;            // reflection coefficient
    
    image3f*    ke_txt = nullptr;       // emission texture
    image3f*    kd_txt = nullptr;       // diffuse texture
    image3f*    ks_txt = nullptr;       // specular texture
    image3f*    kr_txt = nullptr;       // reflection texture
    image3f*    norm_txt = nullptr;     // normal texture
    
    bool        double_sided = false;   // double-sided material
    bool        microfacet = false;     // use microfacet formulation
    
    timestamp_t         _id_ = 0;       // unique id
    int                 _version = -1;  // version

    // constructor
    Material() {}
    
    // copy constructor
    Material(const Material& mat){
        ke = mat.ke;
        kd = mat.kd;
        ks = mat.ks;
        n = mat.n;
        kr = mat.kr;
        _id_ = mat._id_;
    }
    
    // assignment operator
    Material& operator=(const Material& mat){
        ke = mat.ke;
        kd = mat.kd;
        ks = mat.ks;
        n = mat.n;
        kr = mat.kr;
        _id_ = mat._id_;
        return *this;
    }
};



// Keyframed Animation Data
struct FrameAnimation {
    frame3f                 rest_frame = identity_frame3f; // animation rest frame
    vector<int>             keytimes;   // key frame times
    vector<vec3f>           translation;// translation key frames
    vector<vec3f>           rotation;   // rotation key frames
};

// Mesh Skinning Data
struct MeshSkinning {
    vector<vec3f>           rest_pos;      // rest pos
    vector<vec3f>           rest_norm;     // rest norm
    vector<vec4i>           bone_ids;      // skin bones
    vector<vec4f>           bone_weights;  // skin weights
    vector<vector<mat4f>>   bone_xforms;   // bone xforms (bone index is the first index)
};

// Mesh Simulation Data
struct MeshSimulation {
    // simulation init data
    vector<vec3f>           init_pos;  // initial position
    vector<vec3f>           init_vel;  // initial velocity
    vector<float>           mass;      // mass
    vector<bool>            pinned;    // whether a point is pinned
    
    // inter-particle springs
    struct Spring {
        vec2i               ids;       // springs vertex ids
        float               restlength;// springs rest length
        float               ks;        // springs static constant
        float               kd;        // springs dynamic constant
    };
    vector<Spring>              springs;    // springs
    
    // simulation compute data
    vector<vec3f>           vel;       // velocity
    vector<vec3f>           force;     // forces
};

// Mesh Collision Data
struct MeshCollision {
    float                   radius;     // collision radius
    bool                    isquad;     // whether the collision object is a sphere or quad
};

// indexed mesh data structure with vertex positions and normals,
// a list of indices for triangle and quad faces, material and frame
struct Mesh {
    frame3f         frame = identity_frame3f;   // frame
    
    map<timestamp_t, pair<int,set<timestamp_t>>> vertices; // vertex ids (id -> index in pos, set of faces ids)
    vector<vec3f>   pos;                        // vertex position
    
    vector<timestamp_t> normal_ids;             // vertex normal ids
    vector<vec3f>   norm;                       // vertex normal
    
    vector<vec2f>   texcoord;                   // vertex texcture coordinates
   
    map<timestamp_t,tuple<int,vec3id,vec3f>>   triangle;    // triangle faces ids
    vector<vec3i>   triangle_index;                         // triangle index
    
    map<timestamp_t,tuple<int,vec4id,vec3f>>   quad;        // quad faces ids
    vector<vec4i>   quad_index;                             // quad index
    
    map<timestamp_t,pair<int,vec2id>> edge;                 // edges ids
    vector<vec2i>   edge_index;                             // edges index
    
    vector<int>     point;                      // point
    vector<vec2i>   line;                       // line
    vector<vec4i>   spline;                     // cubic bezier segments
    
    Material*       mat = new Material();       // material
    
    int  subdivision_catmullclark_level = 0;        // catmullclark subdiv level
    bool subdivision_catmullclark_smooth = false;   // catmullclark subdiv smooth
    int  subdivision_bezier_level = 0;              // bezier subdiv level
    
    FrameAnimation* animation = nullptr;        // animation data
    MeshSkinning*   skinning = nullptr;         // skinning data
    MeshSimulation* simulation = nullptr;       // simulation data
    MeshCollision*  collision = nullptr;        // collision data
    
    BVHAccelerator* bvh = nullptr;              // bvh accelerator for intersection
    
    timestamp_t         _id_ = 0;               // unique id
    int                 _version = -1;          // version
    
    // constructor
    Mesh() {}
    
    // copy constructor
    Mesh(const Mesh& mesh){
        frame = mesh.frame;
        pos = mesh.pos;
        normal_ids = mesh.normal_ids;
        norm = mesh.norm;
        texcoord = mesh.texcoord;
        triangle = mesh.triangle;
        triangle_index = mesh.triangle_index;
        quad = mesh.quad;
        quad_index = mesh.quad_index;
        edge = mesh.edge;
        edge_index = mesh.edge_index;
        vertices = mesh.vertices;

        _id_ = mesh._id_;
        _version = mesh._version;
        
        mat = new Material();
        *mat = *mesh.mat;
    }
};

// surface made of eitehr a spehre or a quad (as determined by
// isquad. the sphere is centered frame.o with radius radius.
// the quad is at frame.o with normal frame.z and axes frame.x, frame.y.
// the quad side is 2*radius.
struct Surface {
    frame3f     frame = identity_frame3f;   // frame
    float       radius = 1;                 // radius
    bool        isquad = false;             // whether it's a quad
    Material*   mat = new Material();       // material

    FrameAnimation* animation = nullptr;    // animation data

    Mesh*       _display_mesh = nullptr;    // display mesh
};

// point light at frame.o with intensity intensity
struct Light {
    frame3f     frame = identity_frame3f;       // frame
    vec3f       intensity = one3f;              // intersntiy
    
    timestamp_t         _id_ = 0;               // unique id
    int                 _version = -1;          // version
};

// perspective camera at frame.o with direction (-z)
// and image plane orientation (x,y); the image plane
// is at a distance dist with size (width,height);
// the camera is focussed at a distance focus.
struct Camera {
    frame3f frame = identity_frame3f;   // frame
    float   width = 1;                  // image plane width
    float   height = 1;                 // image plane height
    float   dist = 1;                   // image plane distance
    float   focus = 1;                  // distance of focus
    
    timestamp_t         _id_ = 0;       // unique id
    int                 _version = -1;  // version

};

// Scene Animation Data
struct SceneAnimation {
    int     time = 0;                       // current animation time
    int     length = 0;                     // animation length
    float   dest = 1/30.0f;                   // time in seconds for each time step
    int     simsteps = 100;                 // simulation steps for time step of animation
    vec3f   gravity = {0,-9.8f,0};          // acceleration of gravity
    vec2f   bounce_dump = {0.001f,0.5f};    // loss of velocity at bounce (parallel,ortho)
    bool    gpu_skinning = false;           // whether to skin on the gpu
};

// scene comprised of a camera, a list of meshes,
// and a list of lights. rendering parameters are
// also included, namely the background color (color
// if a ray misses) the ambient illumination, the
// image resolution (image_width, image_height) and
// the samples per pixel (image_samples).
struct Scene {
    Camera*             camera = new Camera();  // camera
    vector<Mesh*>       meshes;                 // meshes
    vector<Surface*>    surfaces;               // surfaces
    vector<Light*>      lights;                 // lights
    vector<Material*>   materials;              // materials

    vec3f               background = one3f*0.2; // background color
    image3f*            background_txt = nullptr;// background texture
    vec3f               ambient = one3f*0.2;    // ambient illumination

    SceneAnimation*     animation = new SceneAnimation();    // scene animation data

    int                 image_width = 512;      // image resolution in x
    int                 image_height = 512;     // image resolution in y
    int                 image_samples = 1;      // samples per pixels in each direction
    
    bool                draw_wireframe = false; // whether to use wireframe for interactive drawing
    bool                draw_animated = false;  // whether to draw with animation
    bool                draw_gpu_skinning = false;  // whether skinning is performed on the gpu
    bool                draw_captureimage = false;  // whether to capture the image in the next frame
    
    int                 path_max_depth = 2;     // maximum path depth
    bool                path_sample_brdf = true;// sample brdf in path tracing
    bool                path_shadows = true;    // whether to compute shadows
    
    map<timestamp_t,id_reference>    ids_map;

};

// Diff object

struct CameraDiff {
    frame3f frame = nanframe3f;   // frame
    float   width = nanf("");     // image plane width
    float   height = nanf("");    // image plane height
    float   dist = nanf("");      // image plane distance
    float   focus = nanf("");     // distance of focus
    
    timestamp_t         _id_ = 0;       // unique id
    int                 _version = -1;  // version
    
    // constructor
    CameraDiff() {}
    
    // copy constructor
    CameraDiff(const CameraDiff& cameradiff){
        frame = cameradiff.frame;
        width = cameradiff.width;
        height = cameradiff.height;
        dist = cameradiff.dist;
        focus = cameradiff.focus;
        _id_ = cameradiff._id_;
        _version = cameradiff._version;
    }
    
    // constructor from 2 cameras
    CameraDiff(const Camera* first_camera,const Camera* second_camera){
        error_if_not(first_camera->_id_ == second_camera->_id_, "[CameraDiff] cameras have different ids: %llu != %llu", first_camera->_id_, second_camera->_id_);
        
        _id_ = second_camera->_id_;
        _version = second_camera->_version;

        if (not (first_camera->frame == second_camera->frame)) frame = second_camera->frame;
        if (not (first_camera->width == second_camera->width)) width = second_camera->width;
        if (not (first_camera->height == second_camera->height)) height = second_camera->height;
        if (not (first_camera->dist == second_camera->dist)) dist = second_camera->dist;
        if (not (first_camera->focus == second_camera->focus)) focus = second_camera->focus;
    }
};

struct LightDiff {
    frame3f     frame = nanframe3f;             // frame
    vec3f       intensity = nan3f;              // intersntiy
    
    timestamp_t             _id_ = 0;           // unique id
    int                     _version = -1;      // version
    
    // constructor
    LightDiff() {}
    
    // copy constructor
    LightDiff(const LightDiff& lightdiff){
        frame = lightdiff.frame;
        intensity = lightdiff.intensity;
        _id_ = lightdiff._id_;
        _version = lightdiff._version;
    }
    
    // constructor from 2 lights
    LightDiff(const Light* first_light,const Light* second_light){
        error_if_not(first_light->_id_ == second_light->_id_, "[LightDiff] lights have different ids: %llu != %llu", first_light->_id_, second_light->_id_);
        
        _id_ = second_light->_id_;
        _version = second_light->_version;

        if (not (first_light->frame == second_light->frame)) frame = second_light->frame;
        if (not (first_light->intensity == second_light->intensity)) intensity = second_light->intensity;
    }
};

struct MaterialDiff {
    vec3f       ke = nan3f;        // emission coefficient
    vec3f       kd = nan3f;        // diffuse coefficient
    vec3f       ks = nan3f;        // specular coefficient
    float       n = nanf("");      // specular exponent
    vec3f       kr = nan3f;        // reflection coefficient
    
    timestamp_t             _id_ = 0;           // unique id
    int                     _version = -1;      // version

    // constructor
    MaterialDiff() {}
    
    // copy constructor
    MaterialDiff(const MaterialDiff& matdiff){
        ke = matdiff.ke;
        kd = matdiff.kd;
        ks = matdiff.ks;
        n = matdiff.n;
        kr = matdiff.kr;
        _id_ = matdiff._id_;
        _version = matdiff._version;
    }
    
    // constructor from 2 materials
    MaterialDiff(const Material* first_mat,const Material* second_mat){
        error_if_not(first_mat->_id_ == second_mat->_id_, "[MaterialDiff] materials have different ids: %llu != %llu", first_mat->_id_, second_mat->_id_);
        
        _id_ = second_mat->_id_;
        _version = second_mat->_version;

        if (not (first_mat->ke == second_mat->ke)) ke = second_mat->ke;
        if (not (first_mat->kd == second_mat->kd)) kd = second_mat->kd;
        if (not (first_mat->ks == second_mat->ks)) ks = second_mat->ks;
        if (not (first_mat->n == second_mat->n)) n = second_mat->n;
        if (not (first_mat->kr == second_mat->kr)) kr = second_mat->kr;
    }

};


struct MeshDiff {
    frame3f         frame = nanframe3f;   // frame
    
    // remove elements
    vector<timestamp_t> remove_vertex;
    vector<timestamp_t> remove_edge;
    vector<timestamp_t> remove_triangle;
    vector<timestamp_t> remove_quad;
    
    // add elements
    map<timestamp_t, vec3f> add_vertex;
    map<timestamp_t, vec3f> add_norm;
    map<timestamp_t, vec2id> add_edge;
    map<timestamp_t, vec3id> add_triangle;
    map<timestamp_t, vec4id> add_quad;
    
    // update elements
    map<timestamp_t, vec3f> update_vertex;
    map<timestamp_t, vec3f> update_norm;
    
    timestamp_t             _id_ = 0;
    int                     _version = -1;
    
    // constructor
    MeshDiff() {}
    
    // copy constructor
    MeshDiff(const MeshDiff& meshdiff){
        frame = meshdiff.frame;
        remove_vertex = meshdiff.remove_vertex;
        remove_edge = meshdiff.remove_edge;
        remove_triangle = meshdiff.remove_triangle;
        remove_quad = meshdiff.remove_quad;
        add_vertex = meshdiff.add_vertex;
        add_norm = meshdiff.add_norm;
        add_edge = meshdiff.add_edge;
        add_triangle = meshdiff.add_triangle;
        update_vertex = meshdiff.update_vertex;
        update_norm = meshdiff.update_norm;
        _id_ = meshdiff._id_;
        _version = meshdiff._version;
    }
    
    // constructor from 2 meshes
    MeshDiff(const Mesh* first_mesh,const Mesh* second_mesh, bool geometric_only = false){
        error_if_not(first_mesh->_id_ == second_mesh->_id_, "[MeshDiff] meshes have different ids: %llu != %llu", first_mesh->_id_, second_mesh->_id_);
        
        _id_ = second_mesh->_id_;
        _version = second_mesh->_version;
        
        if (not (first_mesh->frame == second_mesh->frame)) frame = second_mesh->frame;

        //check vertex difference
        map<timestamp_t, pair<int,set<timestamp_t>>> diffVertex;
        
        if(!geometric_only){
            //  removed vertex
            set_difference(first_mesh->vertices.begin(), first_mesh->vertices.end(),
                           second_mesh->vertices.begin(), second_mesh->vertices.end(),
                           insert_iterator<map<timestamp_t, pair<int,set<timestamp_t>>>>(diffVertex, diffVertex.end()),
                           [=](const map<timestamp_t,pair<int,set<timestamp_t>>>::value_type  & a, const map<timestamp_t,pair<int,set<timestamp_t>>>::value_type & b)
                           { return a.first < b.first; }
                           );
            for( auto it = diffVertex.begin(); it != diffVertex.end(); it++)
                remove_vertex.push_back(it->first);
        
            //  added vertex & pos
            diffVertex.clear();
            set_difference(second_mesh->vertices.begin(), second_mesh->vertices.end(),
                           first_mesh->vertices.begin(), first_mesh->vertices.end(),
                           insert_iterator<map<timestamp_t,pair<int,set<timestamp_t>>>>(diffVertex, diffVertex.end()),
                           [=](const map<timestamp_t,pair<int,set<timestamp_t>>>::value_type  & a, const map<timestamp_t,pair<int,set<timestamp_t>>>::value_type & b)
                           { return a.first < b.first; }
                           );
            for( auto it = diffVertex.begin(); it != diffVertex.end(); it++)
                add_vertex.emplace(it->first, second_mesh->pos[it->second.first]);
        
            diffVertex.clear();
        }
        
        // vertex pos update
        set_intersection(second_mesh->vertices.begin(), second_mesh->vertices.end(),
                         first_mesh->vertices.begin(), first_mesh->vertices.end(),
                         insert_iterator<map<timestamp_t,pair<int,set<timestamp_t>>>>(diffVertex, diffVertex.end()),
                         [=](const map<timestamp_t,pair<int,set<timestamp_t>>>::value_type  & a, const map<timestamp_t,pair<int,set<timestamp_t>>>::value_type & b)
                         {   if (a.first == b.first && second_mesh->pos[a.second.first] == first_mesh->pos[b.second.first]) return true; return a.first < b.first; }
                         );
        for( auto it = diffVertex.begin(); it != diffVertex.end(); it++)
            update_vertex.emplace(it->first, second_mesh->pos[it->second.first]);

        diffVertex.clear();

        if(!geometric_only){
            //check edges difference
            //  removed edges
            map<timestamp_t,pair<int,vec2id>> diffEdges;
            set_difference(first_mesh->edge.begin(), first_mesh->edge.end(),
                           second_mesh->edge.begin(), second_mesh->edge.end(),
                           insert_iterator<map<timestamp_t,pair<int,vec2id>>>(diffEdges, diffEdges.end()),
                           [](const map<timestamp_t,pair<int,vec2id>>::value_type  & a, const map<timestamp_t,pair<int,vec2id>>::value_type & b)
                           { return a.first < b.first; }
                           );
            for( auto it = diffEdges.begin(); it != diffEdges.end(); it++)
                remove_edge.push_back(it->first);
            diffEdges.clear();
        
            //  added edges & vert id
            set_difference(second_mesh->edge.begin(), second_mesh->edge.end(),
                           first_mesh->edge.begin(), first_mesh->edge.end(),
                           insert_iterator<map<timestamp_t,pair<int,vec2id>>>(diffEdges, diffEdges.end()),
                           [](const map<timestamp_t,pair<int,vec2id>>::value_type  & a, const map<timestamp_t,pair<int,vec2id>>::value_type & b)
                           { return a.first < b.first; }
                           );
        
            for( auto it = diffEdges.begin(); it != diffEdges.end(); it++)
                add_edge.emplace(it->first,it->second.second);
            diffEdges.clear();
            
            //check triangle difference
            //  removed triangle
            map<timestamp_t,tuple<int,vec3id,vec3f>> diffTriangle;
            set_difference(first_mesh->triangle.begin(), first_mesh->triangle.end(),
                           second_mesh->triangle.begin(), second_mesh->triangle.end(),
                           insert_iterator<map<timestamp_t,tuple<int,vec3id,vec3f>>>(diffTriangle, diffTriangle.end()),
                           [](const map<timestamp_t,tuple<int,vec3id,vec3f>>::value_type  & a, const map<timestamp_t,tuple<int,vec3id,vec3f>>::value_type & b)
                           { return a.first < b.first; }
                           );
            for( auto it = diffTriangle.begin(); it != diffTriangle.end(); it++)
                remove_triangle.push_back(it->first);
            diffTriangle.clear();
        
            //  added triangle & vert id
            set_difference(second_mesh->triangle.begin(), second_mesh->triangle.end(),
                           first_mesh->triangle.begin(), first_mesh->triangle.end(),
                           insert_iterator<map<timestamp_t,tuple<int,vec3id,vec3f>>>(diffTriangle, diffTriangle.end()),
                           [](const map<timestamp_t,tuple<int,vec3id,vec3f>>::value_type  & a, const map<timestamp_t,tuple<int,vec3id,vec3f>>::value_type & b)
                           { return a.first < b.first; }
                           );
            for( auto it = diffTriangle.begin(); it != diffTriangle.end(); it++)
                add_triangle.emplace(it->first,get<1>(it->second));
            diffTriangle.clear();
        
            //check quad difference
            //  removed quad
            map<timestamp_t,tuple<int,vec4id,vec3f>> diffQuad;
            set_difference(first_mesh->quad.begin(), first_mesh->quad.end(),
                           second_mesh->quad.begin(), second_mesh->quad.end(),
                           insert_iterator<map<timestamp_t,tuple<int,vec4id,vec3f>>>(diffQuad, diffQuad.end()),
                           [](const map<timestamp_t,tuple<int,vec4id,vec3f>>::value_type  & a, const map<timestamp_t,tuple<int,vec4id,vec3f>>::value_type & b)
                           { return a.first < b.first; }
                           );
            for( auto it = diffQuad.begin(); it != diffQuad.end(); it++)
                remove_quad.push_back(it->first);
            diffQuad.clear();
            
            //  added quad & vert id
            set_difference(second_mesh->quad.begin(), second_mesh->quad.end(),
                           first_mesh->quad.begin(), first_mesh->quad.end(),
                           insert_iterator<map<timestamp_t,tuple<int,vec4id,vec3f>>>(diffQuad, diffQuad.end()),
                           [](const map<timestamp_t,tuple<int,vec4id,vec3f>>::value_type  & a, const map<timestamp_t,tuple<int,vec4id,vec3f>>::value_type & b)
                           { return a.first < b.first; }
                           );
            for( auto it = diffQuad.begin(); it != diffQuad.end(); it++)
                add_quad.emplace(it->first,get<1>(it->second));
            diffQuad.clear();
        }
    }
    

};

// not yet completed (TO DO: implement elements diff by id)
struct SceneDiff {
    vector<CameraDiff*>      cameras;
    vector<LightDiff*>       lights;
    vector<MaterialDiff*>    materials;
    vector<MeshDiff*>        meshes;
    
    timestamp_t     _label = 0;
    
    // constructor
    SceneDiff() {}
    
    // copy constructor
    SceneDiff(const SceneDiff& scenediff){
        cameras = scenediff.cameras;
        lights = scenediff.lights;
        materials = scenediff.materials;
        meshes = scenediff.meshes;
        _label = scenediff._label;
    }
    
    // constructor from 2 scenes
    SceneDiff(const Scene* first_scene, const Scene* second_scene){
        cameras.push_back( new CameraDiff(first_scene->camera, second_scene->camera) );
        lights.push_back( new LightDiff(first_scene->lights[0], second_scene->lights[0]));
        materials.push_back(new MaterialDiff(first_scene->materials[0], second_scene->materials[0]));
    }
};

// grab all scene textures
vector<image3f*> get_textures(Scene* scene);

// create a Camera at eye, pointing towards center with up vector up, and with specified image plane params
//Camera* lookat_camera(vec3f eye, vec3f center, vec3f up, float width, float height, float dist);
Camera* lookat_camera(vec3f eye, vec3f center, vec3f up, float width, float height, float dist, timestamp_t id, int version);

// set camera view with a "turntable" modification
void set_view_turntable(Camera* camera, float rotate_phi, float rotate_theta, float dolly, float pan_x, float pan_y);

// find material by id
Material* find_material(vector<Material*> materials,timestamp_t id);

// load a scene from a json file
Scene* load_json_scene(const string& filename);

void load_json_objects(vector<Light*>* lights, vector<Material*>* materials, vector<Camera*>* cameras, vector<Mesh*>* meshes, const string& filename);

// force indexing of vertex position
void indexing_vertex_position(Mesh* mesh);

// compute edge&faces indexing and normal value per pixel
//void init_mesh_properties_from_map(Mesh* mesh, bool force_quad_update, bool force_triangle_update, bool force_edges_update, bool force_norm_update);
timestamp_t get_global_version();

void restore_to_version(Scene* scene, timestamp_t version);
timestamp_t restore_version(Scene* scene);

MeshDiff* meshdiff_assume_ordered( Mesh* first_mesh, Mesh* second_mesh);
void obj_apply_mesh_change(Mesh* mesh, MeshDiff* meshdiff, timestamp_t save_history);


// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)

void apply_mesh_change(Mesh* mesh, MeshDiff* meshdiff, timestamp_t save_history);

void apply_camera_change(Camera* camera, CameraDiff* cameradiff, timestamp_t save_history);

void apply_light_change(Light* light, LightDiff* lightdiff, timestamp_t save_history);

void apply_material_change(Material* material, MaterialDiff* matdiff, timestamp_t save_history);

// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_change(Scene* scene, SceneDiff* scenediff, timestamp_t save_history);

void apply_change_reverse(Scene* scene, SceneDiff* scenediff, timestamp_t save_history);

// swap 2 mesh
void swap_mesh(Mesh* mesh, Scene* scene, bool save_history);

// put in timing log
void timing(const string& s);
void timing(const string& s, long long i);

// save timing log
void save_timing(const string& s);

// put in timing log
void timing_apply(const string& s);
void timing_apply(const string& s, long long i);

// save timing log
void save_timing_apply(const string& s);

#endif

