#include "scene_distributed.h"
#include "serialization.hpp"
#include "id_reference.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/map.hpp>

map<string, long long> timing_log;
map<string, long long> apply_log;


void timing(const string& s){
    std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
    timing_log[s] = time.time_since_epoch().count();
}

void timing(const string& s, long long i){
    timing_log[s] = i;
};

// save timing log
void save_timing(const string& s){
    string filename = "../log/" + s;
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << timing_log;
};

void timing_apply(const string& s){
    std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
    apply_log[s] = time.time_since_epoch().count();
}

void timing_apply(const string& s, long long i){
    apply_log[s] = i;
};

// save timing log
void save_timing_apply(const string& s){
    string filename = "../log/" + s;
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << apply_log;
};

// reference to solve
vector<id_reference*> ref_to_solve;

vector<image3f*> get_textures(Scene* scene) {
    auto textures = set<image3f*>();
    for(auto mesh : scene->meshes) {
        if(mesh->mat->ke_txt) textures.insert(mesh->mat->ke_txt);
        if(mesh->mat->kd_txt) textures.insert(mesh->mat->kd_txt);
        if(mesh->mat->ks_txt) textures.insert(mesh->mat->ks_txt);
        if(mesh->mat->norm_txt) textures.insert(mesh->mat->norm_txt);
    }
    for(auto surface : scene->surfaces) {
        if(surface->mat->ke_txt) textures.insert(surface->mat->ke_txt);
        if(surface->mat->kd_txt) textures.insert(surface->mat->kd_txt);
        if(surface->mat->ks_txt) textures.insert(surface->mat->ks_txt);
        if(surface->mat->norm_txt) textures.insert(surface->mat->norm_txt);
    }
    if(scene->background_txt) textures.insert(scene->background_txt);
    return vector<image3f*>(textures.begin(),textures.end());
}

map<timestamp_t,Mesh*> mesh_history;
map<timestamp_t,MeshDiff*> meshdiff_history;
map<timestamp_t,CameraDiff*> cameradiff_history;
map<timestamp_t,LightDiff*> lightdiff_history;
map<timestamp_t,MaterialDiff*> materialdiff_history;
vector<pair<timestamp_t,SceneDiff*>> scenediff_history;
vector<pair<timestamp_t,SceneDiff*>> scenediff_history_reverse;
timestamp_t global_scene_version = 0; // 0 is base version

timestamp_t get_global_version(){
    return global_scene_version;
}

void restore_backward(Scene* scene, int previous, int current){
    for (auto i = current; i >= previous; i--)
        apply_change(scene, scenediff_history_reverse[i].second, 0);
    
    if (previous == 0) global_scene_version = 0;
    else global_scene_version = scenediff_history_reverse[previous-1].first;
}

void restore_forward(Scene* scene, int succ, int current){
    for (auto i = current; i <= succ; i++)
        apply_change(scene, scenediff_history[i].second, 0);
    
    global_scene_version = scenediff_history[succ].first;
}

void restore_to_version(Scene* scene, timestamp_t version){
    int current = -2, to = -2, i = scenediff_history_reverse.size()-1;
    if(global_scene_version == 0) current = -1;
    if(version == 0) to = -1;
    while (i >= 0 and (current == -2 or to == -2)) {
        if (current == -2 and scenediff_history_reverse[i].first == global_scene_version) current = i;
        if (to == -2 and scenediff_history_reverse[i].first == version) to = i;
        i--;
    }
    
    error_if_not(current != -2 and to != -2, "error in restore_to_version, can't find version\n");
    
    if (current > to){
        // undo
        restore_backward(scene, to+1, current);

    }
    else if(current < to){
        // redo
        restore_forward(scene, to, current+1);
    }
    else if (to == current) message("You already are in the version\n");

}

timestamp_t restore_version(Scene* scene){
    cout << "[0] base version";
    if (global_scene_version == 0) cout << " <- current";
    cout << endl;

    int choice = 0, current = 0;
    for(; choice < scenediff_history.size(); choice++){
        cout << "[" << choice+1 << "] " << scenediff_history[choice].first;
        if (scenediff_history[choice].first == global_scene_version){
            cout << " <- current";
            current = choice+1;
        }
        cout << endl;
    }
    message("Version to restore:\n");
    cin >> choice;
    
    if (choice < current and choice > -1) {
        // undo
        restore_backward(scene, choice, current-1);
    }
    else if (choice > current and choice <= scenediff_history.size()){
        // redo
        restore_forward(scene, choice-1, current);
    }
    else if (choice == current){ message("You are in the current version\n");
    }
    
    return global_scene_version;
}


Camera* lookat_camera(vec3f eye, vec3f center, vec3f up, float width, float height, float dist, timestamp_t id, int version) {
    auto camera = new Camera();
    camera->_id_ = id;
    camera->_version = version;
    camera->frame = lookat_frame(eye, center, up, true);
    camera->width = width;
    camera->height = height;
    camera->dist = dist;
    camera->focus = length(eye-center);
    return camera;
}

void set_view_turntable(Camera* camera, float rotate_phi, float rotate_theta, float dolly, float pan_x, float pan_y) {
    auto phi = atan2(camera->frame.z.z,camera->frame.z.x) + rotate_phi;
    auto theta = clamp(acos(camera->frame.z.y) + rotate_theta, 0.001f,pif-0.001f);
    auto new_z = vec3f(sin(theta)*cos(phi),cos(theta),sin(theta)*sin(phi));
    auto new_center = camera->frame.o-camera->frame.z*camera->focus;
    auto new_o = new_center + new_z * camera->focus;
    camera->frame = lookat_frame(new_o,new_center,y3f,true);
    camera->focus = dist(new_o,new_center);
    
    // apply dolly
    vec3f c = camera->frame.o - camera->frame.z * camera->focus;
    camera->focus = max(camera->focus+dolly,0.00001f);
    camera->frame.o = c + camera->frame.z * camera->focus;
    
    // apply pan
    camera->frame.o += camera->frame.x * pan_x + camera->frame.y * pan_y;
}

void json_set_values(const jsonvalue& json, float* value, int n) {
    error_if_not(n == json.array_size(), "incorrect array size");
    for(auto i : range(n)) value[i] = json.array_element(i).as_float();
}
void json_set_values(const jsonvalue& json, int* value, int n) {
    error_if_not(n == json.array_size(), "incorrect array size");
    for(auto i : range(n)) value[i] = json.array_element(i).as_int();
}
void json_set_values(const jsonvalue& json, timestamp_t* value, int n) {
    error_if_not(n == json.array_size(), "incorrect array size");
    for(auto i : range(n)) value[i] = (timestamp_t)json.array_element(i).as_id();
}
void json_set_value(const jsonvalue& json, bool& value) { value = json.as_bool(); }
void json_set_value(const jsonvalue& json, int& value) { value = json.as_int(); }
void json_set_value(const jsonvalue& json, float& value) { value = json.as_double(); }
void json_set_value(const jsonvalue& json, timestamp_t& value) { value = json.as_id(); }
void json_set_value(const jsonvalue& json, vec2f& value) { json_set_values(json, &value.x, 2); }
void json_set_value(const jsonvalue& json, vec3f& value) { json_set_values(json, &value.x, 3); }
void json_set_value(const jsonvalue& json, vec4f& value) { json_set_values(json, &value.x, 4); }
void json_set_value(const jsonvalue& json, vec2i& value) { json_set_values(json, &value.x, 2); }
void json_set_value(const jsonvalue& json, vec3i& value) { json_set_values(json, &value.x, 3); }
void json_set_value(const jsonvalue& json, vec4i& value) { json_set_values(json, &value.x, 4); }
void json_set_value(const jsonvalue& json, vec2id& value) { json_set_values(json, &value.first, 2); }
void json_set_value(const jsonvalue& json, vec3id& value) { json_set_values(json, &value.first, 3); }
void json_set_value(const jsonvalue& json, vec4id& value) { json_set_values(json, &value.first, 4); }
void json_set_value(const jsonvalue& json, vector<bool>& value) {
    value.resize(json.array_size());
    for(auto i : range(value.size())) value[i] = json.array_element(i).as_bool();
}
void json_set_value(const jsonvalue& json, vector<float>& value) { value.resize(json.array_size()); json_set_values(json, &value[0], value.size()); }
void json_set_value(const jsonvalue& json, vector<vec2f>& value) { value.resize(json.array_size()/2); json_set_values(json, &value[0].x, value.size()*2); }
void json_set_value(const jsonvalue& json, vector<vec3f>& value) { value.resize(json.array_size()/3); json_set_values(json, &value[0].x, value.size()*3); }
void json_set_value(const jsonvalue& json, vector<vec4f>& value) { value.resize(json.array_size()/4); json_set_values(json, &value[0].x, value.size()*4); }
void json_set_value(const jsonvalue& json, vector<int>& value) { value.resize(json.array_size()); json_set_values(json, &value[0], value.size()); }
void json_set_value(const jsonvalue& json, vector<vec2i>& value) { value.resize(json.array_size()/2); json_set_values(json, &value[0].x, value.size()*2); }
void json_set_value(const jsonvalue& json, vector<vec3i>& value) { value.resize(json.array_size()/3); json_set_values(json, &value[0].x, value.size()*3); }
void json_set_value(const jsonvalue& json, vector<vec4i>& value) { value.resize(json.array_size()/4); json_set_values(json, &value[0].x, value.size()*4); }
void json_set_value(const jsonvalue& json, vector<vec2id>& value) { value.resize(json.array_size()/2); json_set_values(json, &value[0].first, value.size()*2); }
void json_set_value(const jsonvalue& json, vector<vec3id>& value) { value.resize(json.array_size()/3); json_set_values(json, &value[0].first, value.size()*3); }
void json_set_value(const jsonvalue& json, vector<vec4id>& value) { value.resize(json.array_size()/4); json_set_values(json, &value[0].first, value.size()*4); }
void json_set_value(const jsonvalue& json, vector<mat4f>& value) { value.resize(json.array_size()/16); json_set_values(json, &value[0].x.x, value.size()*16); }
void json_set_value(const jsonvalue& json, frame3f& value) {
    value = identity_frame3f;
    if(json.object_contains("frame_from") or json.object_contains("frame_to") or json.object_contains("frame_up")) {
        auto from = z3f, to = zero3f, up = y3f;
        if(json.object_contains("frame_from")) json_set_value(json.object_element("frame_from"), from);
        if(json.object_contains("frame_to")) json_set_value(json.object_element("frame_to"), to);
        if(json.object_contains("frame_up")) json_set_value(json.object_element("frame_up"), up);
        value = lookat_frame(from, to, up);
    } else {
        if(json.object_contains("frame_o")) json_set_value(json.object_element("frame_o"), value.o);
        if(json.object_contains("frame_x")) json_set_value(json.object_element("frame_x"), value.x);
        if(json.object_contains("frame_y")) json_set_value(json.object_element("frame_y"), value.y);
        if(json.object_contains("frame_z")) json_set_value(json.object_element("frame_z"), value.z);
        value = orthonormalize_zyx(value);
    }
}

