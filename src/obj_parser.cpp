//
//  obj_parser.cpp
//  dist_scene
//
//  Created by Gabriele Palozzi on 29/11/14.
//  Copyright (c) 2014 Gabriele Palozzi. All rights reserved.
//

#include "obj_parser.h"
#include "common.h"
#include "id_reference.h"
#include <sstream>

using namespace std;

string obj_matlib_el = "mtllib ";
string obj_vertex_el = "v ";
string obj_normal_el = "vn ";
string obj_face_el = "f ";
string obj_mesh_el = "o ";
string obj_mesh_matref_el = "usemtl ";
string obj_mat_el = "newmtl ";
string mat_ns_el = "Ns ";
string mat_kd_el = "Kd ";
string mat_ks_el = "Ks ";
string mat_ka_el = "Ka ";
string mat_ni_el = "Ni ";

string dirname;

stringstream load_obj(const string& filename){
    // open file
    auto stream = std::ifstream(filename);
    error_if_not(bool(stream), "cannot open file: %s\n", filename.c_str());
    // read file
    stringstream buffer;
    buffer << stream.rdbuf();
    stream.close();
    // done
    return buffer;
}

void obj_read_vertices(Scene* scene, Mesh* mesh, stringstream* ss){
    string line, v, x, y, z;
    timestamp_t v_id = 1;
    while(getline(*ss, line)){
        stringstream c;
        c << line;
        c >> v;
        c >> x;
        c >> y;
        c >> z;
        if(line.find(obj_normal_el) != string::npos)
            mesh->norm.push_back(vec3f(stof(x), stof(y), stof(z)));
        else if(line.find(obj_vertex_el) != string::npos){
            mesh->vertices.emplace(v_id++,make_pair(mesh->pos.size(), set<timestamp_t>()));
            mesh->pos.push_back(vec3f(stof(x), stof(y), stof(z)));
        }
        else if (line.find(obj_mesh_matref_el) != string::npos){
            for (auto& mat : scene->materials){
                if(mat->_id_ == stoull(x)) mesh->mat = mat;
            }
        }
        else
            break;
    }
    
}

void obj_read_faces(Mesh* mesh, stringstream *ss){
    timestamp_t _id = 1;
    string line, v_id, n_id;
    vector<vec3f> temp_norm = vector<vec3f>(mesh->pos.size(), zero3f);
    while(getline(*ss, line)){
        if(line.find(obj_face_el) == string::npos)
            break;
        istringstream iss(line);
        vector<string> tokens{istream_iterator<string>{iss},
            istream_iterator<string>{}};
        vector<timestamp_t> v_ids, n_ids;
        for(int j = 1; j < tokens.size(); j++){
            size_t pos = tokens[j].find("/");
            v_ids.push_back(stoull(tokens[j].substr(0, pos)));
            pos = tokens[j].find("/", pos + 1);
            n_ids.push_back(stoull(tokens[j].substr(pos+1)));
        }
        if(v_ids.size() == 3){
            // find indices position
            auto& a = mesh->vertices[v_ids[0]];
            auto& b = mesh->vertices[v_ids[1]];
            auto& c = mesh->vertices[v_ids[2]];
            // insert this id in vertex adiacencis
            a.second.insert(_id);
            b.second.insert(_id);
            c.second.insert(_id);
            // compute normal
            auto n = normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first]));
            // build triangle structure
            mesh->triangle.emplace(_id++,make_tuple(mesh->triangle_index.size(),vec3id(v_ids[0],v_ids[1],v_ids[2]),n));
            mesh->triangle_index.push_back({a.first,b.first,c.first});
            // rearrange normals
            for (int i = 0; i < 3; i++) {
                if (v_ids[i] != n_ids[i]) temp_norm[v_ids[i]-1] = mesh->norm[n_ids[i]-1];
                else temp_norm[n_ids[i]-1] = mesh->norm[n_ids[i]-1];
            }
        }
        if(v_ids.size() == 4){
            // find indices position
            auto& a = mesh->vertices[v_ids[0]];
            auto& b = mesh->vertices[v_ids[1]];
            auto& c = mesh->vertices[v_ids[2]];
            auto& d = mesh->vertices[v_ids[3]];
            // insert this id in vertex adiacencis
            a.second.insert(_id);
            b.second.insert(_id);
            c.second.insert(_id);
            d.second.insert(_id);
            // compute normal
            auto n = normalize(normalize(cross(mesh->pos[b.first]-mesh->pos[a.first], mesh->pos[c.first]-mesh->pos[a.first])) +
                               normalize(cross(mesh->pos[c.first]-mesh->pos[a.first], mesh->pos[d.first]-mesh->pos[a.first])));
            // build quad structure
            mesh->quad.emplace(_id,make_tuple(mesh->quad_index.size(),vec4id(v_ids[0],v_ids[1],v_ids[2],v_ids[3]),n));
            mesh->quad_index.push_back({a.first,b.first,c.first,d.first});
            // rearrange normals
            for (int i = 0; i < 4; i++) {
                if (v_ids[i] != n_ids[i]) temp_norm[v_ids[i]-1] = mesh->norm[n_ids[i]-1];
                else temp_norm[n_ids[i]-1] = mesh->norm[n_ids[i]-1];
            }

        }
    }
    mesh->norm = temp_norm;
}

