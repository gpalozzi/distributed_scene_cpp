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
    vec3f       ke = zero3f;        // emission coefficient
    vec3f       kd = one3f;         // diffuse coefficient
    vec3f       ks = zero3f;        // specular coefficient
    float       n = 10;             // specular exponent
    vec3f       kr = zero3f;        // reflection coefficient
    
    image3f*    ke_txt = nullptr;   // emission texture
    image3f*    kd_txt = nullptr;   // diffuse texture
    image3f*    ks_txt = nullptr;   // specular texture
    image3f*    kr_txt = nullptr;   // reflection texture
    image3f*    norm_txt = nullptr; // normal texture
    
    bool        double_sided = false;   // double-sided material
    bool        microfacet = false; // use microfacet formulation
    
    timestamp_t         _id_ = 0;                      // unique id
    
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
    //vector<timestamp_t> vertex_ids;             // vertex ids
    //map<timestamp_t,int> vertex_id_map;
    map<timestamp_t, pair<int,vector<timestamp_t>>> vertex_id_map;
    vector<vec3f>   pos;                        // vertex position
    
    vector<timestamp_t> normal_ids;             // vertex normal ids
    vector<vec3f>   norm;                       // vertex normal
    
    //vector<timestamp_t> texcoord_ids;           // vertex texture coordinates ids
    vector<vec2f>   texcoord;                   // vertex texcture coordinates
    
    //vector<timestamp_t> triangle_ids;           // triangle faces ids
//    map<timestamp_t,vec3id>   triangle;                  // triangle
    map<timestamp_t,tuple<int,vec3id,vec3f>>   triangle;         // triangle
    vector<vec3i>   triangle_index;             // triangle index
    
    //vector<timestamp_t> quad_ids;               // quad faces ids
    map<timestamp_t,tuple<int,vec4id,vec3f>>   quad;                      // quad
    vector<vec4i>   quad_index;                 // quad index
    
    //vector<timestamp_t> edge_ids;               // edges ids