void json_set_value(const jsonvalue& json, vector<vector<mat4f>>& value) {
    value.resize(json.array_size());
    for(auto i : range(value.size())) json_set_value(json.array_element(i), value[i]);
}

template<typename T>
void json_set_optvalue(const jsonvalue& json, T& value, const string& name) {
    if(not json.object_contains(name)) return;
    json_set_value(json.object_element(name), value);
}

template<typename T>
void add_reference(T& value, const jsonvalue& json) {
    timestamp_t id = 0;
    json_set_value(json, id);
    ref_to_solve.push_back(new id_reference(&value, id));
}

Camera* json_parse_camera(const jsonvalue& json) {
    auto camera = new Camera();
    json_set_optvalue(json, camera->_id_, "_id_");
    json_set_optvalue(json, camera->_version, "version");
    if(json.object_contains("frame_from") or json.object_contains("frame_to") or json.object_contains("frame_up")) {
        auto from = z3f, to = zero3f, up = y3f;
        json_set_optvalue(json, from, "frame_from");
        json_set_optvalue(json, to, "frame_to");
        json_set_optvalue(json, up, "frame_up");
        camera->frame = lookat_frame(from, to, up);
    } else {
        json_set_optvalue(json, camera->frame.o, "frame_o");
        json_set_optvalue(json, camera->frame.x, "frame_x");
        json_set_optvalue(json, camera->frame.y, "frame_y");
        json_set_optvalue(json, camera->frame.z, "frame_z");
        camera->frame = orthonormalize_zyx(camera->frame);
    }
    json_set_optvalue(json, camera->width, "width");
    json_set_optvalue(json, camera->height, "height");
    json_set_optvalue(json, camera->focus, "focus");
    json_set_optvalue(json, camera->dist, "dist");
    return camera;
}

Camera* json_parse_lookatcamera(const jsonvalue& json) {
    auto from = z3f, to = zero3f, up = y3f;
    auto width = 1.0f, height = 1.0f, dist = 1.0f;
    timestamp_t id = 0;
    int version = 0;
    json_set_optvalue(json, id, "_id_");
    json_set_optvalue(json, version, "version");
    json_set_optvalue(json, from, "from");
    json_set_optvalue(json, to, "to");
    json_set_optvalue(json, up, "up");
    json_set_optvalue(json, width, "width");
    json_set_optvalue(json, height, "height");
    json_set_optvalue(json, dist, "dist");
    return lookat_camera(from, to, up, width, height, dist, id, version);
}

vector<string>          json_texture_paths;
map<string,image3f*>    json_texture_cache;

void json_texture_path_push(string filename) {
    auto pos = filename.rfind("/");
    auto dirname = (pos == string::npos) ? string() : filename.substr(0,pos+1);
    json_texture_paths.push_back(dirname);
}
void json_texture_path_pop() { json_texture_paths.pop_back(); }

void json_parse_opttexture(jsonvalue json, image3f*& txt, string name) {
    if(not json.object_contains(name)) return;
    auto filename = json.object_element(name).as_string();
    if(filename.empty()) { txt = nullptr; return; }
    auto dirname = json_texture_paths.back();
    auto fullname = dirname + filename;
    if (json_texture_cache.find(fullname) == json_texture_cache.end()) {
        auto ext = fullname.substr(fullname.size()-3);
        if(ext == "pfm") {
            auto image = read_pnm("models/pisa_latlong.pfm", true);
            image = image.gamma(1/2.2);
            json_texture_cache[fullname] = new image3f(image);
        } else if(ext == "png") {
            auto image = read_png(fullname,true);
            json_texture_cache[fullname] = new image3f(image);
        } else error("unsupported image format %s\n", ext.c_str());
    }
    txt = json_texture_cache[fullname];
}

Material* json_parse_material(const jsonvalue& json) {
    auto material = new Material();
    json_set_optvalue(json, material->_id_, "_id_");
    json_set_optvalue(json, material->_version, "version");
    json_set_optvalue(json, material->ke, "ke");
    json_set_optvalue(json, material->kd, "kd");
    json_set_optvalue(json, material->ks, "ks");
    json_set_optvalue(json, material->kr, "kr");
    json_set_optvalue(json, material->n, "n");
    json_set_optvalue(json, material->microfacet, "microfacet");
    json_parse_opttexture(json, material->ke_txt, "ke_txt");
    json_parse_opttexture(json, material->kd_txt, "kd_txt");
    json_parse_opttexture(json, material->ks_txt, "ks_txt");
    json_parse_opttexture(json, material->norm_txt, "norm_txt");
    return material;
}

FrameAnimation* json_parse_frame_animation(const jsonvalue& json) {
    auto animation = new FrameAnimation();
    json_set_optvalue(json, animation->rest_frame, "rest_frame");
    json_set_optvalue(json, animation->keytimes, "keytimes");
    json_set_optvalue(json, animation->translation, "translation");
    json_set_optvalue(json, animation->rotation, "rotation");
    return animation;
}

Surface* json_parse_surface(const jsonvalue& json) {
    auto surface = new Surface();
    json_set_optvalue(json, surface->frame, "frame");
    json_set_optvalue(json, surface->radius,"radius");
    json_set_optvalue(json, surface->isquad,"isquad");
    if(json.object_contains("material")) surface->mat = json_parse_material(json.object_element("material"));
    if(json.object_contains("animation")) surface->animation = json_parse_frame_animation(json.object_element("animation"));
    return surface;
}

vector<Surface*> json_parse_surfaces(const jsonvalue& json) {
    auto surfaces = vector<Surface*>();
    for(auto value : json.as_array_ref())
        surfaces.push_back( json_parse_surface(value) );
    return surfaces;
}

MeshSkinning* json_parse_mesh_skinning(const jsonvalue& json) {
    auto skinning = new MeshSkinning();
    json_set_optvalue(json, skinning->rest_pos, "rest_pos");
    json_set_optvalue(json, skinning->rest_norm, "rest_norm");
    json_set_optvalue(json, skinning->bone_ids, "bone_ids");
    json_set_optvalue(json, skinning->bone_weights, "bone_weights");
    json_set_optvalue(json, skinning->bone_xforms, "bone_xforms");
    return skinning;
}

MeshSimulation* json_parse_mesh_simulation(const jsonvalue& json) {
    auto simulation = new MeshSimulation();
    json_set_optvalue(json, simulation->init_pos, "init_pos");
    json_set_optvalue(json, simulation->init_vel, "init_vel");
    json_set_optvalue(json, simulation->mass, "mass");
    json_set_optvalue(json, simulation->pinned, "pinned");
    json_set_optvalue(json, simulation->vel, "vel");
    json_set_optvalue(json, simulation->force, "force");
    if (json.object_contains("springs")) {
        for(auto& elem : json.object_element("springs").as_array_ref()) {
            auto spring = MeshSimulation::Spring();
            json_set_optvalue(elem, spring.ids, "ids");
            json_set_optvalue(elem, spring.restlength, "restlength");
            json_set_optvalue(elem, spring.ks, "ks");
            json_set_optvalue(elem, spring.kd, "kd");
            simulation->springs.push_back(spring);
        }
    }
    return simulation;
}

/*
// deprecated: too slow
void init_mesh_properties_from_array(Mesh* mesh){
    auto quad_indices = mesh->quad_index.empty();
    auto norm = mesh->norm.empty();
    vector<int> faces_per_vert;
    
    if (norm) {
        faces_per_vert = vector<int>(mesh->vertex_ids.size(),0);
        mesh->norm = vector<vec3f>(mesh->vertex_ids.size(),zero3f);
    }
    
    if (norm or quad_indices) {
        // for each quad
        for(auto quad : mesh->quad){
            // for each vertex find the id in vertices vector and get the index
            auto a = indexof(quad.first, mesh->vertex_ids);
            auto b = indexof(quad.second, mesh->vertex_ids);
            auto c = indexof(quad.third, mesh->vertex_ids);
            auto d = indexof(quad.fourth, mesh->vertex_ids);
            
            // put this 4 indices in vector
            if (quad_indices) mesh->quad_index.push_back({a,b,c,d});
            
            // compute normals
            if (norm){
                // face normal
                auto n = normalize(normalize(cross(mesh->pos[b]-mesh->pos[a], mesh->pos[c]-mesh->pos[a])) +
                                   normalize(cross(mesh->pos[c]-mesh->pos[a], mesh->pos[d]-mesh->pos[a])));
                // add to previous normals
                mesh->norm[a] += n; mesh->norm[b] += n; mesh->norm[c] += n; mesh->norm[d] += n;
                // increment number of faces involeved in computatation for normal
                faces_per_vert[a]++; faces_per_vert[b]++; faces_per_vert[c]++; faces_per_vert[d]++;
            }
        }
        
        // for each triangle
        for(auto triangle : mesh->triangle){
            // for each vertex find the id in vertices vector and get the index
            auto a = indexof(triangle.first, mesh->vertex_ids);
            auto b = indexof(triangle.second, mesh->vertex_ids);
            auto c = indexof(triangle.third, mesh->vertex_ids);
            
            // put this 3 indices in vector
            if (quad_indices) mesh->triangle_index.push_back({a,b,c});
            
            // compute normals
            if (norm){
                // face normal
                auto n = normalize(cross(mesh->pos[b]-mesh->pos[a], mesh->pos[c]-mesh->pos[a]));
                // add to previous normals
                mesh->norm[a] += n; mesh->norm[b] += n; mesh->norm[c] += n;
                // increment number of faces involeved in computatation for normal
                faces_per_vert[a]++; faces_per_vert[b]++; faces_per_vert[c]++;
            }
        }
        
        // apply mean to normals
        if(norm){
            for(auto i : range(mesh->norm.size()) ){ if(auto mean = faces_per_vert[i]) mesh->norm[i] /= mean; }
            // delete
            faces_per_vert.clear();
        }
    }
};
*/

