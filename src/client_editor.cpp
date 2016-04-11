#ifdef _WIN32
#pragma warning (disable: 4224)
#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#endif

#ifdef __APPLE__
//#include <GLUT/GLUT.h>
#include <OpenGL/gl.h>
#include "GLFW/glfw3.h"
#endif

#include "scene_distributed.h"
#include "serialization.hpp"
#include "image.h"
#include "id_reference.h"
#include "client.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <boost/thread/thread.hpp>
#include <algorithm>

#include <iostream>

#include <cstdio>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/assume_abstract.hpp>

// filesystem lib
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <sys/stat.h>


#define PICOJSON_USE_INT64

using boost::asio::ip::tcp;


int gl_program_id = 0;          // OpenGL program handle
int gl_vertex_shader_id = 0;    // OpenGL vertex shader handle
int gl_fragment_shader_id = 0;  // OpenGL fragment shader handle
map<image3f*,int> gl_texture_id;// OpenGL texture handles

// check_dir var
map<string,pair<bool, bool>> file_upload;
int client_id;


// check if an OpenGL error
void error_if_glerror() {
    auto error = glGetError();
    error_if_not(error == GL_NO_ERROR, "gl error");
    
}

// check if an OpenGL shader is compiled
void error_if_shader_not_valid(int shader_id) {
    int iscompiled;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &iscompiled);
    if(not iscompiled) {
        char buf[10000];
        glGetShaderInfoLog(shader_id, 10000, 0, buf);
        error("shader not compiled\n\n%s\n\n",buf);
    }
}

// check if an OpenGL program is valid
void error_if_program_not_valid(int prog_id) {
    int islinked;
    glGetProgramiv(prog_id, GL_LINK_STATUS, &islinked);
    if(not islinked) {
        char buf[10000];
        glGetProgramInfoLog(prog_id, 10000, 0, buf);
        error("program not linked\n\n%s\n\n",buf);
    }
    
    int isvalid;
    glValidateProgram(prog_id);
    glGetProgramiv(prog_id, GL_VALIDATE_STATUS, &isvalid);
    if(not isvalid) {
        char buf[10000];
        glGetProgramInfoLog(prog_id, 10000, 0, buf);
        error("program not validated\n\n%s\n\n",buf);
    }
}

// initialize the shaders
void init_shaders() {
    // load shader code from files
    auto vertex_shader_code = load_text_file("../src/vertex.glsl");
    auto fragment_shader_code = load_text_file("../src/fragment.glsl");
    auto vertex_shader_codes = (char *)vertex_shader_code.c_str();
    auto fragment_shader_codes = (char *)fragment_shader_code.c_str();
    
    // create shaders
    gl_vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    gl_fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
    
    // load shaders code onto the GPU
    glShaderSource(gl_vertex_shader_id,1,(const char**)&vertex_shader_codes,nullptr);
    glShaderSource(gl_fragment_shader_id,1,(const char**)&fragment_shader_codes,nullptr);
    
    // compile shaders
    glCompileShader(gl_vertex_shader_id);
    glCompileShader(gl_fragment_shader_id);
    
    // check if shaders are valid
    error_if_glerror();
    error_if_shader_not_valid(gl_vertex_shader_id);
    error_if_shader_not_valid(gl_fragment_shader_id);
    
    // create program
    gl_program_id = glCreateProgram();
    
    // attach shaders
    glAttachShader(gl_program_id,gl_vertex_shader_id);
    glAttachShader(gl_program_id,gl_fragment_shader_id);
    
    // bind vertex attributes locations
    glBindAttribLocation(gl_program_id, 0, "vertex_pos");
    glBindAttribLocation(gl_program_id, 1, "vertex_norm");
    glBindAttribLocation(gl_program_id, 2, "vertex_texcoord");
    
    // link program
    glLinkProgram(gl_program_id);
    
    // check if program is valid
    error_if_glerror();
    error_if_program_not_valid(gl_program_id);
}

// initialize the textures
void init_textures(Scene* scene) {
    // grab textures from scene
    auto textures = get_textures(scene);
    // foreach texture
    for(auto texture : textures) {
        // if already in the gl_texture_id map, skip
        if(gl_texture_id.find(texture) != gl_texture_id.end()) continue;
        // gen texture id
        unsigned int id = 0;
        glGenTextures(1, &id);
        // set id to the gl_texture_id map for later use
        gl_texture_id[texture] = id;
        // bind texture
        glBindTexture(GL_TEXTURE_2D, id);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        // load texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     texture->width(), texture->height(),
                     0, GL_RGB, GL_FLOAT, texture->data());
    }
}

