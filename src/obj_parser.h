//
//  obj_parser.h
//  dist_scene
//
//  Created by Gabriele Palozzi on 29/11/14.
//  Copyright (c) 2014 Gabriele Palozzi. All rights reserved.
//

#ifndef __dist_scene__obj_parser__
#define __dist_scene__obj_parser__

#include "scene_distributed.h"

extern string obj_vertex_el;
extern string obj_normal_el;
extern string obj_face_el;
extern string obj_mesh_el;


void obj_read_vertices(Mesh *m, stringstream *ss);
void obj_read_faces(Mesh *m, stringstream *ss);
void obj_parse_materials(Scene* scene, stringstream mtl);
Scene* obj_parse_scene(stringstream obj, bool mtl_file, Scene* s);

stringstream load_obj(const string& filename);
Scene* load_obj_scene(const string& filename);

#endif /* defined(__dist_scene__obj_parser__) */