//void init_mesh_properties_from_map(Mesh* mesh, bool force_quad_update, bool force_triangle_update, bool force_edges_update, bool force_norm_update){
//    auto quad_indices = mesh->quad_index.empty() or force_quad_update;
//    auto triangle_indices = mesh->triangle_index.empty() or force_triangle_update;
//    auto edges_indices = mesh->edge_index.empty() or force_edges_update;
//    auto norm = mesh->norm.empty() or force_norm_update;
//    
//    // clear previous vector
//    if (force_quad_update) mesh->quad_index.clear();
//    if (force_triangle_update) mesh->triangle_index.clear();
//    if (force_edges_update) mesh->edge_index.clear();
//    if (force_norm_update) mesh->norm.clear();
//
//    vector<int> faces_per_vert;
//    
//    if (norm) {
//        faces_per_vert = vector<int>(mesh->pos.size(),0);
//        mesh->norm = vector<vec3f>(mesh->pos.size(),zero3f);
//    }
//    
//    if (norm or quad_indices) {
//        // for each quad
//        for(auto quad : mesh->quad){
//            // for each vertex find the id in vertices vector and get the index
//            auto a = mesh->vertices[quad.second];
//            auto b = mesh->vertices[quad.second];
//            auto c = mesh->vertices[quad.second];
//            auto d = mesh->vertices[quad.second];
//            
//            // put this 4 indices in vector
//            if (quad_indices) mesh->quad_index.push_back({a,b,c,d});
//            
//            // compute normals
//            if (norm){
//                // face normal
//                auto n = normalize(normalize(cross(mesh->pos[b]-mesh->pos[a], mesh->pos[c]-mesh->pos[a])) +
//                                   normalize(cross(mesh->pos[c]-mesh->pos[a], mesh->pos[d]-mesh->pos[a])));
//                // add to previous normals
//                mesh->norm[a] += n; mesh->norm[b] += n; mesh->norm[c] += n; mesh->norm[d] += n;
//                // increment number of faces involeved in computatation for normal
//                faces_per_vert[a]++; faces_per_vert[b]++; faces_per_vert[c]++; faces_per_vert[d]++;
//            }
//        }
//    }
//    if (norm or triangle_indices) {
//        // for each triangle
//        for(auto triangle : mesh->triangle){
//            // for each vertex find the id in vertices vector and get the index
//            auto a = mesh->vertices[triangle.second.first];
//            auto b = mesh->vertices[triangle.second.second];
//            auto c = mesh->vertices[triangle.second.third];
//            
//            // put this 3 indices in vector
//            if (triangle_indices) mesh->triangle_index.push_back({a,b,c});
//            
//            // compute normals
//            if (norm){
//                // face normal
//                auto n = normalize(cross(mesh->pos[b]-mesh->pos[a], mesh->pos[c]-mesh->pos[a]));
//                // add to previous normals
//                mesh->norm[a] += n; mesh->norm[b] += n; mesh->norm[c] += n;
//                // increment number of faces involeved in computatation for normal
//                faces_per_vert[a]++; faces_per_vert[b]++; faces_per_vert[c]++;
//            }
//        }
//    }
//    // apply mean to normals
//    if(norm){
//        for(auto i : range(mesh->norm.size()) ){ if(auto mean = faces_per_vert[i]) mesh->norm[i] /= mean; }
//        // delete
//        faces_per_vert.clear();
//    }
//    if (edges_indices) {
//        // for each edge
//        for(auto edge : mesh->edge){
//            // for each vertex find the id in vertices vector and get the index
//            auto a = mesh->vertices[edge.second.first];
//            auto b = mesh->vertices[edge.second.second];
//            
//            // put this 2 indices in vector
//            mesh->edge_index.push_back({a,b});
//        }
//    }    
//}

void indexing_vertex_position(Mesh* mesh){
    vector<vec3f> temp = mesh->pos;
    vector<vec3f> temp_norm = mesh->norm;
    mesh->pos.clear();
    mesh->norm.clear();
    
    for (auto& vertex : mesh->vertices){
        // move normal if needed
        if (not temp_norm.empty()) mesh->norm.push_back(temp_norm[vertex.second.first]);
        else mesh->norm.push_back(zero3f);
        // move vertex position
        mesh->pos.push_back(temp[vertex.second.first]);
        vertex.second.first = mesh->pos.size()-1;
    }
    temp_norm.clear();
    temp.clear();
}

void indexing_triangle_position(Mesh* mesh){
    vector<vec3i> temp = mesh->triangle_index;
    mesh->triangle_index.clear();
    for (auto& triangle : mesh->triangle){
        // move index position
        mesh->triangle_index.push_back(temp[get<0>(triangle.second)]);
        get<0>(triangle.second) = mesh->triangle_index.size()-1;
    }
    temp.clear();
}

void indexing_quad_position(Mesh* mesh){
    vector<vec4i> temp = mesh->quad_index;
    mesh->quad_index.clear();
    for (auto& quad : mesh->quad){
        // move index position
        mesh->quad_index.push_back(temp[get<0>(quad.second)]);
        get<0>(quad.second) = mesh->quad_index.size()-1;
    }
    temp.clear();
}

void indexing_edge_position(Mesh* mesh){
    vector<vec2i> temp = mesh->edge_index;
    mesh->edge_index.clear();
    for (auto& edge : mesh->edge){
        // move index position
        mesh->edge_index.push_back(temp[get<0>(edge.second)]);
        get<0>(edge.second) = mesh->edge_index.size()-1;
    }
    temp.clear();
}


Mesh* json_parse_mesh(const jsonvalue& json) {
    auto mesh = new Mesh();
    json_set_optvalue(json, mesh->_id_, "_id_");
    json_set_optvalue(json, mesh->_version, "version");
    if(json.object_contains("json_mesh")) {
        json_texture_path_push(json.object_element("json_mesh").as_string());
        mesh = json_parse_mesh(load_json(json.object_element("json_mesh").as_string()));
        json_texture_path_pop();
    }
    if(json.object_contains("frame_from") or json.object_contains("frame_to") or json.object_contains("frame_up")) {
        auto from = z3f, to = zero3f, up = y3f;
        json_set_optvalue(json, from, "frame_from");
        json_set_optvalue(json, to, "frame_to");
        json_set_optvalue(json, up, "frame_up");
        mesh->frame = lookat_frame(from, to, up);
    } else {
        json_set_optvalue(json, mesh->frame.o, "frame_o");
        json_set_optvalue(json, mesh->frame.x, "frame_x");
        json_set_optvalue(json, mesh->frame.y, "frame_y");
        json_set_optvalue(json, mesh->frame.z, "frame_z");
        mesh->frame = orthonormalize_zyx(mesh->frame);
    }
    
    // vertex
    if (json.object_contains("vertex")) {
        auto vertex_array = json.object_element("vertex");

        for (auto& vertex : vertex_array.as_array_ref()) {
            timestamp_t _id = 0;
            json_set_optvalue(vertex, _id, "_id_");
            auto v = vec3f();
            json_set_optvalue(vertex, v, "pos");
            //use map
            //mesh->vertices.emplace(_id,mesh->pos.size());
            mesh->vertices.emplace(_id, make_pair(mesh->pos.size(),set<timestamp_t>() ));
            // use array [deprecated]
            //mesh->vertex_ids.push_back(_id);
            mesh->pos.push_back(v);
        }
        // keep vertex pos indexed by vertex id map ordering
        indexing_vertex_position(mesh);
    }
    
    // triangles
    if (json.object_contains("triangle")) {
        auto triangle_array = json.object_element("triangle");
        for (auto& triangle : triangle_array.as_array_ref()) {
            timestamp_t _id = 0;
            json_set_optvalue(triangle, _id, "_id_");
            auto v = vec3id();
            json_set_optvalue(triangle, v, "ver_ids");
            //mesh->triangle_ids.push_back(_id);
            //mesh->triangle.push_back(v);
            // find indices position
            auto& a = mesh->vertices[v.first];
            auto& b = mesh->vertices[v.second];
            auto& c = mesh->vertices[v.third];
            // insert this id in vertex adiacencis
            a.second.insert(_id);
            b.second.insert(_id);
            c.second.insert(_id);
            // compute normal
            auto n = normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first]));
            // add normal to vertex normal
            mesh->norm[a.first] += n;
            mesh->norm[b.first] += n;
            mesh->norm[c.first] += n;
            // build triangle structure
            mesh->triangle.emplace(_id,make_tuple(mesh->triangle_index.size(),v,n));
            mesh->triangle_index.push_back({a.first,b.first,c.first});
        }
        // keep triangle index orderd by map key order
        indexing_triangle_position(mesh);
    }
    
    // quads
    if (json.object_contains("quad")) {
        auto quad_array = json.object_element("quad");
        for (auto& quad : quad_array.as_array_ref()) {
            timestamp_t _id = 0;
            json_set_optvalue(quad, _id, "_id_");
            auto v = vec4id();
            json_set_optvalue(quad, v, "ver_ids");
            //mesh->quad_ids.push_back(_id);
            //mesh->quad.push_back(v);
            // find indices position
            auto& a = mesh->vertices[v.first];
            auto& b = mesh->vertices[v.second];
            auto& c = mesh->vertices[v.third];
            auto& d = mesh->vertices[v.fourth];
            // insert this id in vertex adiacencis
            a.second.insert(_id);
            b.second.insert(_id);
            c.second.insert(_id);
            d.second.insert(_id);
            // compute normal
            auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                               normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
            // add normal to vertex normal
            mesh->norm[a.first] += n;
            mesh->norm[b.first] += n;
            mesh->norm[c.first] += n;
            mesh->norm[d.first] += n;
            // build quad structure
            mesh->quad.emplace(_id,make_tuple(mesh->quad_index.size(),v,n));
            mesh->quad_index.push_back({a.first,b.first,c.first,d.first});
        }
        // keep quad index orderd by map key order
        indexing_quad_position(mesh);
    }
    // apply mean to normals
    for (auto v : mesh->vertices) {
        if(auto mean = v.second.second.size()) mesh->norm[v.second.first] /= mean;
            
    }
    
    // edges
    if (json.object_contains("edges")) {
        auto edges_array = json.object_element("edges");
        for (auto& edge : edges_array.as_array_ref()) {
            timestamp_t _id = 0;
            json_set_optvalue(edge, _id, "_id_");
            auto v = vec2id();
            json_set_optvalue(edge, v, "ver_ids");
            //mesh->edge_ids.push_back(_id);
            //mesh->edge.push_back(v);
            // find indices position
            auto& a = mesh->vertices[v.first];
            auto& b = mesh->vertices[v.second];
            // build edge structure
            mesh->edge.emplace(_id,make_pair(mesh->edge_index.size(),v));
            mesh->edge_index.push_back({a.first,b.first});
        }
        // keep edge index orderd by map key order
        indexing_edge_position(mesh);
    }

    //if(json.object_contains("norm")) json_set_optvalue(json, mesh->norm, "norm");
    //if(json.object_contains("quad_index")) json_set_optvalue(json, mesh->quad_index, "quad_index");
    
    //init_mesh_properties_from_map(mesh, false, false, false, false);

    /*
    json_set_optvalue(json, mesh->pos, "pos");
    json_set_optvalue(json, mesh->texcoord, "texcoord");
    json_set_optvalue(json, mesh->triangle, "triangle");
    json_set_optvalue(json, mesh->quad, "quad");
    json_set_optvalue(json, mesh->point, "point");
    json_set_optvalue(json, mesh->line, "line");
    json_set_optvalue(json, mesh->spline, "spline");
    */
    
    if(json.object_contains("material")) add_reference(mesh->mat,json.object_element("material"));
    json_set_optvalue(json, mesh->subdivision_catmullclark_level, "subdivision_catmullclark_level");
    json_set_optvalue(json, mesh->subdivision_catmullclark_smooth, "subdivision_catmullclark_smooth");
    json_set_optvalue(json, mesh->subdivision_bezier_level, "subdivision_bezier_level");
    if(json.object_contains("animation")) mesh->animation = json_parse_frame_animation(json.object_element("animation"));
    if(json.object_contains("skinning")) mesh->skinning = json_parse_mesh_skinning(json.object_element("skinning"));
    if(json.object_contains("json_skinning")) mesh->skinning = json_parse_mesh_skinning(load_json(json.object_element("json_skinning").as_string()));
    if(json.object_contains("simulation")) mesh->simulation = json_parse_mesh_simulation(json.object_element("simulation"));
    if (mesh->skinning) {
        if (mesh->skinning->rest_pos.empty()) mesh->skinning->rest_pos = mesh->pos;
        if (mesh->skinning->rest_norm.empty()) mesh->skinning->rest_norm = mesh->norm;
        if (mesh->pos.empty()) mesh->pos = mesh->skinning->rest_pos;
        if (mesh->norm.empty()) mesh->norm = mesh->skinning->rest_norm;
    }
    return mesh;
}