//    map<timestamp_t,vec2id>   edge;                      // edges
    map<timestamp_t,tuple<int,vec2id>>   edge;                      // edges
    vector<vec2i>   edge_index;                 // edges index
    
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
    
    timestamp_t         _id_ = 0;                      // unique id
    int                 _version = -1;      // mesh version for shuttle
    // constructor
    Mesh() {}
    
    // copy constructor
    Mesh(const Mesh& mesh){
        frame = mesh.frame;
        //vertex_ids = mesh.vertex_ids;
        pos = mesh.pos;
        normal_ids = mesh.normal_ids;
        norm = mesh.norm;
        //texcoord_ids = mesh.texcoord_ids;
        texcoord = mesh.texcoord;
        //triangle_ids = mesh.triangle_ids;
        triangle = mesh.triangle;
        triangle_index = mesh.triangle_index;
        //quad_ids = mesh.quad_ids;
        quad = mesh.quad;
        quad_index = mesh.quad_index;
        //edge_ids = mesh.edge_ids;
        edge = mesh.edge;
        edge_index = mesh.edge_index;
        vertex_id_map = mesh.vertex_id_map;
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
    
    timestamp_t         _id_ = 0;                      // unique id
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
    
    timestamp_t         _id_ = 0;                      // unique id
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

struct SubMesh {
    // remove element
    vector<timestamp_t> remove_vertex;
    vector<timestamp_t> remove_edge;
    vector<timestamp_t> remove_triangle;
    vector<timestamp_t> remove_quad;
    // add element
    map<timestamp_t, vec3f> add_vertex;
    map<timestamp_t, vec2id> add_edge;
    map<timestamp_t, vec3id> add_triangle;
    map<timestamp_t, vec4id> add_quad;
    // update element
    map<timestamp_t, vec3f> update_vertex;
    
    timestamp_t             _id_ = 0;
    int                     _version = -1;
    
    // constructor
    SubMesh() {}
    
    // copy constructor
    SubMesh(const SubMesh& submesh){
        remove_vertex = submesh.remove_vertex;
        remove_edge = submesh.remove_edge;
        remove_triangle = submesh.remove_triangle;
        remove_quad = submesh.remove_quad;
        add_vertex = submesh.add_vertex;
        add_edge = submesh.add_edge;
        add_triangle = submesh.add_triangle;
        update_vertex = submesh.update_vertex;
        _id_ = submesh._id_;
        _version = submesh._version;
    }
    
    // constructor from 2 mesh
    SubMesh(const Mesh* first_mesh,const Mesh* second_mesh){
        //check vertex difference
        //  removed vertex
        map<timestamp_t,int> diffVertex;
        set_difference(first_mesh->vertex_id_map.begin(), first_mesh->vertex_id_map.end(),
                       second_mesh->vertex_id_map.begin(), second_mesh->vertex_id_map.end(),
                       insert_iterator<map<timestamp_t,int>>(diffVertex, diffVertex.end()),
                       [](const map<timestamp_t,int>::value_type  & a, const map<timestamp_t,int>::value_type & b)
                       { return a.first < b.first; }
                       );
        for( auto it = diffVertex.begin(); it != diffVertex.end(); it++)
            remove_vertex.push_back(it->first);
        
        //  added vertex & pos
        diffVertex.clear();
        set_difference(second_mesh->vertex_id_map.begin(), second_mesh->vertex_id_map.end(),
                       first_mesh->vertex_id_map.begin(), first_mesh->vertex_id_map.end(),
                       insert_iterator<map<timestamp_t,int>>(diffVertex, diffVertex.end()),
                       [](const map<timestamp_t,int>::value_type  & a, const map<timestamp_t,int>::value_type & b)
                       { return a.first < b.first; }
                       );
        for( auto it = diffVertex.begin(); it != diffVertex.end(); it++)
            add_vertex.emplace(it->first, second_mesh->pos[it->second]);
        
        // vertex pos update
        diffVertex.clear();
        set_intersection(second_mesh->vertex_id_map.begin(), second_mesh->vertex_id_map.end(),
                         first_mesh->vertex_id_map.begin(), first_mesh->vertex_id_map.end(),
                         insert_iterator<map<timestamp_t,int>>(diffVertex, diffVertex.end()),
                         [=](const map<timestamp_t,int>::value_type  & a, const map<timestamp_t,int>::value_type & b)
                         {   if (a.first == b.first && second_mesh->pos[a.second] == first_mesh->pos[b.second]) return true; return a.first < b.first; }
                         );
        for( auto it = diffVertex.begin(); it != diffVertex.end(); it++)
            update_vertex.emplace(it->first, second_mesh->pos[it->second]);
        
        //check edges difference
        //  removed edges
        diffVertex.clear();
        map<timestamp_t,vec2id> diffEdges;
        set_difference(first_mesh->edge.begin(), first_mesh->edge.end(),
                       second_mesh->edge.begin(), second_mesh->edge.end(),
                       insert_iterator<map<timestamp_t,vec2id>>(diffEdges, diffEdges.end()),
                       [](const map<timestamp_t,vec2id>::value_type  & a, const map<timestamp_t,vec2id>::value_type & b)
                       { return a.first < b.first; }
                       );
        for( auto it = diffEdges.begin(); it != diffEdges.end(); it++)
            remove_edge.push_back(it->first);
        diffEdges.clear();
        
        //  added edges & vert id
        set_difference(second_mesh->edge.begin(), second_mesh->edge.end(),
                       first_mesh->edge.begin(), first_mesh->edge.end(),
                       insert_iterator<map<timestamp_t,vec2id>>(add_edge, add_edge.end()),
                       [](const map<timestamp_t,vec2id>::value_type  & a, const map<timestamp_t,vec2id>::value_type & b)
                       { return a.first < b.first; }
                       );
        
        //check triangle difference
        //  removed triangle
        map<timestamp_t,vec3id> diffTriangle;
        set_difference(first_mesh->triangle.begin(), first_mesh->triangle.end(),
                       second_mesh->triangle.begin(), second_mesh->triangle.end(),
                       insert_iterator<map<timestamp_t,vec3id>>(diffTriangle, diffTriangle.end()),
                       [](const map<timestamp_t,vec3id>::value_type  & a, const map<timestamp_t,vec3id>::value_type & b)
                       { return a.first < b.first; }
                       );
        for( auto it = diffTriangle.begin(); it != diffTriangle.end(); it++)
            remove_triangle.push_back(it->first);
        diffTriangle.clear();
        
        //  added triangle & vert id
        set_difference(second_mesh->triangle.begin(), second_mesh->triangle.end(),
                       first_mesh->triangle.begin(), first_mesh->triangle.end(),
                       insert_iterator<map<timestamp_t,vec3id>>(add_triangle, add_triangle.end()),
                       [](const map<timestamp_t,vec3id>::value_type  & a, const map<timestamp_t,vec3id>::value_type & b)
                       { return a.first < b.first; }
                       );
        
        //check quad difference
        //  removed quad
        map<timestamp_t,vec4id> diffQuad;
        set_difference(first_mesh->quad.begin(), first_mesh->quad.end(),
                       second_mesh->quad.begin(), second_mesh->quad.end(),
                       insert_iterator<map<timestamp_t,vec4id>>(diffQuad, diffQuad.end()),
                       [](const map<timestamp_t,vec4id>::value_type  & a, const map<timestamp_t,vec4id>::value_type & b)
                       { return a.first < b.first; }
                       );
        for( auto it = diffQuad.begin(); it != diffQuad.end(); it++)
            remove_quad.push_back(it->first);
        diffQuad.clear();
        
        //  added quad & vert id
        set_difference(second_mesh->quad.begin(), second_mesh->quad.end(),
                       first_mesh->quad.begin(), first_mesh->quad.end(),
                       insert_iterator<map<timestamp_t,vec4id>>(add_quad, add_quad.end()),
                       [](const map<timestamp_t,vec4id>::value_type  & a, const map<timestamp_t,vec4id>::value_type & b)
                       { return a.first < b.first; }
                       );
        
        _id_ = second_mesh->_id_;
        _version = second_mesh->_version;
    }
    
};


// grab all scene textures
vector<image3f*> get_textures(Scene* scene);

// create a Camera at eye, pointing towards center with up vector up, and with specified image plane params
Camera* lookat_camera(vec3f eye, vec3f center, vec3f up, float width, float height, float dist);

// set camera view with a "turntable" modification
void set_view_turntable(Camera* camera, float rotate_phi, float rotate_theta, float dolly, float pan_x, float pan_y);

// load a scene from a json file
Scene* load_json_scene(const string& filename);

// force indexing of vertex position
void indexing_vertex_position(Mesh* mesh);

// compute edge&faces indexing and normal value per pixel
void init_mesh_properties_from_map(Mesh* mesh, bool force_quad_update, bool force_triangle_update, bool force_edges_update, bool force_norm_update);

Mesh* get_mesh_to_restrore();

// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_changes(Mesh* mesh, SubMesh* submesh, bool save_history);
// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_changes_range(Mesh* mesh, SubMesh* submesh, bool save_history);

// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_changes_check(Mesh* mesh, Mesh* real_mesh, SubMesh* submesh, bool save_history);

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

