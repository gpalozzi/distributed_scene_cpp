#ifndef _SERIALIZATION_HPP_
#define _SERIALIZATION_HPP_

#include "scene_distributed.h"
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>

namespace boost {
    namespace serialization {
        
        // tuple serialization
        template<typename Archive, typename T1, typename T2, typename T3>
        void serialize(Archive & ar, std::tuple<T1,T2,T3>& t, const unsigned int)
        {
            ar & std::get<0>(t);
            ar & std::get<1>(t);
            ar & std::get<2>(t);
        }
        
        // vec3f serialization
        template<class Archive>
        void serialize(Archive & ar, vec3f& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
            ar & v.z;
        }
        
        // vec2f serialization
        template<class Archive>
        void serialize(Archive & ar, vec2f& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
        }
        
        // vec4i serialization
        template<class Archive>
        void serialize(Archive & ar, vec4i& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
            ar & v.z;
            ar & v.w;
        }

        // vec3i serialization
        template<class Archive>
        void serialize(Archive & ar, vec3i& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
            ar & v.z;
        }
        
        // vec2i serialization
        template<class Archive>
        void serialize(Archive & ar, vec2i& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
        }

        // vec4id serialization
        template<class Archive>
        void serialize(Archive & ar, vec4id& v, const unsigned int version)
        {
            ar & v.first;
            ar & v.second;
            ar & v.third;
            ar & v.fourth;
        }

        // vec3id serialization
        template<class Archive>
        void serialize(Archive & ar, vec3id& v, const unsigned int version)
        {
            ar & v.first;
            ar & v.second;
            ar & v.third;
        }
        
        // vec2id serialization
        template<class Archive>
        void serialize(Archive & ar, vec2id& v, const unsigned int version)
        {
            ar & v.first;
            ar & v.second;
        }

        // frame3f serialization
        template<class Archive>
        void serialize(Archive & ar, frame3f& f, const unsigned int version)
        {
            ar & f.o;
            ar & f.x;
            ar & f.y;
            ar & f.z;
        }

        // camera serialization
        template<class Archive>
        void serialize(Archive & ar, Camera& c, const unsigned int version)
        {
            ar & c.frame;
            ar & c.width;
            ar & c.height;
            ar & c.dist;
            ar & c.focus;
            ar & c._id_;
        }
        
        // light serialization
        template<class Archive>
        void serialize(Archive & ar, Light& l, const unsigned int version)
        {
            ar & l.frame;
            ar & l.intensity;
            ar & l._id_;
        }
        
        // material serialization
        template<class Archive>
        void serialize(Archive & ar, Material& m, const unsigned int version)
        {
            ar & m.ke;
            ar & m.kd;
            ar & m.ks;
            ar & m.n;
            ar & m.kr;
            ar & m._id_;
        }
        
        // mesh serialization
        template<class Archive>
        void serialize(Archive & ar, Mesh& m, const unsigned int version)
        {
            ar & m.frame;
            ar & m.mat;
            ar & m.pos;
            ar & m.normal_ids;
            ar & m.norm;
            ar & m.texcoord;
            ar & m.triangle;
            ar & m.triangle_index;
            ar & m.quad;
            ar & m.quad_index;
            ar & m.edge;
            ar & m.edge_index;
            ar & m.point;
            ar & m.line;
            ar & m.spline;
            ar & m.vertices;
            ar & m._id_;
            ar & m._version;
        }
        
        // scene serialization
        template<class Archive>
        void serialize(Archive & ar, Scene& s, const unsigned int version)
        {
            ar & s.camera;
            ar & s.meshes;
            //ar & s.surfaces;
            ar & s.lights;
            ar & s.materials;
            ar & s.background;
            ar & s.ambient;
            ar & s.image_width;
            ar & s.image_height;
            ar & s.image_samples;
            //ar & s.ids_map;
        }
        
        // cameradiff serialization
        template<class Archive>
        void serialize(Archive & ar, CameraDiff& cd, const unsigned int version)
        {
            ar & cd.frame;
            ar & cd.width;
            ar & cd.height;
            ar & cd.dist;
            ar & cd.focus;
            ar & cd._id_;
            ar & cd._version;
        }
        
        // lightdiff serialization
        template<class Archive>
        void serialize(Archive & ar, LightDiff& ld, const unsigned int version)
        {
            ar & ld.frame;
            ar & ld.intensity;
            ar & ld._id_;
            ar & ld._version;
        }
        
        // materialdiff serialization
        template<class Archive>
        void serialize(Archive & ar, MaterialDiff& md, const unsigned int version)
        {
            ar & md.ke;
            ar & md.kd;
            ar & md.ks;
            ar & md.n;
            ar & md.kr;
            ar & md._id_;
            ar & md._version;
        }
        
        // meshdiff serialization
        template<class Archive>
        void serialize(Archive & ar, MeshDiff& md, const unsigned int version)
        {
            ar & md.remove_vertex;
            ar & md.remove_edge;
            ar & md.remove_triangle;
            ar & md.remove_quad;
            ar & md.add_vertex;
            ar & md.add_edge;
            ar & md.add_triangle;
            ar & md.add_quad;
            ar & md.update_vertex;
            ar & md._id_;
            ar & md._version;
        }
        
        // scenediff serialization
        template<class Archive>
        void serialize(Archive & ar, SceneDiff& sd, const unsigned int version)
        {
            ar & sd.cameras;
            ar & sd.lights;
            ar & sd.materials;
            ar & sd.meshes;
            ar & sd._label;
        }
        
    } // namespace serialization
} // namespace boost


#endif