vector<Mesh*> json_parse_meshes(const jsonvalue& json) {
    auto meshes = vector<Mesh*>();
    for(auto& value : json.as_array_ref())
        meshes.push_back( json_parse_mesh(value) );
    return meshes;
}

Light* json_parse_light(const jsonvalue& json) {
    auto light = new Light();
    json_set_optvalue(json, light->_id_, "_id_");
    json_set_optvalue(json, light->_version, "version");
    if(json.object_contains("frame_from") or json.object_contains("frame_to") or json.object_contains("frame_up")) {
        auto from = z3f, to = zero3f, up = y3f;
        json_set_optvalue(json, from, "frame_from");
        json_set_optvalue(json, to, "frame_to");
        json_set_optvalue(json, up, "frame_up");
        light->frame = lookat_frame(from, to, up);
    } else {
        json_set_optvalue(json, light->frame.o, "frame_o");
        json_set_optvalue(json, light->frame.x, "frame_x");
        json_set_optvalue(json, light->frame.y, "frame_y");
        json_set_optvalue(json, light->frame.z, "frame_z");
        light->frame = orthonormalize_zyx(light->frame);
    }
    json_set_optvalue(json, light->intensity, "intensity");
    return light;
}

vector<Light*> json_parse_lights(const jsonvalue& json) {
    auto lights = vector<Light*>();
    for(auto& value : json.as_array_ref())
        lights.push_back( json_parse_light(value) );
    return lights;
}

SceneAnimation* json_parse_scene_animation(const jsonvalue& json) {
    auto animation = new SceneAnimation();
    json_set_optvalue(json, animation->time, "time");
    json_set_optvalue(json, animation->length, "length");
    json_set_optvalue(json, animation->dest, "dt");
    json_set_optvalue(json, animation->simsteps, "simsteps");
    json_set_optvalue(json, animation->gravity, "gravity");
    json_set_optvalue(json, animation->bounce_dump, "bounce_dump");
    return animation;
}

// find material by id
Material* find_material(vector<Material*> materials,timestamp_t id) {
    for (auto mat : materials) {
        if(mat->_id_ == id) return mat;
    }
    return nullptr;
}

// solve reference - link object to other object by id if it's possible
void solve_references(Scene* scene){
    for (auto r : ref_to_solve) {
        if (r->is_materialptr()){
            auto old_mat_ptr = r->as_materialptr();
            if( auto mat = find_material(scene->materials, r->_id))
                *old_mat_ptr = mat;
            else message("cannot solve reference for material id %d - mat default applied\n",r->_id); // leave default material
        }
    }
}

MeshDiff* meshdiff_assume_ordered( Mesh* first_mesh, Mesh* second_mesh){
    auto mesh_diff = new MeshDiff();
    
    if ( not (first_mesh->vertices.size() == first_mesh->pos.size()) ) indexing_vertex_position(first_mesh);
    
    // vertices diff
    auto size_min = min(first_mesh->pos.size(),second_mesh->pos.size());
    auto size_max = max(first_mesh->pos.size(),second_mesh->pos.size());
    unsigned long i = 0;
    // check updated vertices
    for (;i < size_min; i++){
        if(!(first_mesh->pos[i] == second_mesh->pos[i]))
            mesh_diff->update_vertex.emplace(i+1,second_mesh->pos[i]); // vertex changed
        if(!(first_mesh->norm[i] == second_mesh->norm[i]))
            mesh_diff->update_norm.emplace(i+1,second_mesh->norm[i]); // norm changed
    }
    
    if (first_mesh->pos.size() > second_mesh->pos.size()){
        // removed vertices
        for (; i < size_max; i++){
            mesh_diff->remove_vertex.push_back(i+1);
        }
    }
    if (first_mesh->pos.size() > second_mesh->pos.size()){
        // added vertices
        for (; i < size_max; i++){
            mesh_diff->add_vertex.emplace(i+1, second_mesh->pos[i]);
            mesh_diff->add_norm.emplace(i+1, second_mesh->norm[i]);
        }
    }
    
    // faces diff
    //// triangles
    bool first_bigger = first_mesh->triangle.size() >= second_mesh->triangle.size();
    
    auto it1 = first_mesh->triangle.begin();
    auto it2 = second_mesh->triangle.begin();
    
    if (not first_bigger){
        while (it1 != first_mesh->triangle.end())
        {
            if ( not (get<1>(it1->second) == get<1>(it2->second)) ) {
                mesh_diff->remove_triangle.push_back(it1->first);   // remove trinagle
                mesh_diff->add_triangle.emplace(it2->first, get<1>(it2->second)); // add triangle
            }
            ++it1;
            ++it2;
        }
        // add remaining trinagle
        while (it2 != second_mesh->triangle.end()) {
            mesh_diff->add_triangle.emplace(it2->first, get<1>(it2->second)); // add triangle
            ++it2;
        }

    }
    else {
        while (it2 != second_mesh->triangle.end())
        {
            if ( not (get<1>(it1->second) == get<1>(it2->second)) ) {
                mesh_diff->remove_triangle.push_back(it1->first);   // remove trinagle
                mesh_diff->add_triangle.emplace(it2->first, get<1>(it2->second)); // add triangle
            }
            ++it1;
            ++it2;
        }
        // remove remaining trinagle
        while (it1 != first_mesh->triangle.end()) {
            mesh_diff->remove_triangle.push_back(it1->first); // remove triangle
            ++it1;
        }
    }

    //// quads

    first_bigger = first_mesh->quad.size() >= second_mesh->quad.size();
    
    auto qit1 = first_mesh->quad.begin();
    auto qit2 = second_mesh->quad.begin();
    
    if (not first_bigger){
        while (qit1 != first_mesh->quad.end())
        {
            if ( not (get<1>(qit1->second) == get<1>(qit2->second)) ) {
                mesh_diff->remove_quad.push_back(qit1->first);   // remove quad
                mesh_diff->add_quad.emplace(qit2->first, get<1>(qit2->second)); // add quad
            }
            ++qit1;
            ++qit2;
        }
        // add remaining quad
        while (qit2 != second_mesh->quad.end()) {
            mesh_diff->add_quad.emplace(qit2->first, get<1>(qit2->second)); // add quad
            ++qit2;
        }
        
    }
    else {
        while (qit2 != second_mesh->quad.end())
        {
            if ( not (get<1>(qit1->second) == get<1>(qit2->second)) ) {
                mesh_diff->remove_quad.push_back(qit1->first);   // remove quad
                mesh_diff->add_quad.emplace(qit2->first, get<1>(qit2->second)); // add quad
            }
            ++qit1;
            ++qit2;
        }
        // remove remaining quad
        while (qit1 != first_mesh->quad.end()) {
            mesh_diff->remove_quad.push_back(qit1->first); // remove quad
            ++qit1;
        }
    }

    return mesh_diff;
}