// utility to bind texture parameters for shaders
// uses texture name, texture_on name, texture pointer and texture unit position
void _bind_texture(string name_map, string name_on, image3f* txt, int pos) {
    // if txt is not null
    if(txt) {
        // set texture on boolean parameter to true
        glUniform1i(glGetUniformLocation(gl_program_id,name_on.c_str()),GL_TRUE);
        // activate a texture unit at position pos
        glActiveTexture(GL_TEXTURE0+pos);
        // bind texture object to it from gl_texture_id map
        glBindTexture(GL_TEXTURE_2D, gl_texture_id[txt]);
        // set texture parameter to the position pos
        glUniform1i(glGetUniformLocation(gl_program_id, name_map.c_str()), pos);
    } else {
        // set texture on boolean parameter to false
        glUniform1i(glGetUniformLocation(gl_program_id,name_on.c_str()),GL_FALSE);
        // activate a texture unit at position pos
        glActiveTexture(GL_TEXTURE0+pos);
        // set zero as the texture id
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

// render the scene with OpenGL
void shade(Scene* scene, bool wireframe) {
    // enable depth test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // disable culling face
    glDisable(GL_CULL_FACE);
    // let the shader control the points
    glEnable(GL_POINT_SPRITE);
    
    // set up the viewport from the scene image size
    glViewport(0, 0, scene->image_width, scene->image_height);
    
    // clear the screen (both color and depth) - set cleared color to background
    glClearColor(scene->background.x, scene->background.y, scene->background.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // enable program
    glUseProgram(gl_program_id);
    
    // bind camera's position, inverse of frame and projection
    // use frame_to_matrix_inverse and frustum_matrix
    glUniform3fv(glGetUniformLocation(gl_program_id,"camera_pos"),
                 1, &scene->camera->frame.o.x);
    glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"camera_frame_inverse"),
                       1, true, &frame_to_matrix_inverse(scene->camera->frame)[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"camera_projection"),
                       1, true, &frustum_matrix(-scene->camera->dist*scene->camera->width/2, scene->camera->dist*scene->camera->width/2,
                                                -scene->camera->dist*scene->camera->height/2, scene->camera->dist*scene->camera->height/2,
                                                scene->camera->dist,10000)[0][0]);
    
    // bind ambient and number of lights
    glUniform3fv(glGetUniformLocation(gl_program_id,"ambient"),1,&scene->ambient.x);
    glUniform1i(glGetUniformLocation(gl_program_id,"lights_num"),scene->lights.size());
    
    // foreach light
    auto count = 0;
    for(auto light : scene->lights) {
        // bind light position and internsity (create param name with tostring)
        glUniform3fv(glGetUniformLocation(gl_program_id,tostring("light_pos[%d]",count).c_str()),
                     1, &light->frame.o.x);
        glUniform3fv(glGetUniformLocation(gl_program_id,tostring("light_intensity[%d]",count).c_str()),
                     1, &light->intensity.x);
        count++;
    }
    
    // foreach mesh
    for(auto mesh : scene->meshes) {
        // bind material kd, ks, n
        glUniform3fv(glGetUniformLocation(gl_program_id,"material_kd"),
                     1,&mesh->mat->kd.x);
        glUniform3fv(glGetUniformLocation(gl_program_id,"material_ks"),
                     1,&mesh->mat->ks.x);
        glUniform1f(glGetUniformLocation(gl_program_id,"material_n"),
                    mesh->mat->n);
        // bind texture params (txt_on, sampler)
        _bind_texture("material_kd_txt", "material_kd_txt_on", mesh->mat->kd_txt, 0);
        _bind_texture("material_ks_txt", "material_ks_txt_on", mesh->mat->ks_txt, 1);
        _bind_texture("material_norm_txt", "material_norm_txt_on", mesh->mat->norm_txt, 2);
        
        // bind mesh frame - use frame_to_matrix
        glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"mesh_frame"),
                           1,true,&frame_to_matrix(mesh->frame)[0][0]);
        
        // enable vertex attributes arrays and set up pointers to the mesh data
        auto vertex_pos_location = glGetAttribLocation(gl_program_id, "vertex_pos");
        auto vertex_norm_location = glGetAttribLocation(gl_program_id, "vertex_norm");
        auto vertex_texcoord_location = glGetAttribLocation(gl_program_id, "vertex_texcoord");
        glEnableVertexAttribArray(vertex_pos_location);
        glVertexAttribPointer(vertex_pos_location, 3, GL_FLOAT, GL_FALSE, 0, &mesh->pos[0].x);
        glEnableVertexAttribArray(vertex_norm_location);
        glVertexAttribPointer(vertex_norm_location, 3, GL_FLOAT, GL_FALSE, 0, &mesh->norm[0].x);
        if(not mesh->texcoord.empty()) {
            glEnableVertexAttribArray(vertex_texcoord_location);
            glVertexAttribPointer(vertex_texcoord_location, 2, GL_FLOAT, GL_FALSE, 0, &mesh->texcoord[0].x);
        }
        else glVertexAttrib2f(vertex_texcoord_location, 0, 0);
        
        // draw triangles and quads
        if(not wireframe) {
//            if(mesh->triangle.size()) glDrawElements(GL_TRIANGLES, mesh->triangle_index.size()*3, GL_UNSIGNED_INT, &mesh->triangle_index[0].x);
//            if(mesh->quad.size()) glDrawElements(GL_QUADS, mesh->quad_index.size()*4, GL_UNSIGNED_INT, &mesh->quad_index[0].x);
            if(mesh->triangle.size()) glDrawRangeElements(GL_TRIANGLES, 0, mesh->pos.size() - 1,  mesh->triangle_index.size()*3, GL_UNSIGNED_INT, &mesh->triangle_index[0].x);
            if(mesh->quad.size()) glDrawRangeElements(GL_QUADS, 0, mesh->pos.size() - 1, mesh->quad_index.size()*4, GL_UNSIGNED_INT, &mesh->quad_index[0].x);

        } else {
            //auto edges = EdgeMap(mesh->triangle, mesh->quad).edges();
            //if(mesh->edge.size()) glDrawElements(GL_LINES, mesh->edge_index.size()*2, GL_UNSIGNED_INT, &mesh->edge_index[0].x);
            if(mesh->edge.size()) glDrawRangeElements(GL_LINES,  0, mesh->pos.size() - 1, mesh->edge_index.size()*2, GL_UNSIGNED_INT, &mesh->edge_index[0].x);
        }
        
        // draw line sets
        if(not mesh->line.empty()) glDrawElements(GL_LINES, mesh->line.size()*2, GL_UNSIGNED_INT, mesh->line.data());
        for(auto segment : mesh->spline) glDrawElements(GL_LINE_STRIP, 4, GL_UNSIGNED_INT, &segment);
        
        // disable vertex attribute arrays
        glDisableVertexAttribArray(vertex_pos_location);
        glDisableVertexAttribArray(vertex_norm_location);
        if(not mesh->texcoord.empty()) glDisableVertexAttribArray(vertex_texcoord_location);
    }
}

