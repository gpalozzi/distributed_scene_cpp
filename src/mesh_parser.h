//
//  mesh_parser.h
//  tgl
//
//  Created by Gabriele Palozzi on 15/10/14.
//  Copyright (c) 2014 Sapienza. All rights reserved.
//

#ifndef tgl_mesh_parser_h
#define tgl_mesh_parser_h

#include <string>
#include <iostream>
#include "vmath.h"

using namespace std;

struct stock
{
    vector<vec3f> vec;
//    friend ostream& operator<<(ostream&, const stock&);
    vector<float> fl = {1.0,2.2,6.6352};
    vector<timestamp_t> _id = {1231234,965195,215421546};
    map<timestamp_t,int> vertex_id_map = {{124241412,0},{23543666,1},{76543,2},{6585864,3},{3425646,4},{256426,5}};
};

struct wallet
{
    stock* _s;
    //    friend ostream& operator<<(ostream&, const stock&);
};


// inline ostream& operator<<(ostream& os, const stock& rhs) { return os << " " << rhs.vec << "\n"; }


#endif