// new version
// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void obj_apply_mesh_change(Mesh* mesh, MeshDiff* meshdiff, timestamp_t save_history){
    
    auto vs = mesh->vertices.size();
    auto ts = mesh->triangle.size();
    auto qs = mesh->quad.size();
    auto es = mesh->edge.size();
    
    if ( not (mesh->vertices.size() == mesh->pos.size()) ) indexing_vertex_position(mesh);
    
    timing_apply("save_history[start]");
    if (save_history) {
        auto clone = new MeshDiff(*meshdiff);
        meshdiff_history.emplace(save_history,clone);
    }
    timing_apply("save_history[end]");
    
    
    // remove
    
    timing_apply("remove_edges[start]");
    // remove edges
    for (auto edge : meshdiff->remove_edge){
        // set index to -1
        mesh->edge_index[mesh->edge[edge].first] = none2i;
        // remove edge
        mesh->edge.erase(edge);
    }
    
    if(mesh->edge.size() != es - meshdiff->remove_edge.size()){
        message("error 4\n");
    }
    else
        es -= meshdiff->remove_edge.size();
    
    timing_apply("remove_edges[end]");
    timing_apply("remove_triangle[start]");
    // remove triangles
    for (auto triangle : meshdiff->remove_triangle){
        auto t = mesh->triangle[triangle];
        //update vertex adjacency
        auto& a = mesh->vertices[get<1>(t).first];
        auto& b = mesh->vertices[get<1>(t).second];
        auto& c = mesh->vertices[get<1>(t).third];
        
        if (a.second.size()) a.second.erase(triangle);
        if (b.second.size()) b.second.erase(triangle);
        if (c.second.size()) c.second.erase(triangle);
        
        // set index to -1
        //mesh->triangle_index[get<0>(t)] = zero3i;
        mesh->triangle_index[get<0>(t)] = none3i;
        // remove triangle
        mesh->triangle.erase(triangle);
    }
    
    if(mesh->triangle.size() != ts - meshdiff->remove_triangle.size()){
        message("error 5\n");
    }
    else
        ts -= meshdiff->remove_triangle.size();
    
    timing_apply("remove_triangle[end]");
    timing_apply("remove_quad[start]");
    // remove quads
    for (auto quad : meshdiff->remove_quad){
        auto q = mesh->quad[quad];
        //update vertex normal
        auto& a = mesh->vertices[get<1>(q).first];
        auto& b = mesh->vertices[get<1>(q).second];
        auto& c = mesh->vertices[get<1>(q).third];
        auto& d = mesh->vertices[get<1>(q).fourth];
        
        if (a.second.size()) a.second.erase(quad);
        if (b.second.size()) b.second.erase(quad);
        if (c.second.size()) c.second.erase(quad);
        if (d.second.size()) d.second.erase(quad);
        
        // set index to -1
        mesh->quad_index[get<0>(q)] = none4i;
        // remove quad
        mesh->quad.erase(quad);
    }
    if(mesh->quad.size() != qs - meshdiff->remove_quad.size()){
        message("error 6\n");
    }
    else
        qs -= meshdiff->remove_quad.size();
    
    
    timing_apply("remove_quad[end]");
    
    timing_apply("delete_vertex[start]");
    // vertex delete
    for (auto v : meshdiff->remove_vertex){
        mesh->vertices.erase(v);
        if(mesh->vertices.find(v) != mesh->vertices.end()){
            message("error 7.1: %llu found!!!\n", v);
        }
        
    }
    
    if(mesh->vertices.size() != vs - meshdiff->remove_vertex.size()){
        message("error 7\n");
    }
    else
        vs -= meshdiff->add_vertex.size();
    
    
    timing_apply("delete_vertex[end]");


    
    // update
    
    timing_apply("update_vertex[start]");
    // update existing vertex
    for (auto v : meshdiff->update_vertex) mesh->pos[mesh->vertices[v.first].first] = v.second;
    
    // update existing normal
    for (auto n : meshdiff->update_norm) mesh->norm[mesh->vertices[n.first].first] = n.second;

    if(mesh->vertices.size() != vs){
        message("error 3\n");
    }
    
    timing_apply("update_vertex[end]");

    
    // add
    
    timing_apply("add_vertex[start]");
    // add new vertex
    for (auto v : meshdiff->add_vertex){
        if(mesh->vertices.find(v.first) != mesh->vertices.end()){
            message("error 1.1: %llu found!!!\n", v.first);
        }
        mesh->vertices.emplace(v.first, make_pair(mesh->pos.size(),set<timestamp_t>() ));
        mesh->pos.push_back(v.second);
    }
    timing_apply("add_vertex[end]");
    
    if(mesh->vertices.size() != vs + meshdiff->add_vertex.size()){
        message("error 1: %d != %d + %d\n", mesh->vertices.size() , vs , meshdiff->add_vertex.size());
    }
    else
        vs += meshdiff->add_vertex.size();
    
    timing_apply("add_norm[start]");

    for (auto v : meshdiff->add_norm){
        if(mesh->vertices.find(v.first) != mesh->vertices.end()){
            message("error 1.1: %llu found!!!\n", v.first);
        }
        mesh->norm.push_back(v.second);
    }
    timing_apply("add_norm[end]");

    
    
    timing_apply("add_edge[start]");
    // add edges
    for (auto edge : meshdiff->add_edge){
        mesh->edge.emplace(edge.first,make_pair(mesh->edge_index.size(),edge.second));
        auto& a = mesh->vertices[edge.second.first];
        auto& b = mesh->vertices[edge.second.second];
        mesh->edge_index.push_back({a.first,b.first});
    }
    
    if(mesh->edge.size() != es + meshdiff->add_edge.size()){
        message("error 8\n");
    }
    else
        es += meshdiff->add_edge.size();
    
    timing_apply("add_edge[end]");
    timing_apply("add_triangle[start]");
    // add triangles
    for (auto triangle : meshdiff->add_triangle){
        auto& a = mesh->vertices[triangle.second.first];
        auto& b = mesh->vertices[triangle.second.second];
        auto& c = mesh->vertices[triangle.second.third];
        // compute normal
        auto n = normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first]));
        // update vertex adj
        a.second.emplace(triangle.first);
        b.second.emplace(triangle.first);
        c.second.emplace(triangle.first);
        
        // add triangle
        mesh->triangle.emplace(triangle.first, make_tuple(mesh->triangle_index.size(),triangle.second,n) );
        mesh->triangle_index.push_back({a.first,b.first,c.first});
    }
    
    if(mesh->triangle.size() != ts + meshdiff->add_triangle.size()){
        message("error 9\n");
    }
    else
        ts += meshdiff->add_triangle.size();
    
    
    timing_apply("add_triangle[end]");
    timing_apply("add_quad[start]");
    // add quads
    for (auto quad : meshdiff->add_quad){
        auto& a = mesh->vertices[quad.second.first];
        auto& b = mesh->vertices[quad.second.second];
        auto& c = mesh->vertices[quad.second.third];
        auto& d = mesh->vertices[quad.second.fourth];
        // compute normal
        auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                           normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
        // update vertex  adj
        a.second.emplace(quad.first);
        b.second.emplace(quad.first);
        c.second.emplace(quad.first);
        d.second.emplace(quad.first);
        
        // add
        mesh->quad.emplace(quad.first, make_tuple(mesh->quad_index.size(),quad.second,n));
        mesh->quad_index.push_back({a.first,b.first,c.first,d.first});
    }
    if(mesh->quad.size() != qs + meshdiff->add_quad.size()){
        message("error 10\n");
    }
    else
        qs += meshdiff->add_quad.size();
    
    timing_apply("add_quad[end]");
    
    // set mesh version for shuttle
    mesh->_version = meshdiff->_version;
    
    // if set change frame coordinates
    if (not isnan(meshdiff->frame)) mesh->frame = meshdiff->frame;
    
    vs = mesh->vertices.size();
    ts = mesh->triangle.size();
    qs = mesh->quad.size();
    
    //indexing_vertex_position(mesh);
    timing_apply("indexing[start]");
    indexing_triangle_position(mesh);
    indexing_quad_position(mesh);
    //indexing_edge_position(mesh);
    timing_apply("indexing[end]");
    
    
}

// new version
// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_mesh_change(Mesh* mesh, MeshDiff* meshdiff, timestamp_t save_history){
    auto vs = mesh->vertices.size();
    auto ts = mesh->triangle.size();
    auto qs = mesh->quad.size();
    auto es = mesh->edge.size();
    
    timing_apply("save_history[start]");
    if (save_history) {
        auto clone = new MeshDiff(*meshdiff);
        meshdiff_history.emplace(save_history,clone);
    }
    timing_apply("save_history[end]");

    timing_apply("add_vertex[start]");
    // add new vertex
    for (auto v : meshdiff->add_vertex){
        if(mesh->vertices.find(v.first) != mesh->vertices.end()){
            message("error 1.1: %llu found!!!\n", v.first);
        }
        mesh->vertices.emplace(v.first, make_pair(mesh->pos.size(),set<timestamp_t>() ));
        mesh->pos.push_back(v.second);
        mesh->norm.push_back(zero3f);
    }
    timing_apply("add_vertex[end]");
    
    if(mesh->vertices.size() != vs + meshdiff->add_vertex.size()){
        message("error 1: %d != %d + %d\n", mesh->vertices.size() , vs , meshdiff->add_vertex.size());
    }
    else
        vs += meshdiff->add_vertex.size();
    
    timing_apply("update_vertex[start]");
    // update existing vertex
    for (auto v : meshdiff->update_vertex) mesh->pos[mesh->vertices[v.first].first] = v.second;
    
    if(mesh->vertices.size() != vs){
        message("error 2\n");
    }
    
    // for each vertex update compute new normal
    for (auto v : meshdiff->update_vertex){
        auto vertex_info = mesh->vertices[v.first];
        auto faces = vertex_info.second;
        //mesh->norm[vertex_info.first] = zero3f; // initialize new norm
        // for each adjacent face compute new normal
        for (auto f : faces){
            auto triangle_info = mesh->triangle.find(f);
            if (not (triangle_info == mesh->triangle.end())){
                auto t_id = get<1>(triangle_info->second);
                // face is a triangle
                auto& a = mesh->vertices[t_id.first];
                auto& b = mesh->vertices[t_id.second];
                auto& c = mesh->vertices[t_id.third];
                // compute triangle normal
                auto n = normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first]));
                //mesh->norm[vertex_info.first] += n; // increment vertex normal
                auto old_face_norm = get<2>(triangle_info->second);
                // update linked vertex normal
                mesh->norm[a.first] += (n - old_face_norm) / a.second.size();
                mesh->norm[b.first] += (n - old_face_norm) / b.second.size();
                mesh->norm[c.first] += (n - old_face_norm) / c.second.size();
             
                get<2>(triangle_info->second) = n; // update face normal
            }
            else{
                // face is a quad
                auto& quad_info = mesh->quad[f];
                auto q_id = std::get<1>(quad_info);
                auto& a = mesh->vertices[q_id.first];
                auto& b = mesh->vertices[q_id.second];
                auto& c = mesh->vertices[q_id.third];
                auto& d = mesh->vertices[q_id.fourth];
                // compute triangle normal
                auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                                   normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
                //mesh->norm[vertex_info.first] += n; // increment vertex normal
                auto old_face_norm = std::get<2>(quad_info);
                // update linked vertex normal
                mesh->norm[a.first] += (n - old_face_norm) / a.second.size();
                mesh->norm[b.first] += (n - old_face_norm) / b.second.size();
                mesh->norm[c.first] += (n - old_face_norm) / c.second.size();
                mesh->norm[d.first] += (n - old_face_norm) / d.second.size();
                std::get<2>(quad_info) = n; // update face normal
            }
        }
        // apply mean to normals
        //mesh->norm[vertex_info.first] /= faces.size();

    }
    
    if(mesh->vertices.size() != vs){
        message("error 3\n");
    }

    timing_apply("update_vertex[end]");
    
