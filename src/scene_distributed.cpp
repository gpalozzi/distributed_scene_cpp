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
map<timestamp_t,SubMesh*> submesh_history;

Mesh* get_mesh_to_restrore(){
    vector<timestamp_t> choices;
    int choice = 1;
    message("mesh history\n");
    for(auto m : mesh_history){
        message("[%d] %llu\n",choice++,m.first);
        choices.push_back(m.first);
    }
    message("Mesh to restore (0 = exit): ");
    choice = 0;
    std::cin >> choice;
    if (choice > 0 && choice <= mesh_history.size()) return mesh_history[choices[choice-1]];
    return nullptr;
}


Camera* lookat_camera(vec3f eye, vec3f center, vec3f up, float width, float height, float dist, timestamp_t id) {
    auto camera = new Camera();
    camera->_id_ = id;
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
    json_set_optvalue(json, id, "_id_");
    json_set_optvalue(json, from, "from");
    json_set_optvalue(json, to, "to");
    json_set_optvalue(json, up, "up");
    json_set_optvalue(json, width, "width");
    json_set_optvalue(json, height, "height");
    json_set_optvalue(json, dist, "dist");
    return lookat_camera(from, to, up, width, height, dist, id);
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

void init_mesh_properties_from_map(Mesh* mesh, bool force_quad_update, bool force_triangle_update, bool force_edges_update, bool force_norm_update){
    auto quad_indices = mesh->quad_index.empty() or force_quad_update;
    auto triangle_indices = mesh->triangle_index.empty() or force_triangle_update;
    auto edges_indices = mesh->edge_index.empty() or force_edges_update;
    auto norm = mesh->norm.empty() or force_norm_update;
    
    // clear previous vector
    if (force_quad_update) mesh->quad_index.clear();
    if (force_triangle_update) mesh->triangle_index.clear();
    if (force_edges_update) mesh->edge_index.clear();
    if (force_norm_update) mesh->norm.clear();

    vector<int> faces_per_vert;
    
    if (norm) {
        faces_per_vert = vector<int>(mesh->pos.size(),0);
        mesh->norm = vector<vec3f>(mesh->pos.size(),zero3f);
    }
    
    if (norm or quad_indices) {
        // for each quad
        for(auto quad : mesh->quad){
            // for each vertex find the id in vertices vector and get the index
            auto a = mesh->vertex_id_map[quad.second];
            auto b = mesh->vertex_id_map[quad.second];
            auto c = mesh->vertex_id_map[quad.second];
            auto d = mesh->vertex_id_map[quad.second];
            
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
    }
    if (norm or triangle_indices) {
        // for each triangle
        for(auto triangle : mesh->triangle){
            // for each vertex find the id in vertices vector and get the index
            auto a = mesh->vertex_id_map[triangle.second.first];
            auto b = mesh->vertex_id_map[triangle.second.second];
            auto c = mesh->vertex_id_map[triangle.second.third];
            
            // put this 3 indices in vector
            if (triangle_indices) mesh->triangle_index.push_back({a,b,c});
            
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
    }
    // apply mean to normals
    if(norm){
        for(auto i : range(mesh->norm.size()) ){ if(auto mean = faces_per_vert[i]) mesh->norm[i] /= mean; }
        // delete
        faces_per_vert.clear();
    }
    if (edges_indices) {
        // for each edge
        for(auto edge : mesh->edge){
            // for each vertex find the id in vertices vector and get the index
            auto a = mesh->vertex_id_map[edge.second.first];
            auto b = mesh->vertex_id_map[edge.second.second];
            
            // put this 2 indices in vector
            mesh->edge_index.push_back({a,b});
        }
    }    
}

void indexing_vertex_position(Mesh* mesh){
    vector<vec3f> temp = mesh->pos;
    vector<vec3f> temp_norm;
    // if normal per vertex are already computed
    if (not mesh->norm.empty()){
        temp_norm = mesh->norm;
        mesh->norm.clear();
    }
    mesh->pos.clear();
    for (auto& vertex : mesh->vertex_id_map){
        // move vertex norm if needed
        if (not mesh->norm.empty()) mesh->norm.push_back(temp_norm[vertex.second.first]);
        // move vertex position
//        mesh->pos.push_back(temp[vertex.second]);           edit after vertex_id_map type change
        mesh->pos.push_back(temp[vertex.second.first]);
        vertex.second.first = mesh->pos.size()-1;
        //vertex.second = mesh->pos.size()-1;                edit after vertex_id_map type change
    }
    temp_norm.clear();
    temp.clear();
}

void indexing_triangle_position(Mesh* mesh){
    vector<vec3i> temp = mesh->triangle_index;
    mesh->triangle_index.clear();
    for (auto& triangle : mesh->triangle){
        // move index position
        //        mesh->pos.push_back(temp[vertex.second]);           edit after vertex_id_map type change
        mesh->triangle_index.push_back(temp[get<0>(triangle.second)]);
        get<0>(triangle.second) = mesh->triangle_index.size()-1;
        //vertex.second = mesh->pos.size()-1;                edit after vertex_id_map type change
    }
    temp.clear();
}

void indexing_quad_position(Mesh* mesh){
    vector<vec4i> temp = mesh->quad_index;
    mesh->quad_index.clear();
    for (auto& quad : mesh->quad){
        // move index position
        //        mesh->pos.push_back(temp[vertex.second]);           edit after vertex_id_map type change
        mesh->quad_index.push_back(temp[get<0>(quad.second)]);
        get<0>(quad.second) = mesh->quad_index.size()-1;
        //vertex.second = mesh->pos.size()-1;                edit after vertex_id_map type change
    }
    temp.clear();
}

void indexing_edge_position(Mesh* mesh){
    vector<vec2i> temp = mesh->edge_index;
    mesh->edge_index.clear();
    for (auto& edge : mesh->edge){
        // move index position
        //        mesh->pos.push_back(temp[vertex.second]);           edit after vertex_id_map type change
        mesh->edge_index.push_back(temp[get<0>(edge.second)]);
        get<0>(edge.second) = mesh->edge_index.size()-1;
        //vertex.second = mesh->pos.size()-1;                edit after vertex_id_map type change
    }
    temp.clear();
}


Mesh* json_parse_mesh(const jsonvalue& json) {
    auto mesh = new Mesh();
    json_set_optvalue(json, mesh->_id_, "_id_");
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
            //mesh->vertex_id_map.emplace(_id,mesh->pos.size());
            mesh->vertex_id_map.emplace(_id, make_pair(mesh->pos.size(),vector<timestamp_t>() ));
            // use array [deprecated]
            //mesh->vertex_ids.push_back(_id);
            mesh->pos.push_back(v);
        }
        // keep vertex pos indexed by vertex id map ordering
        indexing_vertex_position(mesh);
    }
    
    // triangle
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
            auto& a = mesh->vertex_id_map[v.first];
            auto& b = mesh->vertex_id_map[v.second];
            auto& c = mesh->vertex_id_map[v.third];
            // insert this id in vertex adiacencis
            a.second.push_back(_id);
            b.second.push_back(_id);
            c.second.push_back(_id);
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
    
    // quad
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
            auto& a = mesh->vertex_id_map[v.first];
            auto& b = mesh->vertex_id_map[v.second];
            auto& c = mesh->vertex_id_map[v.third];
            auto& d = mesh->vertex_id_map[v.fourth];
            // insert this id in vertex adiacencis
            a.second.push_back(_id);
            b.second.push_back(_id);
            c.second.push_back(_id);
            d.second.push_back(_id);
            // compute normal
            auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                               normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
            // add normal to vertex normal
            mesh->norm[a.first] += n;
            mesh->norm[b.first] += n;
            mesh->norm[c.first] += n;
            mesh->norm[d.first] += n;
            // build triangle structure
            mesh->quad.emplace(_id,make_tuple(mesh->quad_index.size(),v,n));
            mesh->quad_index.push_back({a.first,b.first,c.first,d.first});
        }
        // keep quad index orderd by map key order
        indexing_quad_position(mesh);
    }
    // apply mean to normals
    for (auto v : mesh->vertex_id_map) {
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
            auto& a = mesh->vertex_id_map[v.first];
            auto& b = mesh->vertex_id_map[v.second];
            // insert this id in vertex adiacencis
            a.second.push_back(_id);
            b.second.push_back(_id);
            // build triangle structure
            mesh->edge.emplace(_id,make_pair(mesh->edge_index.size(),v));
            mesh->edge_index.push_back({a.first,b.first});
        }
        // keep edge index orderd by map key order
        indexing_edge_position(mesh);
    }

    //if(json.object_contains("norm")) json_set_optvalue(json, mesh->norm, "norm");
    //if(json.object_contains("quad_index")) json_set_optvalue(json, mesh->quad_index, "quad_index");
    
    init_mesh_properties_from_map(mesh, false, false, false, false);
    json_set_optvalue(json, mesh->_version, "version");

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

// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_changes_range(Mesh* mesh, SubMesh* submesh, bool save_history){
    timing_apply("save_history[start]");
    if (save_history) {
        auto clone = new SubMesh(*submesh);
        submesh_history.emplace(get_timestamp(),clone);
    }
    timing_apply("save_history[end]");
    
    auto need_update_pos_faces = not submesh->remove_vertex.empty();
    
    timing_apply("add_vertex[start]");
    // add new vertex
    for (auto v : submesh->add_vertex){
        mesh->vertex_id_map.emplace(v.first,mesh->pos.size());
        mesh->pos.push_back(v.second);
        mesh->norm.push_back(zero3f);
    }
    
    timing_apply("add_vertex[end]");
    timing_apply("update_vertex[start]");
    // update existing vertex
    for (auto v : submesh->update_vertex) mesh->pos[mesh->vertex_id_map[v.first]] = v.second;
    timing_apply("update_vertex[end]");
    timing_apply("delete_vertex[start]");
    // vertex delete
    for (auto v : submesh->remove_vertex) mesh->vertex_id_map.erase(v);
    timing_apply("delete_vertex[end]");
    timing_apply("init_position[start]");
    // restore vertex position indexing
    if(need_update_pos_faces) indexing_vertex_position(mesh);
    timing_apply("init_position[end]");
    timing_apply("remove_edges[start]");
    // remove edges
    for (auto edge : submesh->remove_edge){
        /*
         if (not need_update_pos_faces){
         int index = distance(mesh->edge.begin(), mesh->edge.find(edge));
         mesh->edge_index.erase(mesh->edge_index.begin()+index);
         }
         */
        mesh->edge.erase(edge);
    }
    timing_apply("remove_edges[end]");
    timing_apply("remove_triangle[start]");
    // remove triangles
    for (auto triangle : submesh->remove_triangle) mesh->triangle.erase(triangle);
    timing_apply("remove_triangle[end]");
    timing_apply("remove_quad[start]");
    // remove quads
    for (auto quad : submesh->remove_quad) mesh->quad.erase(quad);
    timing_apply("remove_quad[end]");
    timing_apply("add_edge[start]");
    // add edges
    for (auto edge : submesh->add_edge){
        mesh->edge.insert(edge);
        /*
         auto pair = mesh->edge.insert(edge);
         if (not need_update_pos_faces){
         int index = distance(mesh->edge.begin(), pair.first);
         // for each vertex find the id in vertices vector and get the index
         auto a = mesh->vertex_id_map[edge.second.first];
         auto b = mesh->vertex_id_map[edge.second.second];
         
         // put this 2 indices in vector
         mesh->edge_index.insert(mesh->edge_index.begin()+index, {a,b});
         }
         */
    }
    timing_apply("add_edge[end]");
    timing_apply("add_triangle[start]");
    // add triangles
    for (auto triangle : submesh->add_triangle) mesh->triangle.insert(triangle);
    timing_apply("add_triangle[end]");
    timing_apply("add_quad[start]");
    // add quads
    for (auto quad : submesh->add_quad) mesh->quad.insert(quad);
    timing_apply("add_quad[end]");
    
    // set mesh version for shuttle
    mesh->_version = submesh->_version;
    
    bool force_quad_update = not submesh->add_quad.empty() or not submesh->remove_quad.empty() or need_update_pos_faces;
    bool force_triangle_update = not submesh->add_triangle.empty() or not submesh->remove_triangle.empty() or need_update_pos_faces;
    bool force_edges_update = need_update_pos_faces or not submesh->remove_edge.empty() or not submesh->add_edge.empty();
    bool force_normal_update = not submesh->add_quad.empty() or not submesh->remove_quad.empty() or
    not submesh->add_triangle.empty() or not submesh->remove_triangle.empty() or
    not submesh->update_vertex.empty();
    timing_apply("rebuild_index[start]");
    init_mesh_properties_from_map(mesh, force_quad_update, force_triangle_update, force_edges_update, force_normal_update);
    timing_apply("rebuild_index[end]");
}

// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_changes(Mesh* mesh, SubMesh* submesh, bool save_history){
    timing_apply("save_history[start]");
    if (save_history) {
        auto clone = new SubMesh(*submesh);
        submesh_history.emplace(get_timestamp(),clone);
    }
    timing_apply("save_history[end]");

    auto need_update_pos_faces = not submesh->remove_vertex.empty();
    
    timing_apply("add_vertex[start]");
    // add new vertex
    for (auto v : submesh->add_vertex){
        mesh->vertex_id_map.emplace(v.first,mesh->pos.size());
        mesh->pos.push_back(v.second);
        mesh->norm.push_back(zero3f);
    }
    timing_apply("add_vertex[end]");
    timing_apply("update_vertex[start]");
    // update existing vertex
    for (auto v : submesh->update_vertex) mesh->pos[mesh->vertex_id_map[v.first]] = v.second;
    timing_apply("update_vertex[end]");
    timing_apply("delete_vertex[start]");
    // vertex delete
    for (auto v : submesh->remove_vertex) mesh->vertex_id_map.erase(v);    
    timing_apply("delete_vertex[end]");
    timing_apply("init_position[start]");
    // restore vertex position indexing
    if(need_update_pos_faces) indexing_vertex_position(mesh);
    timing_apply("init_position[end]");
    timing_apply("remove_edges[start]");
    // remove edges
    for (auto edge : submesh->remove_edge){
        /*
         if (not need_update_pos_faces){
         int index = distance(mesh->edge.begin(), mesh->edge.find(edge));
         mesh->edge_index.erase(mesh->edge_index.begin()+index);
         }
         */
        mesh->edge.erase(edge);
    }
    timing_apply("remove_edges[end]");
    timing_apply("remove_triangle[start]");
    // remove triangles
    for (auto triangle : submesh->remove_triangle) mesh->triangle.erase(triangle);
    timing_apply("remove_triangle[end]");
    timing_apply("remove_quad[start]");
    // remove quads
    for (auto quad : submesh->remove_quad) mesh->quad.erase(quad);
    timing_apply("remove_quad[end]");
    timing_apply("add_edge[start]");
    // add edges
    for (auto edge : submesh->add_edge){
        mesh->edge.insert(edge);
        /*
         auto pair = mesh->edge.insert(edge);
         if (not need_update_pos_faces){
         int index = distance(mesh->edge.begin(), pair.first);
         // for each vertex find the id in vertices vector and get the index
         auto a = mesh->vertex_id_map[edge.second.first];
         auto b = mesh->vertex_id_map[edge.second.second];
         
         // put this 2 indices in vector
         mesh->edge_index.insert(mesh->edge_index.begin()+index, {a,b});
         }
         */
    }
    timing_apply("add_edge[end]");
    timing_apply("add_triangle[start]");
    // add triangles
    for (auto triangle : submesh->add_triangle) mesh->triangle.insert(triangle);
    timing_apply("add_triangle[end]");
    timing_apply("add_quad[start]");
    // add quads
    for (auto quad : submesh->add_quad) mesh->quad.insert(quad);
    timing_apply("add_quad[end]");

    // set mesh version for shuttle
    mesh->_version = submesh->_version;
    
    bool force_quad_update = not submesh->add_quad.empty() or not submesh->remove_quad.empty() or need_update_pos_faces;
    bool force_triangle_update = not submesh->add_triangle.empty() or not submesh->remove_triangle.empty() or need_update_pos_faces;
    bool force_edges_update = need_update_pos_faces or not submesh->remove_edge.empty() or not submesh->add_edge.empty();
    bool force_normal_update = not submesh->add_quad.empty() or not submesh->remove_quad.empty() or
    not submesh->add_triangle.empty() or not submesh->remove_triangle.empty() or
    not submesh->update_vertex.empty();
    timing_apply("rebuild_index[start]");
    init_mesh_properties_from_map(mesh, force_quad_update, force_triangle_update, force_edges_update, force_normal_update);
    timing_apply("rebuild_index[end]");
}

