#include "serialization.hpp"
#include "id_reference.h"
#include "common.h"
#include <iostream>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

using namespace std;

double time_passed(long long from, long long to){
    return (to - from) / (double)1000000000;
}

void print_diff(MeshDiff* diff){
    cout << "\t\tadd_v: " << diff->add_vertex.size() << endl;
    cout << "\t\tadd_f: " << diff->add_quad.size() + diff->add_triangle.size() << endl;
    cout << "\t\trem_v: " << diff->remove_vertex.size() << endl;
    cout << "\t\trem_f: " << diff->remove_quad.size() + diff->remove_triangle.size() << endl;
    cout << "\t\tupd_v: " << diff->update_vertex.size() << endl;
}

double to_kb(long long size){
    return size / (double)1024;
}

long long get_clock(){
    std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
    return time.time_since_epoch().count();
}

void save_scene(const Scene &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_scene(Scene &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the schedule from the archive
    ia >> m;
}

void save_mesh(const Mesh &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

size_t restore_mesh(Mesh &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the schedule from the archive
    ia >> m;
    return (long long)ifs.gcount();
}

size_t save_meshdiff(const MeshDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
    return (long long)ofs.width();
}

size_t restore_meshdiff(MeshDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the schedule from the archive
    ia >> m;
    return ifs.gcount();
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
    
    // restore the schedule from the archive
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
    
    // restore the schedule from the archive
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
    
    // restore the schedule from the archive
    ia >> m;
}

int main () {
    
    std::vector<Scene*> scenes;
    std::vector<Scene*> objs;

    for (auto y : range(6)) {
        std::string filename = "../scenes/shuttleply_v" + std::to_string(y) + ".json";
        message("parsing scene <%s>...\n",filename.c_str());
        long long from = get_clock();
        auto scene = load_json_scene(filename);
        long long to = get_clock();
        message("\t--- parsing %d - ",y);
        std::cout << time_passed(from, to) << "s\n";
        Scene* obj;
        message(" done\n");
        message("serializing scene...\n");
        std::string name = "../scenes/shuttleply_v" + std::to_string(y) + ".serialized";
        from = get_clock();
        save_scene(*scene, name.c_str());
        to = get_clock();
        message("\t--- serialize %d - ",y);
        std::cout << time_passed(from, to) << "s\n";
        message(" done\n");
        scenes.push_back(scene);
        
        if(y != 0){
            std::string filename_obj = "../scenes/v" + std::to_string(y) + ".json";
            message("parsing obj <%s>...",filename_obj.c_str());
            obj = load_json_scene(filename_obj);
            message(" done\n");
            message("serializing obj...");
            std::string name = "../scenes/v" + std::to_string(y) + ".serialized";
            //save_mesh(*obj, name.c_str());
            message(" done\n");
            objs.push_back(obj);
        }
        else
            objs.push_back(scene);


    }

    for (int i = 0; i < 6; i++){
        for (int j=i+1; j<6; j++) {
            // a -> b
            message("\n[m%dto%d]\n",i,j);
            message("diff...\n");
            long long from = get_clock();
            auto diff = new MeshDiff(scenes[i]->meshes[0],scenes[j]->meshes[0]);
            long long to = get_clock();
            message("\t--- diff %d-%d - ",i,j);
            std::cout << time_passed(from, to) << "s - ";
            message(" done\n");
            std::string name = "../scenes/diff/m" + std::to_string(i) + "tom" + std::to_string(j);
            message("serializing diff...");
            auto size = save_meshdiff(*diff, name.c_str());
            std::cout << to_kb((long long) size) << " kb" << endl;
            
            message(" done\n");
            // b -> a
            message("[m%dto%d]",j,i);
            message("diff...\n");
            from = get_clock();
            auto diff2 = new MeshDiff(scenes[j]->meshes[0],scenes[i]->meshes[0]);
            to = get_clock();
            message("\t--- diff %d-%d - ",j,i);
            std::cout << time_passed(from, to) << "s" << std::endl;
            message(" done\n");
            std::string name2 = "../scenes/diff/m" + std::to_string(j) + "tom" + std::to_string(i);
            message("serializing diff...");
            save_meshdiff(*diff2, name2.c_str());
            message(" done\n");
            // obj ->
            message("diff obj...");
            auto camera = new CameraDiff(objs[i]->camera,objs[j]->camera);
            auto light = new LightDiff(objs[i]->lights[0],objs[j]->lights[0]);
            auto material = new MaterialDiff(objs[i]->materials[0],objs[j]->materials[0]);
            auto material2 = new MaterialDiff(objs[i]->materials[1],objs[j]->materials[1]);
            message(" done\n");
            message("serializing diff obj...");
            std::string name_obj = "../scenes/diff/c" + std::to_string(i) + "toc" + std::to_string(j);
            save_cameradiff(*camera, name_obj.c_str());
            name_obj = "../scenes/diff/l" + std::to_string(i) + "tol" + std::to_string(j);
            save_lightdiff(*light, name_obj.c_str());
            name_obj = "../scenes/diff/mat" + std::to_string(i) + ".1tomat" + std::to_string(j) + ".1";
            save_materialdiff(*material, name_obj.c_str());
            name_obj = "../scenes/diff/mat" + std::to_string(i) + ".2tomat" + std::to_string(j) + ".2";
            save_materialdiff(*material2, name_obj.c_str());
            message(" done\n");
            // obj <-
            message("diff obj...");
            camera = new CameraDiff(objs[j]->camera,objs[i]->camera);
            light = new LightDiff(objs[j]->lights[0],objs[i]->lights[0]);
            material = new MaterialDiff(objs[j]->materials[0],objs[i]->materials[0]);
            material2 = new MaterialDiff(objs[j]->materials[1],objs[i]->materials[1]);
            message(" done\n");
            message("serializing diff obj...");
            name_obj = "../scenes/diff/c" + std::to_string(j) + "toc" + std::to_string(i);
            save_cameradiff(*camera, name_obj.c_str());
            name_obj = "../scenes/diff/l" + std::to_string(j) + "tol" + std::to_string(i);
            save_lightdiff(*light, name_obj.c_str());
            name_obj = "../scenes/diff/mat" + std::to_string(j) + ".1tomat" + std::to_string(i) + ".1";
            save_materialdiff(*material, name_obj.c_str());
            name_obj = "../scenes/diff/mat" + std::to_string(j) + ".2tomat" + std::to_string(i) + ".2";
            save_materialdiff(*material2, name_obj.c_str());
            message(" done\n");
        }
    }
    
    message(" \n\n\n\n\n");

    auto base_scene = scenes[0];
    
    
    for ( int i = 1; i < 6; i++){
        auto diff = new MeshDiff();
        std::string name = "../scenes/diff/m" + std::to_string(i-1) + "tom" + std::to_string(i);
        auto size = restore_meshdiff(*diff, name.c_str());
        long long from = get_clock();
        apply_mesh_change(base_scene->meshes[0], diff, 0);
        long long to = get_clock();
        message("\t--- diff %d-%d - ",i+1,i);
        std::cout << time_passed(from, to) << " s -";
        std::cout << to_kb((long long) size) << " kb" << endl;
        print_diff(diff);
    }
    
    for ( int i = 4; i >= 0; i--){
        auto diff = new MeshDiff();
        std::string name = "../scenes/diff/m" + std::to_string(i+1) + "tom" + std::to_string(i);
        auto size = restore_meshdiff(*diff, name.c_str());
        long long from = get_clock();
        apply_mesh_change(base_scene->meshes[0], diff, 0);
        long long to = get_clock();
        message("\t--- diff %d-%d - ",i+1,i);
        std::cout << time_passed(from, to) << " s -";
        std::cout << to_kb((long long) size) << " kb" << endl;
        print_diff(diff);
    }
    
    return 0;
}