//    timing_apply("init_position[start]");
//    // restore vertex position indexing
//    if(need_update_pos_faces) indexing_vertex_position(mesh);
//    timing_apply("init_position[end]");
    timing_apply("remove_edges[start]");
    // remove edges
    for (auto edge : meshdiff->remove_edge){
        // set index to -1
        mesh->edge_index[mesh->edge[edge].first] = none2i;
        // remove edge
        mesh->edge.erase(edge);
    }

    if(mesh->edge.size() != es - meshdiff->remove_edge.size()){
        message("error 4\n");
    }
    else
        es -= meshdiff->remove_edge.size();
    
    timing_apply("remove_edges[end]");
    timing_apply("remove_triangle[start]");
    // remove triangles
    for (auto triangle : meshdiff->remove_triangle){
        auto t = mesh->triangle[triangle];
        //update vertex normal
        auto& a = mesh->vertices[get<1>(t).first];
        auto& b = mesh->vertices[get<1>(t).second];
        auto& c = mesh->vertices[get<1>(t).third];
        
        if (a.second.size()){
            mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) - get<2>(t);
            a.second.erase(triangle);
            if(a.second.size() > 1) mesh->norm[a.first] /= a.second.size();
        }
        
        if (b.second.size()){
            mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) - get<2>(t);
            b.second.erase(triangle);
            if(b.second.size() > 1) mesh->norm[b.first] /= b.second.size();
        }
        
        if (c.second.size()){
            mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) - get<2>(t);
            c.second.erase(triangle);
            if(c.second.size() > 1) mesh->norm[c.first] /= c.second.size();
        }
        // set index to -1
        //mesh->triangle_index[get<0>(t)] = zero3i;
        mesh->triangle_index[get<0>(t)] = none3i;
        
        // remove triangle
        mesh->triangle.erase(triangle);
    }
    
    if(mesh->triangle.size() != ts - meshdiff->remove_triangle.size()){
        message("error 5\n");
    }
    else
        ts -= meshdiff->remove_triangle.size();
    
    timing_apply("remove_triangle[end]");
    timing_apply("remove_quad[start]");
    // remove quads
    for (auto quad : meshdiff->remove_quad){
        auto q = mesh->quad[quad];
        //update vertex normal
        auto& a = mesh->vertices[get<1>(q).first];
        auto& b = mesh->vertices[get<1>(q).second];
        auto& c = mesh->vertices[get<1>(q).third];
        auto& d = mesh->vertices[get<1>(q).fourth];
        
        if (a.second.size()){
            mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) - get<2>(q);
            a.second.erase(quad);
            if(a.second.size() > 1) mesh->norm[a.first] /= a.second.size();
        }
        if (b.second.size()){
            mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) - get<2>(q);
            b.second.erase(quad);
            if(b.second.size() > 1) mesh->norm[b.first] /= b.second.size();
        }
        if (c.second.size()){
            mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) - get<2>(q);
            c.second.erase(quad);
            if(c.second.size() > 1) mesh->norm[c.first] /= c.second.size();
        }
        if (d.second.size()){
            mesh->norm[d.first] = (mesh->norm[d.first] * d.second.size()) - get<2>(q);
            d.second.erase(find(begin(d.second), end(d.second), quad));
            if(d.second.size() > 1) mesh->norm[d.first] /= d.second.size();
        }
        // set index to -1
        //mesh->quad_index[get<0>(q)] = zero4i;
        mesh->quad_index[get<0>(q)] = none4i;
        
        // remove qiad
        mesh->quad.erase(quad);
    }
    if(mesh->quad.size() != qs - meshdiff->remove_quad.size()){
        message("error 6\n");
    }
    else
        qs -= meshdiff->remove_quad.size();

    
    timing_apply("remove_quad[end]");
    
    timing_apply("delete_vertex[start]");
    // vertex delete
    for (auto v : meshdiff->remove_vertex){
        mesh->vertices.erase(v);
        if(mesh->vertices.find(v) != mesh->vertices.end()){
            message("error 7.1: %llu found!!!\n", v);
        }
        
    }
    
    if(mesh->vertices.size() != vs - meshdiff->remove_vertex.size()){
        message("error 7\n");
    }
    else
        vs -= meshdiff->add_vertex.size();
    
    
    timing_apply("delete_vertex[end]");

    
    timing_apply("add_edge[start]");
    // add edges
    for (auto edge : meshdiff->add_edge){
        mesh->edge.emplace(edge.first,make_pair(mesh->edge_index.size(),edge.second));
        auto& a = mesh->vertices[edge.second.first];
        auto& b = mesh->vertices[edge.second.second];
        mesh->edge_index.push_back({a.first,b.first});
    }
    
    if(mesh->edge.size() != es + meshdiff->add_edge.size()){
        message("error 8\n");
    }
    else
        es += meshdiff->add_edge.size();

    timing_apply("add_edge[end]");
    timing_apply("add_triangle[start]");
    // add triangles
    for (auto triangle : meshdiff->add_triangle){
        auto& a = mesh->vertices[triangle.second.first];
        auto& b = mesh->vertices[triangle.second.second];
        auto& c = mesh->vertices[triangle.second.third];
        // compute normal
        auto n = normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first]));
        // update first vertex norm and adj
        mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) + n;
        a.second.emplace(triangle.first);
        mesh->norm[a.first] /= a.second.size();
        // update second vertex norm and adj
        mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) + n;
        b.second.emplace(triangle.first);
        mesh->norm[b.first] /= b.second.size();
        // update third vertex norm and adj
        mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) + n;
        c.second.emplace(triangle.first);
        mesh->norm[c.first] /= c.second.size();
        
        // add
        mesh->triangle.emplace(triangle.first, make_tuple(mesh->triangle_index.size(),triangle.second,n) );
        mesh->triangle_index.push_back({a.first,b.first,c.first});
    }
    
    if(mesh->triangle.size() != ts + meshdiff->add_triangle.size()){
        message("error 9\n");
    }
    else
        ts += meshdiff->add_triangle.size();

    
    timing_apply("add_triangle[end]");
    timing_apply("add_quad[start]");
    // add quads
    for (auto quad : meshdiff->add_quad){
        auto& a = mesh->vertices[quad.second.first];
        auto& b = mesh->vertices[quad.second.second];
        auto& c = mesh->vertices[quad.second.third];
        auto& d = mesh->vertices[quad.second.fourth];
        // compute normal
        auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                           normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
        // update first vertex norm and adj
        mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) + n;
        a.second.emplace(quad.first);
        mesh->norm[a.first] /= a.second.size();
        // update second vertex norm and adj
        mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) + n;
        b.second.emplace(quad.first);
        mesh->norm[b.first] /= b.second.size();
        // update third vertex norm and adj
        mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) + n;
        c.second.emplace(quad.first);
        mesh->norm[c.first] /= c.second.size();
        // update third vertex norm and adj
        mesh->norm[d.first] = (mesh->norm[d.first] * d.second.size()) + n;
        d.second.emplace(quad.first);
        mesh->norm[d.first] /= d.second.size();
        
        // add
        mesh->quad.emplace(quad.first, make_tuple(mesh->quad_index.size(),quad.second,n));
        mesh->quad_index.push_back({a.first,b.first,c.first,d.first});
    }
    if(mesh->quad.size() != qs + meshdiff->add_quad.size()){
        message("error 10\n");
    }
    else
        qs += meshdiff->add_quad.size();

    timing_apply("add_quad[end]");

    // set mesh version for shuttle
    mesh->_version = meshdiff->_version;
    
    // if set change frame coordinates
    if (not isnan(meshdiff->frame)) mesh->frame = meshdiff->frame;
    
    vs = mesh->vertices.size();
    ts = mesh->triangle.size();
    qs = mesh->quad.size();

    //indexing_vertex_position(mesh);
    timing_apply("indexing[start]");
    indexing_triangle_position(mesh);
    indexing_quad_position(mesh);
    indexing_edge_position(mesh);
    timing_apply("indexing[end]");


}

void apply_camera_change(Camera* camera, CameraDiff* cameradiff, timestamp_t save_history){
    if (save_history) {
        auto clone = new CameraDiff(*cameradiff);
        cameradiff_history.emplace(save_history,clone);
    }
    if (not isnan(cameradiff->frame)) camera->frame = cameradiff->frame;
    if (not isnan(cameradiff->width)) camera->width = cameradiff->width;
    if (not isnan(cameradiff->height)) camera->height = cameradiff->height;
    if (not isnan(cameradiff->dist)) camera->dist = cameradiff->dist;
    if (not isnan(cameradiff->focus)) camera->focus = cameradiff->focus;
    camera->_version = cameradiff->_version;

}

void apply_light_change(Light* light, LightDiff* lightdiff, timestamp_t save_history){
    if (save_history) {
        auto clone = new LightDiff(*lightdiff);
        lightdiff_history.emplace(save_history,clone);
    }
    if (not isnan(lightdiff->frame)) light->frame = lightdiff->frame;
    if (not isnan(lightdiff->intensity)) light->intensity = lightdiff->intensity;
    light->_version = lightdiff->_version;
}

void apply_material_change(Material* material, MaterialDiff* matdiff, timestamp_t save_history){
    if (save_history) {
        auto clone = new MaterialDiff(*matdiff);
        materialdiff_history.emplace(save_history,clone);
    }
    if (not isnan(matdiff->ke)) material->ke = matdiff->ke;
    if (not isnan(matdiff->kd)) material->kd = matdiff->kd;
    if (not isnan(matdiff->ks)) material->ks = matdiff->ks;
    if (not isnan(matdiff->n)) material->n = matdiff->n;
    if (not isnan(matdiff->kr)) material->kr = matdiff->kr;
    material->_version = matdiff->_version;
}

void apply_change(Scene* scene, SceneDiff* scenediff, timestamp_t save_history){
    if (save_history) {
        scenediff->_label = save_history;
        auto clone = new SceneDiff(*scenediff);
        scenediff_history.push_back(make_pair(save_history,clone));
    }
    // camera diff change
    // it doesn't support multiple cameras
    if(scenediff->cameras.size()) apply_camera_change(scene->camera, scenediff->cameras[0], save_history);
    
    // light diff change
    if(scenediff->lights.size()){
        for (auto lightdiff : scenediff->lights){
            auto light = scene->ids_map[lightdiff->_id_].as_light();
            apply_light_change(light, lightdiff, save_history);
            
        }
    }
    
    // mesh diff change
    if(scenediff->meshes.size()){
        for (auto meshdiff : scenediff->meshes){
            auto mesh = scene->meshes[0];
            apply_mesh_change(mesh, meshdiff, save_history);
        }
    }
    
    // material diff change
    if(scenediff->materials.size()){
        for (auto matdiff : scenediff->materials){
            auto mat = scene->ids_map[matdiff->_id_].as_material();
            apply_material_change(mat, matdiff, save_history);
        }
    }
    
    global_scene_version = scenediff->_label;

}