string scene_filename;          // scene filename
string image_filename;          // image filename
Scene* scene;                   // scene arrays
bool save = false;              // whether to start the save loop
bool wireframe = false;         // display as wireframe
bool mesh_mat = false;
bool mesh_ver = false;
bool mesh_rot = false;
bool have_msg = false;
bool send_mesh = false;
bool restore_old = false;
bool send_meshdiff = false;
bool write_log    = false;
bool r_write_log    = false;


// glfw callback for character input
void character_callback(GLFWwindow* window, unsigned int key) {
    switch (key) {
        case 's':
            save = true;
            break;
        case 'w':
            wireframe = not wireframe;
            break;
            
            //        case 'm':
            //            mesh_mat = true;
            //            break;
            //        case 'v':
            //            mesh_ver = true;
            //            break;
            //        case 'r':
            //            mesh_rot = true;
            //            break;
        case 'h':
            have_msg = true;
            break;
        case 'u':
            send_mesh = true;
            break;
        case 'r':
            restore_old = true;
            break;
        case 'a':
            send_meshdiff = true;
            break;
        case 'l':
            write_log = true;
            break;
        case 'k':
            r_write_log = true;
            break;


    }
}


void save_mesh(const Mesh &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_mesh(Mesh &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore from the archive
    ia >> m;
}

void save_meshdiff(const MeshDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_meshdiff(MeshDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore from the archive
    ia >> m;
}

void save_cameradiff(const CameraDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_cameradiff(CameraDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore from the archive
    ia >> m;
}

void save_lightdiff(const LightDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_lightdiff(LightDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore from the archive
    ia >> m;
}

void save_materialdiff(const MaterialDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_materialdiff(MaterialDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore from the archive
    ia >> m;
}

void save_scenediff(const SceneDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_scenediff(SceneDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore from the archive
    ia >> m;
}

// uiloop
void uiloop(editor_client* client) {
    
    vector<Light*> l;
    vector<Camera*> c;
    vector<Material*> m;
    vector<Mesh*> me;
    load_json_objects(&l, &m, &c, &me, "../scenes/demo/objects.json");

    auto camera0 = new Camera();
    auto camera1 = new Camera();
    auto camera2 = new Camera();
    auto camera3 = new Camera();
    auto camera4 = new Camera();
    camera0->frame = frame3f(vec3f(-8.59761,3.36389,7.19465),vec3f(0.641762,-0,0.766904),vec3f(0.220408,0.957811,-0.184442),vec3f(-0.734548,0.2874,0.614687));
    camera1->frame = frame3f(vec3f(-1.90267,0.285991,11.5454),vec3f(0.98669,-0,0.162613),vec3f(0.00397429,0.999701,-0.0241149),vec3f(-0.162564,0.0244402,0.986395));
    camera2->frame = frame3f(vec3f(3.18239,1.01947,11.2174),vec3f(0.962037,0,-0.27292),vec3f(-0.0237732,0.996199,-0.0837999),vec3f(0.271883,0.0871068,0.95838));
    camera3->frame = frame3f(vec3f(1.3975,8.76373,-7.63183),vec3f(-0.983646,0,-0.180115),vec3f(-0.13486,0.662859,0.736499),vec3f(0.119391,0.748744,-0.652019));
    camera4->frame = frame3f(vec3f(1.83261,2.76368,11.358),vec3f(0.987231,0,-0.159292),vec3f(-0.0372069,0.972338,-0.230594),vec3f(0.154886,0.233576,0.959923));
    camera0->_id_ = 14132915310782699;
    camera1->_id_ = 14132915310782699;
    camera2->_id_ = 14132915310782699;
    camera3->_id_ = 14132915310782699;
    camera4->_id_ = 14132915310782699;

    c.push_back(camera0);
    c.push_back(camera1);
    c.push_back(camera2);
    c.push_back(camera3);
    c.push_back(camera4);
    

    vector<vec4i> movesA;
    
    movesA.push_back({0,0,3,0});
    movesA.push_back({1,0,9,1});
    movesA.push_back({1,2,9,2});
    movesA.push_back({2,2,7,2});
    movesA.push_back({2,2,7,4});
    movesA.push_back({4,2,9,4});
    movesA.push_back({0,4,8,5});
    movesA.push_back({0,4,8,5});
    movesA.push_back({0,2,6,5});

    vector<vec4i> movesB;
    
    movesB.push_back({1,0,9,0});
    movesB.push_back({1,1,9,2});
    movesB.push_back({1,3,9,2});
    movesB.push_back({2,2,7,3});
    movesB.push_back({2,0,7,4});
    movesB.push_back({4,2,9,5});
    movesB.push_back({0,4,8,4});
    movesB.push_back({0,4,5,5});
    
    int i = 0;
    
    auto ok = glfwInit();
    error_if_not(ok, "glfw init error");
    
    glfwWindowHint(GLFW_SAMPLES, scene->image_samples);
    
    
    auto window = glfwCreateWindow(scene->image_width,
                                   scene->image_height,
                                   "distributed scene | client", NULL, NULL);
    error_if_not(window, "glfw window error");
    
    glfwMakeContextCurrent(window);
    
    glfwSetCharCallback(window, character_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    
#ifdef _WIN32
    auto ok1 = glewInit();
    error_if_not(GLEW_OK == ok1, "glew init error");
#endif
    
    init_shaders();
    init_textures(scene);
    
    auto mouse_last_x = -1.0;
    auto mouse_last_y = -1.0;
    
    while(not glfwWindowShouldClose(window)) {
        glfwGetFramebufferSize(window, &scene->image_width, &scene->image_height);
        scene->camera->width = (scene->camera->height * scene->image_width) / scene->image_height;
        
        shade(scene,wireframe);
        
        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            if (mouse_last_x < 0 or mouse_last_y < 0) { mouse_last_x = x; mouse_last_y = y; }
            auto delta_x = x - mouse_last_x, delta_y = y - mouse_last_y;
            
            set_view_turntable(scene->camera, delta_x*0.01, -delta_y*0.01, 0, 0, 0);
            
            mouse_last_x = x;
            mouse_last_y = y;
        } else { mouse_last_x = -1; mouse_last_y = -1; }
        
        if(save) {
            auto image = image3f(scene->image_width,scene->image_height);
            glReadPixels(0, 0, scene->image_width, scene->image_height, GL_RGB, GL_FLOAT, &image.at(0,0));
            write_png(image_filename, image, true);
            save = false;
        }
        /*
        if(mesh_mat){
            auto mat = scene->ids_map[5].as_material();
            auto rnd = random()%10;
            auto rnd2 = random()%10;
            if (rnd > rnd2) {
                mat->kd = vec3f((float) (1/++rnd),(float) (1/++rnd2),1);
            }
            else
                mat->kd = vec3f(1,(float) (1/++rnd),(float) (1/++rnd2));
            server->remove_front_op();
            mesh_mat = false;
        }
        if(mesh_ver){
            auto mesh = scene->ids_map[7].as_mesh();
            mesh->pos[214] += transform_vector(mesh->frame, z3f);
            server->remove_front_op();
            mesh_ver = false;
        }
        if(mesh_rot){
            auto mesh = scene->ids_map[7].as_mesh();
            auto m = translation_matrix(mesh->frame.o) * rotation_matrix(x3f.x, mesh->frame.x) * rotation_matrix(x3f.y, mesh->frame.y) * rotation_matrix(x3f.z, mesh->frame.z) * translation_matrix(- mesh->frame.o);
            mesh->frame = transform_frame(m, mesh->frame);
            server->remove_front_op();
            mesh_rot = false;
        }
        */
        if(send_mesh){
            client->write(scene->meshes[0]);
            send_mesh = false;
        }
        
        if(send_meshdiff){
            message("Actual versions:\n");
            message("\t- camera %llu - %d\n",scene->camera->_id_, scene->camera->_version);
            message("\t- light %llu - %d\n",scene->lights[0]->_id_, scene->lights[0]->_version);
            message("\t- mesh %llu - %d\n",scene->meshes[0]->_id_, scene->meshes[0]->_version);
            message("\t- material/mesh %llu - %d\n",scene->meshes[0]->mat->_id_, scene->meshes[0]->mat->_version);
            message("\t- material %llu - %d\n",scene->materials[0]->_id_, scene->materials[0]->_version);

            message("Chose version from 0 to 5 for each component (-1 to exit): ");
            message("es. 3 2 4 5 1\n");
            int c,l,m,mm,mat;
            std::cin >> c >> l >> m >> mm >> mat;
            string filename;
            auto scene_diff = new SceneDiff();
            //camera diff
            if (c >= 0 && c <= 5 && c != scene->camera->_version) {
                auto camera_diff = new CameraDiff();
                filename = "../scenes/diff/c" + to_string(scene->camera->_version) + "toc" + to_string(c);
                timing("deserialize_camera[start]");
                restore_cameradiff(*camera_diff, filename.c_str());
                timing("deserialize_camera[end]");
                scene_diff->cameras.push_back(camera_diff);
            }
            if (l >= 0 && l <= 5 && l != scene->lights[0]->_version) {
                auto light_diff = new LightDiff();
                filename = "../scenes/diff/l" + to_string(scene->lights[0]->_version) + "tol" + to_string(l);
                timing("deserialize_light[start]");
                restore_lightdiff(*light_diff, filename.c_str());
                timing("deserialize_light[end]");
                scene_diff->lights.push_back(light_diff);
            }
            if (m >= 0 && m <= 5 && m != scene->meshes[0]->_version) {
                auto mesh_diff = new MeshDiff();
                filename = "../scenes/diff/m" + to_string(scene->meshes[0]->_version) + "tom" + to_string(m);
                timing("deserialize_mesh[start]");
                restore_meshdiff(*mesh_diff, filename.c_str());
                timing("deserialize_mesh[end]");
                scene_diff->meshes.push_back(mesh_diff);
            }
            if (mat >= 0 && mat <= 5 && mat != scene->materials[0]->_version) {
                auto material_diff = new MaterialDiff();
                filename = "../scenes/diff/mat" + to_string(scene->materials[0]->_version) + ".1tomat" + to_string(mat) + ".1";
                timing("deserialize_material[start]");
                restore_materialdiff(*material_diff, filename.c_str());
                timing("deserialize_material[end]");
                scene_diff->materials.push_back(material_diff);
            }
            if (mm >= 0 && mm <= 5 && mm != scene->meshes[0]->mat->_version) {
                auto material_diff = new MaterialDiff();
                filename = "../scenes/diff/mat" + to_string(scene->meshes[0]->mat->_version) + ".2tomat" + to_string(mm) + ".2";
                timing("deserialize_mesh/mat[start]");
                restore_materialdiff(*material_diff, filename.c_str());
                timing("deserialize_mesh/mat[end]");
                scene_diff->materials.push_back(material_diff);
            }
            
            // apply changes to mesh
            timing("apply_diff[start]");
            timing_apply("apply_diff[start]");
            timestamp_t label = get_timestamp();
            apply_change_reverse(scene, scene_diff, label);
            timing("apply_diff[end]");
            timing_apply("apply_diff[end]");
            message("actual mesh version: %d\n", scene->meshes[0]->_version);
            // send meshdiff
            //client->write(scene_diff);
            scene->camera->frame = frame3f(vec3f(-1.68528,1.69959,11.5761),vec3f(0.999631,0,-0.0271681),vec3f(-0.00394499,0.989401,-0.145153),vec3f(0.0268802,0.145207,0.989036));
            send_meshdiff = false;
        }
        
        while(client->has_pending_mesh()){
            // get next message
            auto msg = client->get_next_pending();
            // switch between message type
            switch (msg->type()) {
                case 0: // Message
                {
                    auto message_tokens = msg->as_message();
                    if(message_tokens[0] == "restore_version"){
                        timing_apply("restore_version[end]");
                        restore_to_version(scene, atoll(message_tokens[1].c_str()));
                        timing_apply("restore_version[end]");
                        message("scene restored to version %llu\n", message_tokens[1].c_str());
                    }
                    client->remove_first();
                }
                    break;
                case 1: // Mesh
                {
                    // get the next mesh recived
                    auto mesh = msg->as_mesh();
                    // save mesh in history and update mesh
                    swap_mesh(mesh, scene, true);
                    // remove from queue
                    client->remove_first();
                }
                    break;
                case 2: // SceneDiff
                {
                    timing("deserialize_from_msg[start]");
                    auto scenediff = msg->as_scenediff();
                    timing("deserialize_from_msg[end]");
                    // apply differences
                    timing("apply_diff[start]");
                    apply_change(scene, scenediff, 0);
                    timing("apply_diff[end]");
                    message("actual mesh version: %d\n", scene->meshes[0]->_version);
                    // remove from queue
                    client->remove_first();
                }
                    break;
                case 3: // MeshDiff
                {
                    // get the next mesh recived
                    timing("deserialize_from_msg[start]");
                    auto meshdiff = msg->as_meshdiff();
                    timing("deserialize_from_msg[end]");
                    // apply differences
                    timing("apply_diff[start]");
                    apply_mesh_change(scene->meshes[0], meshdiff, get_timestamp());
                    timing("apply_diff[end]");
                    message("actual mesh version: %d\n", scene->meshes[0]->_version);
                    // remove from queue
                    client->remove_first();
                }
                    break;
                case 7: // OBJ - MTL
                {
                    // save files in folder
                    //msg->save_files("/Users/gpalozzi/Desktop/test/client" + to_string(client_id) + "/received");
                    //message("scene reloaded\n");
                    // remove from queue
                    //client->remove_first();
                }
                    break;
                default:
                    break;
            }
        }
        
        if(restore_old){
            timestamp_t old = get_global_version();
            auto version = restore_version(scene);
            
            if(old != version){
                stringstream restore_message;
                restore_message << "restore_version " << version;
                client->write(restore_message.str().c_str());
            }
            
            restore_old = false;
        }
        
        if(write_log){
            //save_timing("sender_timing_v" + scene_filename + to_string(scene->meshes[0]->_version));
            save_timing_apply("apply_timing_v" + scene_filename + to_string(scene->meshes[0]->_version));
            write_log = false;
        }
        
        if(r_write_log){
            save_timing("receiver_timing_v" + scene_filename + to_string(scene->meshes[0]->_version));
            r_write_log = false;
        }

        
        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) or have_msg) {
        //if(have_msg){
            int mat = 0;
            int luc = 0;
            int cam = 0;
            int mes = 0;

            
            if (client_id == 0 and i < movesA.size()){
                auto move = movesA[i++];
            
                mat = move.x;
                luc = move.y;
                cam = move.z;
                mes = move.w;
            
                cout << mat << luc << cam << mes << endl;
            }
            
            if (client_id == 1 and i < movesB.size()){
                auto move = movesB[i++];
                
                mat = move.x;
                luc = move.y;
                cam = move.z;
                mes = move.w;
                
                cout << mat << luc << cam << mes << endl;
                movesB.pop_back();
            }
            //cin >> mat >> luc >> cam >> mes;
            
//            cout << "frame3f(vec3f(" << scene->camera->frame.o.x << "," << scene->camera->frame.o.y << "," << scene->camera->frame.o.z << "),";
//            cout << "vec3f(" << scene->camera->frame.x.x << "," << scene->camera->frame.x.y << "," << scene->camera->frame.x.z << "),";
//            cout << "vec3f(" << scene->camera->frame.y.x << "," << scene->camera->frame.y.y << "," << scene->camera->frame.y.z << "),";
//            cout << "vec3f(" << scene->camera->frame.z.x << "," << scene->camera->frame.z.y << "," << scene->camera->frame.z.z << "))" << endl;

            auto scene_diff = new SceneDiff();
            auto mdiff = new MaterialDiff(scene->meshes[0]->mat,m[mat]);
            apply_material_change(scene->meshes[0]->mat, mdiff, 0);
            
            auto ldiff = new LightDiff(scene->lights[0],l[luc]);
            apply_light_change(scene->lights[0], ldiff, 0);
            
            auto cdiff = new CameraDiff(scene->camera,c[cam]);
            apply_camera_change(scene->camera, cdiff, 0);

            auto mediff = new MeshDiff(scene->meshes[0],me[mes]);
            apply_mesh_change(scene->meshes[0], mediff, 0);
            
            scene_diff->cameras.push_back(cdiff);
            scene_diff->lights.push_back(ldiff);
            scene_diff->meshes.push_back(mediff);
            scene_diff->materials.push_back(mdiff);
            
            client->write(scene_diff);
            have_msg = false;
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    
    glfwTerminate();
}

void save_submaterial(const MaterialDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_submaterial(MaterialDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the submaterial from the archive
    ia >> m;
}
bool is_hidden(const boost::filesystem::path &p)
{
    boost::filesystem::path::string_type name = p.filename().string();
    if(name != ".." &&
       name != "."  &&
       name[0] == '.') return true;
    
    return false;
}

bool is_mtl(const boost::filesystem::path &p){
    string filename = p.filename().string();
    auto pos = filename.rfind(".");
    auto ext = (pos == string::npos) ? string() : filename.substr(pos+1);
    if (ext == "mtl") return true;
    return false;
}

bool is_obj(const boost::filesystem::path &p){
    string filename = p.filename().string();
    auto pos = filename.rfind(".");
    auto ext = (pos == string::npos) ? string() : filename.substr(pos+1);
    if (ext == "obj") return true;
    return false;
}

string simple_filename (const boost::filesystem::path &p){
    string filename = p.filename().string();
    auto pos = filename.rfind(".");
    if (pos != string::npos) return filename.substr(0, pos);
    return filename;
}

string remove_ext (const boost::filesystem::path &p){
    string path = p.string();
    auto pos = path.rfind(".");
    if (pos != string::npos) return path.substr(0, pos);
    return path;
}

void check_dir(editor_client* client){
    boost::filesystem::path dir( "/Users/gpalozzi/Desktop/test/client" + to_string(client_id) + "/to_send" );
    boost::posix_time::ptime oldTime( boost::posix_time::second_clock::universal_time() );
    boost::posix_time::ptime nowTime = oldTime;
    while(true){
        if (nowTime > oldTime){
            boost::filesystem::directory_iterator dirIter( dir ), dirIterEnd;
            while ( dirIter != dirIterEnd )
            {
                if ( boost::filesystem::exists( *dirIter ) and !boost::filesystem::is_directory( *dirIter ) and !is_hidden((*dirIter).path()) )
                {
                    std::time_t t = boost::filesystem::last_write_time( *dirIter );
                    boost::posix_time::ptime lastAccessTime = boost::posix_time::from_time_t(t);
                    if ( lastAccessTime >= oldTime and lastAccessTime < nowTime )
                    {
                        std::cout << "New file: " << (*dirIter).path() << std::endl;
                        auto mtl = is_mtl((*dirIter).path());
                        auto obj = is_obj((*dirIter).path());
                        error_if_not(mtl or obj, "file extension not supported\n");
                        string path = remove_ext((*dirIter).path());
                        if(file_upload.find(path) == file_upload.end()) file_upload.emplace(path, make_pair(mtl,obj));
                        else if(mtl)
                            file_upload[path].first = mtl;
                        else if(obj)
                            file_upload[path].second = obj;
                        
                        if(file_upload[path].first and file_upload[path].second){
                            message("send...\n");
                            file_upload.erase(path);
                            client->write(path);
                        }
                    }
                }
                ++dirIter;
            }
            oldTime = nowTime;
        }
        else
            nowTime = boost::posix_time::second_clock::universal_time();
        
        if(client->has_pending_mesh()){
            // get next message
            auto msg = client->get_next_pending();
            if (msg->type() == mesh_msg::Obj_Mtl_type){
                msg->save_files("/Users/gpalozzi/Desktop/test/client" + to_string(client_id) + "/received");
                message("obj/mtl saved...\n");
            }
            client->remove_first();
        }
    }
}

// main function
int main(int argc, char** argv) {
    auto args = parse_cmdline(argc, argv,
                              { "3D viewer", "show 3d scene",
                                  {  {"resolution", "r", "image resolution", typeid(int), true, jsonvalue() }  },
                                  {  {"scene_filename", "", "scene filename", typeid(string), false, jsonvalue("scene.json")},
                                      {"image_filename", "", "image filename", typeid(string), true, jsonvalue("")}  }
                              });
    scene_filename = args.object_element("scene_filename").as_string();
    image_filename = (args.object_element("image_filename").as_string() != "") ?
    args.object_element("image_filename").as_string() :
    scene_filename.substr(0,scene_filename.size()-5)+".png";
    // load scene
//    timing("parse_scene[start]");
//    scene = load_obj_scene("../scenes/fat_v0.obj");
//    auto light = new Light();
//    light->_id_ = 126128947120701270;
//    light->frame.o = vec3f(2.0,6.0,5.0);
//    light->intensity = vec3f(25.0,25.0,25.0);
//    scene->meshes[0]->frame.o = vec3f(-2.0,0.0,-1.0);
//    scene->lights.push_back(light);
//    scene->camera = lookat_camera(vec3f(1.0,5.0,4.0), zero3f, y3f, 1.0f, 1.0f, 1.0f,129849216921865, 0);

    scene = load_json_scene("../scenes/shuttleply_v0.json");
    scene->background = zero3f;
    //scene->camera->frame = frame3f(vec3f(-1.68528,1.69959,11.5761),vec3f(0.999631,0,-0.0271681),vec3f(-0.00394499,0.989401,-0.145153),vec3f(0.0268802,0.145207,0.989036));
    timing("parse_scene[end]");
    if(not args.object_element("resolution").is_null()) {
        scene->image_height = args.object_element("resolution").as_int();
        scene->image_width = scene->camera->width * scene->image_height / scene->camera->height;
    }
    
    
    //subdivide(scene);
    /*
    auto diff = new MeshDiff(scene->meshes[0], scene2->meshes[0]);
    string filename = "../scenes/serialsub";
    save_meshdiff(*diff, filename.c_str());
    SubMesh asd;
    restore_meshdiff(asd, filename.c_str());
    */
    // modify mesh 0
    /*
    scene->meshes[0]->mat->kd = vec3f(0.7,0.5,1.0);
    scene->meshes[0]->frame.o.x = 1.5;
    scene->meshes[0]->frame.o.y = 1.5;
    auto m = translation_matrix(scene->meshes[0]->frame.o) * rotation_matrix(x3f.x, scene->meshes[0]->frame.x) * rotation_matrix(x3f.y, scene->meshes[0]->frame.y) * rotation_matrix(x3f.z, scene->meshes[0]->frame.z) * translation_matrix(- scene->meshes[0]->frame.o);
    scene->meshes[0]->frame = transform_frame(m, scene->meshes[0]->frame);
    */
    // server start
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: editor_client <scene> <host> <port>\n";
            return 1;
        }
        
        boost::asio::io_service io_service;
        
        tcp::resolver resolver(io_service);
        //auto endpoint_iterator = resolver.resolve({ argv[1], argv[2] });
        auto endpoint_iterator = resolver.resolve({ "localhost", "3311" });
        editor_client c(io_service, endpoint_iterator);
        
        std::thread t([&io_service](){ io_service.run(); });
        
        /*
        char line[op_message::max_body_length + 1];
        while (std::cin.getline(line, op_message::max_body_length + 1))
        {
            op_message msg;
            msg.body_length(std::strlen(line));
            std::memcpy(msg.body(), line, msg.body_length());
            msg.encode_header();
            c.write(msg);
        }
        */
        client_id = atoi(argv[1]);
        //check_dir(&c);
        uiloop(&c);

        c.close();
        t.join();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    
    return 0;
}