// applay vertex creation, elimination and update on a mesh.
// It updates also all other structure in a mesh (edges & faces)
void apply_changes_check(Mesh* mesh, Mesh* real_mesh, SubMesh* submesh, bool save_history){
    if (save_history) {
        auto clone = new Mesh(*mesh);
        mesh_history.emplace(get_timestamp(),clone);
    }
    
    auto need_update_pos_faces = not submesh->remove_vertex.empty();
    
    // add new vertex
    for (auto v : submesh->add_vertex){
        // check
        if ( real_mesh->vertex_id_map.find(v.first) == real_mesh->vertex_id_map.end() or
            not (real_mesh->pos[real_mesh->vertex_id_map[v.first]] == v.second))
            message("[add_vertex] error for %llu",v.first);

        mesh->vertex_id_map.emplace(v.first,mesh->pos.size());
        mesh->pos.push_back(v.second);
        mesh->norm.push_back(zero3f);
    }
    // update existing vertex
    for (auto v : submesh->update_vertex){
        // check
        if ( real_mesh->vertex_id_map.find(v.first) == real_mesh->vertex_id_map.end() or
            not (real_mesh->pos[real_mesh->vertex_id_map[v.first]] == v.second))
            message("[update_vertex] error for %llu",v.first);
        
        mesh->pos[mesh->vertex_id_map[v.first]] = v.second;
    }
    
    // vertex delete
    for (auto v : submesh->remove_vertex){
        if (  real_mesh->vertex_id_map.find(v) != real_mesh->vertex_id_map.end())
            message("[remove_vertex] error for %llu",v);
        
        mesh->vertex_id_map.erase(v);
    }
    
    // restore vertex position indexing
    if(need_update_pos_faces) indexing_vertex_position(mesh);
    
    
    
    
    // remove edges
    for (auto edge : submesh->remove_edge){
        /*
         if (not need_update_pos_faces){
         int index = distance(mesh->edge.begin(), mesh->edge.find(edge));
         mesh->edge_index.erase(mesh->edge_index.begin()+index);
         }
         */
        mesh->edge.erase(edge);
    }
    
    // remove triangles
    for (auto triangle : submesh->remove_triangle) mesh->triangle.erase(triangle);
    
    // remove quads
    for (auto quad : submesh->remove_quad) mesh->quad.erase(quad);
    
    // add edges
    for (auto edge : submesh->add_edge){
        mesh->edge.insert(edge);
        /*
         auto pair = mesh->edge.insert(edge);
         if (not need_update_pos_faces){
         int index = distance(mesh->edge.begin(), pair.first);
         // for each vertex find the id in vertices vector and get the index
         auto a = mesh->vertex_id_map[edge.second.first];
         auto b = mesh->vertex_id_map[edge.second.second];
         
         // put this 2 indices in vector
         mesh->edge_index.insert(mesh->edge_index.begin()+index, {a,b});
         }
         */
    }
    
    // add triangles
    for (auto triangle : submesh->add_triangle) mesh->triangle.insert(triangle);
    
    // add quads
    for (auto quad : submesh->add_quad) mesh->quad.insert(quad);
    
    // set mesh version for shuttle
    mesh->_version = submesh->_version;
    
    bool force_quad_update = not submesh->add_quad.empty() or not submesh->remove_quad.empty() or need_update_pos_faces;
    bool force_triangle_update = not submesh->add_triangle.empty() or not submesh->remove_triangle.empty() or need_update_pos_faces;
    bool force_edges_update = need_update_pos_faces or not submesh->remove_edge.empty() or not submesh->add_edge.empty();
    bool force_normal_update = not submesh->add_quad.empty() or not submesh->remove_quad.empty() or
    not submesh->add_triangle.empty() or not submesh->remove_triangle.empty() or
    not submesh->update_vertex.empty();
    init_mesh_properties_from_map(mesh, force_quad_update, force_triangle_update, force_edges_update, force_normal_update);
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