//reverse
MeshDiff* apply_mesh_change_reverse(Mesh* mesh, MeshDiff* meshdiff, timestamp_t save_history){
    auto reverse = new MeshDiff();
    reverse->_id_ = mesh->_id_;

    auto vs = mesh->vertices.size();
    auto ts = mesh->triangle.size();
    auto qs = mesh->quad.size();
    auto es = mesh->edge.size();
    
    timing_apply("save_history[start]");
    if (save_history) {
        auto clone = new MeshDiff(*meshdiff);
        meshdiff_history.emplace(save_history,clone);
    }
    timing_apply("save_history[end]");
    
    timing_apply("add_vertex[start]");
    // add new vertex
    for (auto v : meshdiff->add_vertex){
        if(mesh->vertices.find(v.first) != mesh->vertices.end()){
            message("error 1.1: %llu found!!!\n", v.first);
        }
        // reverse
        reverse->remove_vertex.push_back(v.first);

        mesh->vertices.emplace(v.first, make_pair(mesh->pos.size(),set<timestamp_t>() ));
        mesh->pos.push_back(v.second);
        mesh->norm.push_back(zero3f);
    }
    timing_apply("add_vertex[end]");
    
    if(mesh->vertices.size() != vs + meshdiff->add_vertex.size()){
        message("error 1: %d != %d + %d\n", mesh->vertices.size() , vs , meshdiff->add_vertex.size());
    }
    else
        vs += meshdiff->add_vertex.size();
    
    timing_apply("update_vertex[start]");
    // update existing vertex
    for (auto v : meshdiff->update_vertex){
        // reverse
        reverse->update_vertex.emplace(v.first, mesh->pos[mesh->vertices[v.first].first]);
        
        mesh->pos[mesh->vertices[v.first].first] = v.second;
    }
    
    if(mesh->vertices.size() != vs){
        message("error 2\n");
    }
    
    // for each vertex update compute new normal
    for (auto v : meshdiff->update_vertex){
        auto vertex_info = mesh->vertices[v.first];
        auto faces = vertex_info.second;
        //mesh->norm[vertex_info.first] = zero3f; // initialize new norm
        // for each adjacent face compute new normal
        for (auto f : faces){
            auto triangle_info = mesh->triangle.find(f);
            if (not (triangle_info == mesh->triangle.end())){
                auto t_id = get<1>(triangle_info->second);
                // face is a triangle
                auto& a = mesh->vertices[t_id.first];
                auto& b = mesh->vertices[t_id.second];
                auto& c = mesh->vertices[t_id.third];
                // compute triangle normal
                auto n = normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first]));
                //mesh->norm[vertex_info.first] += n; // increment vertex normal
                auto old_face_norm = get<2>(triangle_info->second);
                // update linked vertex normal
                mesh->norm[a.first] += (n - old_face_norm) / a.second.size();
                mesh->norm[b.first] += (n - old_face_norm) / b.second.size();
                mesh->norm[c.first] += (n - old_face_norm) / c.second.size();
                
                get<2>(triangle_info->second) = n; // update face normal
            }
            else{
                // face is a quad
                auto& quad_info = mesh->quad[f];
                auto q_id = std::get<1>(quad_info);
                auto& a = mesh->vertices[q_id.first];
                auto& b = mesh->vertices[q_id.second];
                auto& c = mesh->vertices[q_id.third];
                auto& d = mesh->vertices[q_id.fourth];
                // compute triangle normal
                auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                                   normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
                //mesh->norm[vertex_info.first] += n; // increment vertex normal
                auto old_face_norm = std::get<2>(quad_info);
                // update linked vertex normal
                mesh->norm[a.first] += (n - old_face_norm) / a.second.size();
                mesh->norm[b.first] += (n - old_face_norm) / b.second.size();
                mesh->norm[c.first] += (n - old_face_norm) / c.second.size();
                mesh->norm[d.first] += (n - old_face_norm) / d.second.size();
                std::get<2>(quad_info) = n; // update face normal
            }
        }
        // apply mean to normals
        //mesh->norm[vertex_info.first] /= faces.size();
        
    }
    
    if(mesh->vertices.size() != vs){
        message("error 3\n");
    }
    
    timing_apply("update_vertex[end]");
    
    //    timing_apply("init_position[start]");
    //    // restore vertex position indexing
    //    if(need_update_pos_faces) indexing_vertex_position(mesh);
    //    timing_apply("init_position[end]");
    timing_apply("remove_edges[start]");
    // remove edges
    for (auto edge : meshdiff->remove_edge){
        // reverse
        reverse->add_edge.emplace(edge,mesh->edge[edge].second);
        
        // set index to -1
        mesh->edge_index[mesh->edge[edge].first] = none2i;
        // remove edge
        mesh->edge.erase(edge);
    }
    
    if(mesh->edge.size() != es - meshdiff->remove_edge.size()){
        message("error 4\n");
    }
    else
        es -= meshdiff->remove_edge.size();
    
    timing_apply("remove_edges[end]");
    timing_apply("remove_triangle[start]");
    // remove triangles
    for (auto triangle : meshdiff->remove_triangle){
        auto t = mesh->triangle[triangle];
        
        // reverse
        reverse->add_triangle.emplace(triangle,get<1>(t));
        
        //update vertex normal
        auto& a = mesh->vertices[get<1>(t).first];
        auto& b = mesh->vertices[get<1>(t).second];
        auto& c = mesh->vertices[get<1>(t).third];
        
        if (a.second.size()){
            mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) - get<2>(t);
            a.second.erase(triangle);
            if(a.second.size() > 1) mesh->norm[a.first] /= a.second.size();
        }
        
        if (b.second.size()){
            mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) - get<2>(t);
            b.second.erase(triangle);
            if(b.second.size() > 1) mesh->norm[b.first] /= b.second.size();
        }
        
        if (c.second.size()){
            mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) - get<2>(t);
            c.second.erase(triangle);
            if(c.second.size() > 1) mesh->norm[c.first] /= c.second.size();
        }
        // set index to -1
        //mesh->triangle_index[get<0>(t)] = zero3i;
        mesh->triangle_index[get<0>(t)] = none3i;
        
        // remove triangle
        mesh->triangle.erase(triangle);
    }
    
    if(mesh->triangle.size() != ts - meshdiff->remove_triangle.size()){
        message("error 5\n");
    }
    else
        ts -= meshdiff->remove_triangle.size();
    
    timing_apply("remove_triangle[end]");
    timing_apply("remove_quad[start]");
    // remove quads
    for (auto quad : meshdiff->remove_quad){
        auto q = mesh->quad[quad];
        
        // reverse
        reverse->add_quad.emplace(quad,get<1>(q));

        //update vertex normal
        auto& a = mesh->vertices[get<1>(q).first];
        auto& b = mesh->vertices[get<1>(q).second];
        auto& c = mesh->vertices[get<1>(q).third];
        auto& d = mesh->vertices[get<1>(q).fourth];
        
        if (a.second.size()){
            mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) - get<2>(q);
            a.second.erase(quad);
            if(a.second.size() > 1) mesh->norm[a.first] /= a.second.size();
        }
        if (b.second.size()){
            mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) - get<2>(q);
            b.second.erase(quad);
            if(b.second.size() > 1) mesh->norm[b.first] /= b.second.size();
        }
        if (c.second.size()){
            mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) - get<2>(q);
            c.second.erase(quad);
            if(c.second.size() > 1) mesh->norm[c.first] /= c.second.size();
        }
        if (d.second.size()){
            mesh->norm[d.first] = (mesh->norm[d.first] * d.second.size()) - get<2>(q);
            d.second.erase(find(begin(d.second), end(d.second), quad));
            if(d.second.size() > 1) mesh->norm[d.first] /= d.second.size();
        }
        // set index to -1
        //mesh->quad_index[get<0>(q)] = zero4i;
        mesh->quad_index[get<0>(q)] = none4i;
        
        // remove quad
        mesh->quad.erase(quad);
    }
    if(mesh->quad.size() != qs - meshdiff->remove_quad.size()){
        message("error 6\n");
    }
    else
        qs -= meshdiff->remove_quad.size();
    
    
    timing_apply("remove_quad[end]");
    
    timing_apply("delete_vertex[start]");
    // vertex delete
    for (auto v : meshdiff->remove_vertex){
        // reverse
        reverse->add_vertex.emplace(v,mesh->pos[mesh->vertices[v].first]);

        mesh->vertices.erase(v);
        if(mesh->vertices.find(v) != mesh->vertices.end()){
            message("error 7.1: %llu found!!!\n", v);
        }
        
    }
    
    if(mesh->vertices.size() != vs - meshdiff->remove_vertex.size()){
        message("error 7\n");
    }
    else
        vs -= meshdiff->add_vertex.size();
    
    
    timing_apply("delete_vertex[end]");
    
    
    timing_apply("add_edge[start]");
    // add edges
    for (auto edge : meshdiff->add_edge){
        // reverse
        reverse->remove_edge.push_back(edge.first);

        mesh->edge.emplace(edge.first,make_pair(mesh->edge_index.size(),edge.second));
        auto& a = mesh->vertices[edge.second.first];
        auto& b = mesh->vertices[edge.second.second];
        mesh->edge_index.push_back({a.first,b.first});
    }
    
    if(mesh->edge.size() != es + meshdiff->add_edge.size()){
        message("error 8\n");
    }
    else
        es += meshdiff->add_edge.size();
    
    timing_apply("add_edge[end]");
    timing_apply("add_triangle[start]");
    // add triangles
    for (auto triangle : meshdiff->add_triangle){
        // reverse
        reverse->remove_triangle.push_back(triangle.first);

        auto& a = mesh->vertices[triangle.second.first];
        auto& b = mesh->vertices[triangle.second.second];
        auto& c = mesh->vertices[triangle.second.third];
        // compute normal
        auto n = normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first]));
        // update first vertex norm and adj
        mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) + n;
        a.second.emplace(triangle.first);
        mesh->norm[a.first] /= a.second.size();
        // update second vertex norm and adj
        mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) + n;
        b.second.emplace(triangle.first);
        mesh->norm[b.first] /= b.second.size();
        // update third vertex norm and adj
        mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) + n;
        c.second.emplace(triangle.first);
        mesh->norm[c.first] /= c.second.size();
        
        // add
        mesh->triangle.emplace(triangle.first, make_tuple(mesh->triangle_index.size(),triangle.second,n) );
        mesh->triangle_index.push_back({a.first,b.first,c.first});
    }
    
    if(mesh->triangle.size() != ts + meshdiff->add_triangle.size()){
        message("error 9\n");
    }
    else
        ts += meshdiff->add_triangle.size();
    
    
    timing_apply("add_triangle[end]");
    timing_apply("add_quad[start]");
    // add quads
    for (auto quad : meshdiff->add_quad){
        // reverse
        reverse->remove_quad.push_back(quad.first);

        auto& a = mesh->vertices[quad.second.first];
        auto& b = mesh->vertices[quad.second.second];
        auto& c = mesh->vertices[quad.second.third];
        auto& d = mesh->vertices[quad.second.fourth];
        // compute normal
        auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                           normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
        // update first vertex norm and adj
        mesh->norm[a.first] = (mesh->norm[a.first] * a.second.size()) + n;
        a.second.emplace(quad.first);
        mesh->norm[a.first] /= a.second.size();
        // update second vertex norm and adj
        mesh->norm[b.first] = (mesh->norm[b.first] * b.second.size()) + n;
        b.second.emplace(quad.first);
        mesh->norm[b.first] /= b.second.size();
        // update third vertex norm and adj
        mesh->norm[c.first] = (mesh->norm[c.first] * c.second.size()) + n;
        c.second.emplace(quad.first);
        mesh->norm[c.first] /= c.second.size();
        // update third vertex norm and adj
        mesh->norm[d.first] = (mesh->norm[d.first] * d.second.size()) + n;
        d.second.emplace(quad.first);
        mesh->norm[d.first] /= d.second.size();
        
        // add
        mesh->quad.emplace(quad.first, make_tuple(mesh->quad_index.size(),quad.second,n));
        mesh->quad_index.push_back({a.first,b.first,c.first,d.first});
    }
    if(mesh->quad.size() != qs + meshdiff->add_quad.size()){
        message("error 10\n");
    }
    else
        qs += meshdiff->add_quad.size();
    
    timing_apply("add_quad[end]");
    
    // set mesh version for shuttle
    reverse->_version = mesh->_version;
    mesh->_version = meshdiff->_version;
    
    // if set change frame coordinates
    if (not isnan(meshdiff->frame)){ reverse->frame = mesh->frame; mesh->frame = meshdiff->frame; }
    
    vs = mesh->vertices.size();
    ts = mesh->triangle.size();
    qs = mesh->quad.size();
    
    //indexing_vertex_position(mesh);
    timing_apply("indexing[start]");
    indexing_triangle_position(mesh);
    indexing_quad_position(mesh);
    indexing_edge_position(mesh);
    timing_apply("indexing[end]");
    
    return reverse;
}

