#include "serialization.hpp"
#include "common.h"
#include <iostream>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

using namespace std;


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
    
    // restore the schedule from the archive
    ia >> m;
}

void save_submesh(const SubMesh &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_submesh(SubMesh &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the schedule from the archive
    ia >> m;
}

int main () {
    std::vector<Scene*> scenes;

    for (auto y : range(6)) {
        std::string filename = "../scenes/shuttleply_v" + std::to_string(y) + ".json";
        message("parsing scene <%s>...",filename.c_str());
        auto scene = load_json_scene(filename);
        message(" done\n");
        message("serializing mesh...");
        std::string name = "../scenes/shuttleply_v" + std::to_string(y) + ".serialized";
        save_mesh(*scene->meshes[0], name.c_str());
        message(" done\n");
        scenes.push_back(scene);
    }

    for (int i = 0; i < 6; i++){
        for (int j=i+1; j<6; j++) {
            // a -> b
            message("\n[m%dto%d]\n",i,j);
            message("diff...");
            auto diff = new SubMesh(scenes[i]->meshes[0],scenes[j]->meshes[0]);
            message(" done\n");
            std::string name = "../scenes/diff/m" + std::to_string(i) + "tom" + std::to_string(j);
            message("serializing diff...");
            save_submesh(*diff, name.c_str());
            message(" done\n");
            // b -> a
            message("[m%dto%d]",j,i);
            message("diff...");
            auto diff2 = new SubMesh(scenes[j]->meshes[0],scenes[i]->meshes[0]);
            message(" done\n");
            std::string name2 = "../scenes/diff/m" + std::to_string(j) + "tom" + std::to_string(i);
            message("serializing diff...");
            save_submesh(*diff2, name2.c_str());
            message(" done\n");
        }
    }
    return 0;
}