void obj_read_material(Material* mat, stringstream* ss){
    string line;
    while(getline(*ss, line)){
        if (line == "") break;
        if(line.find(mat_ns_el) != string::npos){
            string t, n;
            stringstream c;
            c << line;
            c >> t;
            c >> n;
            mat->n = stof(n);
            continue;
        }
        if(line.find(mat_kd_el) != string::npos){
            string t, x, y, z;
            stringstream c;
            c << line;
            c >> t;
            c >> x;
            c >> y;
            c >> z;
            mat->kd = vec3f(stof(x), stof(y), stof(z));
            continue;
        }
        if(line.find(mat_ks_el) != string::npos){
            string t, x, y, z;
            stringstream c;
            c << line;
            c >> t;
            c >> x;
            c >> y;
            c >> z;
            mat->ks = vec3f(stof(x), stof(y), stof(z));
            continue;
        }
    }
}

void obj_parse_materials(Scene* scene, stringstream mtl){
    string line;
    while(getline(mtl, line)){
        if(line.find(obj_mat_el) != string::npos)
        {
            auto mat = new Material();
            string m, id;
            stringstream c;
            c << line;
            c >> m;
            c >> id;
            mat->_id_ = stoull(id);
            obj_read_material(mat, &mtl);
            scene->materials.push_back(mat);
            scene->ids_map.emplace(mat->_id_, *new id_reference(mat, mat->_id_));
        }
    }
}

Scene* obj_parse_scene(stringstream obj, bool mtl_file = true, Scene* s = nullptr){
    // prepare scene
    Scene* scene = s;
    if (!s) scene = new Scene();
    
    string line, matlib = "";
    while(getline(obj, line)){
        // material library
        if(mtl_file and line.find(obj_matlib_el) != string::npos)
        {
            string m;
            stringstream c;
            c << line;
            c >> m;
            c >> matlib;
            obj_parse_materials(scene, load_obj(dirname + matlib));
            continue;
        }
        
        if(line.find(obj_mesh_el) != string::npos)
        {
            auto mesh = new Mesh();
            string o, id;
            stringstream c;
            c << line;
            c >> o;
            c >> id;
            mesh->_id_ = stoull(id);
            obj_read_vertices(scene,mesh,&obj);
            obj_read_faces(mesh,&obj);
            scene->meshes.push_back(mesh);
            scene->ids_map.emplace(mesh->_id_, *new id_reference(mesh, mesh->_id_));
        }

    }
    return scene;
}


Scene* load_obj_scene(const string& filename) {
    //get directory name
    auto pos = filename.rfind("/");
    dirname = (pos == string::npos) ? string() : filename.substr(0,pos+1);
    auto scene = obj_parse_scene(load_obj(filename));
    return scene;
}