CameraDiff* apply_camera_change_reverse(Camera* camera, CameraDiff* cameradiff, timestamp_t save_history){
    auto reverse = new CameraDiff();
    reverse->_id_ = camera->_id_;

    if (save_history) {
        auto clone = new CameraDiff(*cameradiff);
        cameradiff_history.emplace(save_history,clone);
    }
    if (not isnan(cameradiff->frame)){ reverse->frame = camera->frame; camera->frame = cameradiff->frame; }
    if (not isnan(cameradiff->width)){ reverse->width = camera->width; camera->width = cameradiff->width; }
    if (not isnan(cameradiff->height)){ reverse->height = camera->height; camera->height = cameradiff->height; }
    if (not isnan(cameradiff->dist)){ reverse->dist = camera->dist; camera->dist = cameradiff->dist; }
    if (not isnan(cameradiff->focus)){ reverse->focus = camera->focus; camera->focus = cameradiff->focus; }

    reverse->_version = camera->_version;
    camera->_version = cameradiff->_version;
    
    return reverse;
}

LightDiff* apply_light_change_reverse(Light* light, LightDiff* lightdiff, timestamp_t save_history){
    auto reverse = new LightDiff();
    reverse->_id_ = light->_id_;

    if (save_history) {
        auto clone = new LightDiff(*lightdiff);
        lightdiff_history.emplace(save_history,clone);
    }
    if (not isnan(lightdiff->frame)){ reverse->frame = light->frame; light->frame = lightdiff->frame; }
    if (not isnan(lightdiff->intensity)){ reverse->intensity = light->intensity; light->intensity = lightdiff->intensity; }
    
    reverse->_version = light->_version;
    light->_version = lightdiff->_version;
    
    return reverse;
}

MaterialDiff* apply_material_change_reverse(Material* material, MaterialDiff* matdiff, timestamp_t save_history){
    auto reverse = new MaterialDiff();
    reverse->_id_ = material->_id_;
    
    if (save_history) {
        auto clone = new MaterialDiff(*matdiff);
        materialdiff_history.emplace(save_history,clone);
    }
    if (not isnan(matdiff->ke)){ reverse->ke = material->ke; material->ke = matdiff->ke; }
    if (not isnan(matdiff->kd)){ reverse->kd = material->kd; material->kd = matdiff->kd; }
    if (not isnan(matdiff->ks)){ reverse->ks = material->ks; material->ks = matdiff->ks; }
    if (not isnan(matdiff->n)){ reverse->n = material->n; material->n = matdiff->n; }
    if (not isnan(matdiff->kr)){ reverse->kr = material->kr; material->kr = matdiff->kr; }
    
    reverse->_version = material->_version;
    material->_version = matdiff->_version;
    
    return reverse;
}

void apply_change_reverse(Scene* scene, SceneDiff* scenediff, timestamp_t save_history){
    auto reverse = new SceneDiff();
    
    if (save_history) {
        scenediff->_label = save_history;
        reverse->_label = save_history;
        auto clone = new SceneDiff(*scenediff);
        scenediff_history.push_back(make_pair(save_history,clone));
    }
    // camera diff change
    // it doesn't support multiple cameras
    if(scenediff->cameras.size()){
        auto cameradiff_reverse = apply_camera_change_reverse(scene->camera, scenediff->cameras[0], save_history);
        reverse->cameras.push_back(cameradiff_reverse);
    }
    
    // light diff change
    if(scenediff->lights.size()){
        for (auto lightdiff : scenediff->lights){
            auto light = scene->ids_map[lightdiff->_id_].as_light();
            auto lightdiff_reverse = apply_light_change_reverse(light, lightdiff, save_history);
            reverse->lights.push_back(lightdiff_reverse);
        }
    }
    
    // mesh diff change
    if(scenediff->meshes.size()){
        for (auto meshdiff : scenediff->meshes){
            auto mesh = scene->meshes[0];
            auto meshdiff_reverse = apply_mesh_change_reverse(mesh, meshdiff, save_history);
            reverse->meshes.push_back(meshdiff_reverse);
        }
    }
    
    // material diff change
    if(scenediff->materials.size()){
        for (auto matdiff : scenediff->materials){
            auto mat = scene->ids_map[matdiff->_id_].as_material();
            auto materialdiff_reverse = apply_material_change_reverse(mat, matdiff, save_history);
            reverse->materials.push_back(materialdiff_reverse);
        }
    }
    
    global_scene_version = scenediff->_label;
    scenediff_history_reverse.push_back(make_pair(save_history,reverse));
}

// swap 2 mesh
void swap_mesh(Mesh* mesh, Scene* scene, bool save_history){
    // update mesh pointers for this id
    auto i = indexof(scene->ids_map[mesh->_id_].as_mesh(), scene->meshes);
    if( i >= 0){
        if (save_history) {
            auto clone = new Mesh(*scene->meshes[i]);
            mesh_history.emplace(get_timestamp(),clone);
        }
        scene->meshes[i] = mesh;
        scene->ids_map[mesh->_id_]._me = mesh;
        //scene->ids_map[mesh->_id_] = *new id_reference(mesh,mesh->_id_);
    }
    else{
        // add new mesh
        scene->meshes.push_back(mesh);
        scene->ids_map.emplace(mesh->_id_,*new id_reference(mesh,mesh->_id_));
    }
    // update mesh material
    i = indexof(scene->ids_map[mesh->mat->_id_].as_material(), scene->materials);
    if( i >= 0){
        scene->materials[i] = mesh->mat;
        scene->ids_map[mesh->mat->_id_]._m = mesh->mat;
        //scene->ids_map[mesh->mat->_id_] = *new id_reference(mesh->mat,mesh->mat->_id_);
    }
    else{
        scene->materials.push_back(mesh->mat);
        scene->ids_map.emplace(mesh->mat->_id_,*new id_reference(mesh->mat,mesh->mat->_id_));
    }
    
}

Scene* json_parse_scene(const jsonvalue& json) {
    // prepare scene
    auto scene = new Scene();
    
    for (auto& obj :  json.as_array_ref()) {
        timestamp_t id = -1;
        // camera
        if (obj.object_contains("camera")){
            scene->camera = json_parse_camera(obj.object_element("camera"));
            id = scene->camera->_id_;
            scene->ids_map.emplace(id, *new id_reference((scene->camera), id));
        }
        if (obj.object_contains("lookat_camera")){
            scene->camera = json_parse_lookatcamera(obj.object_element("lookat_camera"));
            id = scene->camera->_id_;
            scene->ids_map.emplace(id, *new id_reference((scene->camera), id));
        }
        // lights
        if (obj.object_contains("light")){
            auto light = json_parse_light(obj.object_element("light"));
            scene->lights.push_back(light);
            id = light->_id_;
            scene->ids_map.emplace(id, *new id_reference(light, id));
        }
        // meshes
        if (obj.object_contains("mesh")){
            auto mesh = json_parse_mesh(obj.object_element("mesh"));
            scene->meshes.push_back(mesh);
            id = mesh->_id_;
            scene->ids_map.emplace(id, *new id_reference(mesh, id));
        }
        // materials
        if (obj.object_contains("material")){
            auto material = json_parse_material(obj.object_element("material"));
            scene->materials.push_back(material);
            id = material->_id_;
            scene->ids_map.emplace(id, *new id_reference(material, id));
        }
        
    }
    
    // solve object reference
    solve_references(scene);
    
    for (auto& value : scene->ids_map){
        if (value.second.is_mesh()){
            auto mesh = value.second.as_mesh();
            message("id %llu is a %s with material id %llu\n", value.first, value.second.get_type().c_str(), mesh->mat->_id_);
        }
        else
            message("id %llu is a %s\n", value.first, value.second.get_type().c_str());
    }
    
    for (auto mesh : scene->meshes){
        message("mesh %llu with material id %llu\n", mesh->_id_, mesh->mat->_id_);
    }
    
    /*// rendering parameters
    json_set_optvalue(json, scene->image_width, "image_width");
    json_set_optvalue(json, scene->image_height, "image_height");
    json_set_optvalue(json, scene->image_samples, "image_samples");
    json_set_optvalue(json, scene->background, "background");
    json_parse_opttexture(json, scene->background_txt, "background_txt");
    json_set_optvalue(json, scene->ambient, "ambient");
    json_set_optvalue(json, scene->path_max_depth, "path_max_depth");
    json_set_optvalue(json, scene->path_sample_brdf, "path_sample_brdf");
    json_set_optvalue(json, scene->path_shadows, "path_shadows");
    */
    // done
    return scene;
}

Scene* load_json_scene(const string& filename) {
    json_texture_cache.clear();
    json_texture_paths = { "" };
    auto scene = json_parse_scene(load_json(filename));
    json_texture_cache.clear();
    json_texture_paths = { "" };
    return scene;
}

void json_parse_objects(vector<Light*>* lights, vector<Material*>* materials, vector<Camera*>* cameras, vector<Mesh*>* meshes, const jsonvalue& json) {
    for (auto& obj :  json.as_array_ref()) {
        // camera
        if (obj.object_contains("camera")){
            cameras->push_back(json_parse_camera(obj.object_element("camera")));
        }
        if (obj.object_contains("lookat_camera")){
            cameras->push_back(json_parse_lookatcamera(obj.object_element("lookat_camera")));
        }
        // lights
        if (obj.object_contains("light")){
            lights->push_back(json_parse_light(obj.object_element("light")));
        }
        // materials
        if (obj.object_contains("material")){
            materials->push_back(json_parse_material(obj.object_element("material")));
        }
        // meshes
        if (obj.object_contains("mesh")){
            meshes->push_back(json_parse_mesh(obj.object_element("mesh")));
        }
        
    }
}

void load_json_objects(vector<Light*>* lights, vector<Material*>* materials, vector<Camera*>* cameras, vector<Mesh*>* meshes, const string& filename) {
    json_texture_cache.clear();
    json_texture_paths = { "" };
    json_parse_objects(lights, materials, cameras, meshes, load_json(filename));
    json_texture_cache.clear();
    json_texture_paths = { "" };
}

