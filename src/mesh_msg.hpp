#ifndef tgl_mesh_msg_hpp
#define tgl_mesh_msg_hpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include "obj_parser.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/filesystem.hpp>

class mesh_msg
{
public:
    enum { header_length = 8 };
    enum { type_length = 3 };
    enum { max_body_length = 512 };
    enum { Operation_type = 0, Mesh_type = 1, SceneDiff_type = 2, MeshDiff_type = 3, CameraDiff_type = 4,
           LightDiff_type = 5, MaterialDiff_type = 6, Obj_Mtl_type = 7 };
    
    mesh_msg()
    : body_length_(0)
    {
    }
    
    template <typename T>
    mesh_msg(const T obj)
    {
        int type = 0;
        if (typeid(*obj) == typeid(Mesh)) type = Mesh_type;
        if (typeid(*obj) == typeid(SceneDiff)) type = SceneDiff_type;
        if (typeid(*obj) == typeid(MeshDiff)) type = MeshDiff_type;
        if (typeid(*obj) == typeid(CameraDiff)) type = CameraDiff_type;
        if (typeid(*obj) == typeid(LightDiff)) type = LightDiff_type;
        if (typeid(*obj) == typeid(MaterialDiff)) type = MaterialDiff_type;
        
        error_if_not(type, "wrong type for message constructor");
        
        std::ostringstream archive_stream;
        boost::archive::text_oarchive archive(archive_stream);
        archive << *obj;
        
        // Set message body length
        body_length_ = archive_stream.str().size();

        // create header
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << body_length_;
        
        // set message type on stream
        std::ostringstream type_stream;
        type_stream << std::setw(type_length) << std::hex << type;
        
        if(!header_stream || header_stream.str().size() != header_length){
            message("Header size > max header length for msg of type: %d", type);
            body_length_ = 0;
        }
        else if (!type_stream || type_stream.str().size() != type_length){
            message("type size > max header length for msg of type: %d", type);
            body_length_ = 0;
        }
        else{
            mesh_msg_data_.resize(body_length_ + type_length + header_length);
            
            //copy header, type & mesh data
            std::memcpy(&mesh_msg_data_[0], header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length], type_stream.str().c_str(), type_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length], archive_stream.str().c_str(), body_length_);
            
            // set message type on this
            message_type = type;
        }
    }
    
    // obj and mtl constructor
    mesh_msg(const string& path)
    {
        // load files
        auto obj_stream = load_obj(path + ".obj");
        auto mtl_stream = load_obj(path + ".mtl");
        stringstream filename(path);
        auto pos = path.rfind("/");
        if (pos != string::npos) filename.str(path.substr(pos+1));
        // Set message body length
        auto obj_length = obj_stream.str().size();
        auto mtl_length = mtl_stream.str().size();
        filename_length_ = filename.str().size();
        body_length_ = header_length + filename_length_ + header_length + obj_length + mtl_length;
        
        // create header
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << body_length_;
        
        // create header for filename
        std::ostringstream filename_header_stream;
        filename_header_stream << std::setw(header_length) << std::hex << filename_length_;

        // create header for obj
        std::ostringstream obj_header_stream;
        obj_header_stream << std::setw(header_length) << std::hex << obj_length;
        
        // set message type on stream
        std::ostringstream type_stream;
        type_stream << std::setw(type_length) << std::hex << Obj_Mtl_type;
        
        if(!header_stream || header_stream.str().size() != header_length){
            message("Header size > max header length for Obj/Mtl type msg");
            body_length_ = 0;
        }
        else if(!filename_header_stream || filename_header_stream.str().size() != header_length){
            message("Filename Header size > max header length for Obj/Mtl type msg");
            body_length_ = 0;
        }
        else if(!obj_header_stream || obj_header_stream.str().size() != header_length){
            message("Object Header size > max header length for Obj/Mtl type msg");
            body_length_ = 0;
        }
        else if (!type_stream || type_stream.str().size() != type_length){
            message("type size > max type length for Obj/Mtl type msg");
            body_length_ = 0;
        }
        else{
            mesh_msg_data_.resize(body_length_ + type_length + header_length);
            
            std::vector<char> temp = std::vector<char>(header_length + type_length + header_length + filename_length_);
            std::memcpy(&temp[0], header_stream.str().c_str(), header_length);
            std::memcpy(&temp[header_length], type_stream.str().c_str(), type_length);
            std::memcpy(&temp[header_length + type_length], filename_header_stream.str().c_str(), header_length);
            std::memcpy(&temp[header_length + type_length + header_length], filename.str().c_str(), filename_length_);

            
            //copy header, type & mesh data
            std::memcpy(&mesh_msg_data_[0], header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length], type_stream.str().c_str(), type_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length], filename_header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length + header_length], filename.str().c_str(), filename_length_);
            std::memcpy(&mesh_msg_data_[header_length + type_length + header_length + filename_length_], obj_header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length + header_length + filename_length_ + header_length], obj_stream.str().c_str(), obj_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length + header_length + filename_length_ + header_length + obj_length], mtl_stream.str().c_str(), mtl_length);

            // set message type on this
            message_type = Obj_Mtl_type;
        }
    }
    
    // string constructor
    mesh_msg(const char * str )
    {
        // Serialize the mesh.
        std::ostringstream archive_stream(str);
        
        // Set message body length
        body_length_ = archive_stream.str().size();
        
        // create header
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << body_length_;
        
        // set message type on stream
        std::ostringstream type_stream;
        type_stream << std::setw(type_length) << std::hex << Operation_type;
        
        if(!header_stream || header_stream.str().size() != header_length){
            message("Header size > max header length for Operation type msg");
            body_length_ = 0;
        }
        else if (!type_stream || type_stream.str().size() != type_length){
            message("type size > max type length for Operation type msg");
            body_length_ = 0;
        }
        else{
            mesh_msg_data_.resize(body_length_ + type_length + header_length);
            
            //copy header, type & mesh data
            std::memcpy(&mesh_msg_data_[0], header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length], type_stream.str().c_str(), type_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length], archive_stream.str().c_str(), body_length_);
            
            // set message type on this
            message_type = Operation_type;
        }
    }

    
    // deprecated
    mesh_msg(const Mesh* mesh )
    {
        // Serialize the mesh.
        std::ostringstream archive_stream;
        boost::archive::text_oarchive archive(archive_stream);
        archive << *mesh;
        
        // Set message body length
        body_length_ = archive_stream.str().size();
        
        // create header
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << body_length_;
        
        // set message type on stream
        std::ostringstream type_stream;
        type_stream << std::setw(type_length) << std::hex << Mesh_type;

        if(!header_stream || header_stream.str().size() != header_length){
            message("Header size > max header length for mesh (id: %llu)", mesh->_id_);
            body_length_ = 0;
        }
        else if (!type_stream || type_stream.str().size() != type_length){
            message("type size > max type length for mesh (id: %llu)", mesh->_id_);
            body_length_ = 0;
        }
        else{
            mesh_msg_data_.resize(body_length_ + type_length + header_length);
            
            //copy header, type & mesh data
            std::memcpy(&mesh_msg_data_[0], header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length], type_stream.str().c_str(), type_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length], archive_stream.str().c_str(), body_length_);
            
            // set message type on this
            message_type = Mesh_type;
        }
    }
    // deprecated
    mesh_msg(const MeshDiff* meshdiff )
    {
        // Serialize the mesh.
        std::ostringstream archive_stream;
        boost::archive::text_oarchive archive(archive_stream);
        archive << *meshdiff;
        
        // Set message body length
        body_length_ = archive_stream.str().size();
        
        // create header
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << body_length_;
        
        // set message type
        std::ostringstream type_stream;
        type_stream << std::setw(type_length) << std::hex << MeshDiff_type;
        
        if(!header_stream || header_stream.str().size() != header_length){
            message("Header size > max header length for meshdiff (id: %llu)", meshdiff->_id_);
            body_length_ = 0;
        }
        else if (!type_stream || type_stream.str().size() != type_length){
            message("type size > max type length for meshdiff (id: %llu)", meshdiff->_id_);
            body_length_ = 0;
        }
        else{
            // resize
            mesh_msg_data_.resize(body_length_ + type_length + header_length);
            
            //copy header, type & mesh data
            std::memcpy(&mesh_msg_data_[0], header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length], type_stream.str().c_str(), type_length);
            std::memcpy(&mesh_msg_data_[header_length + type_length], archive_stream.str().c_str(), body_length_);
            
            // set message type on this
            message_type = MeshDiff_type;
        }
    }

    // get vector mesh position
    char* operator[](int i) { if (i < mesh_msg_data_.size()-1) return &mesh_msg_data_[i]; else message("Access to member out of bound"); return nullptr; }
    
    // get as mesh
    vector<string> as_message() const
    {
        vector<string> tokens;
        string str(mesh_msg_data_.data()+header_length+type_length,body_length_);
        stringstream ss(str);
        string buf;
        while (ss >> buf)
            tokens.push_back(buf);
        return tokens;
    }
    
    vector<string> as_message()
    {
        vector<string> tokens;
        string str(mesh_msg_data_.data()+header_length+type_length,body_length_);
        stringstream ss(str);
        string buf;
        while (ss >> buf)
            tokens.push_back(buf);
        return tokens;
    }
    
    // get as mesh
    Mesh* as_mesh() const
    {
        auto mesh = new Mesh();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *mesh;
        
        return mesh;
    }
    
     Mesh* as_mesh()
    {
        auto mesh = new Mesh();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *mesh;
        
        return mesh;
    }
    
    // get as mesh
    void mesh(Mesh& mesh) const
    {
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> mesh;
    }
    
    void as_mesh(Mesh& mesh)
    {
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> mesh;
    }

    // get as meshdiff
    MeshDiff* as_meshdiff() const
    {
        auto meshdiff = new MeshDiff();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *meshdiff;
        
        return meshdiff;
    }
    
    MeshDiff* as_meshdiff()
    {
        auto meshdiff = new MeshDiff();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *meshdiff;
        
        return meshdiff;
    }
    
    // get as meshdiff
    SceneDiff* as_scenediff() const
    {
        auto scenediff = new SceneDiff();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *scenediff;
        
        return scenediff;
    }
    
    SceneDiff* as_scenediff()
    {
        auto scenediff = new SceneDiff();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *scenediff;
        
        return scenediff;
    }
    
    // get as obj stream
    stringstream as_obj() const
    {
        // body length = inner header + obj length + mtl length
        auto header_type_skip = header_length + type_length + header_length + filename_length_;
        std::size_t obj_length = 0;
        
        stringstream inner_header(string(&mesh_msg_data_[header_type_skip], header_length));
        inner_header >> std::hex >> obj_length;
        error_if_not(obj_length, "error in as_obj() - obj length 0");
        
        return stringstream(string(&mesh_msg_data_[header_type_skip + header_length], obj_length));
    }
    
    stringstream as_obj()
    {
        // body length = inner header + obj length + mtl length
        auto header_type_skip = header_length + type_length + header_length + filename_length_;
        std::size_t obj_length = 0;
        
        stringstream inner_header(string(&mesh_msg_data_[header_type_skip], header_length));
        inner_header >> std::hex >> obj_length;
        error_if_not(obj_length, "error in as_obj() - obj length 0");

        return stringstream(string(&mesh_msg_data_[header_type_skip + header_length], obj_length));
    }
    
    // get as obj stream
    stringstream as_mtl() const
    {
        // body length = inner header + obj length + mtl length
        auto header_type_skip = header_length + type_length + header_length + filename_length_;
        std::size_t obj_length = 0, mtl_length = 0;
        
        stringstream inner_header(string(&mesh_msg_data_[header_type_skip], header_length));
        inner_header >> std::hex >> obj_length;
        error_if_not(obj_length, "error in as_obj() - obj length 0");
        mtl_length = body_length_ - (header_length + filename_length_ + header_length + obj_length);
        
        // load mtl stream
        return stringstream(string(&mesh_msg_data_[header_type_skip + header_length + obj_length], mtl_length));
    }
    
    stringstream as_mtl()
    {
        // body length = inner header + obj length + mtl length
        auto header_type_skip = header_length + type_length + header_length + filename_length_;
        std::size_t obj_length = 0, mtl_length = 0;
        
        stringstream inner_header(string(&mesh_msg_data_[header_type_skip], header_length));
        inner_header >> std::hex >> obj_length;
        error_if_not(obj_length, "error in as_obj() - obj length 0");
        mtl_length = body_length_ - (header_length + filename_length_ + header_length + obj_length);
        
        // load mtl stream
        return stringstream(string(&mesh_msg_data_[header_type_skip + header_length + obj_length], mtl_length));
    }
    
    void save_files(const string& path){
        auto obj_stream = as_obj();
        auto mtl_stream = as_mtl();
        
        // filename
        string obj_path = path + "/" + filename_ + ".obj";
        string mtl_path = path + "/" + filename_ + ".mtl";
        
        ofstream obj, mtl;
        obj.open(obj_path);
        mtl.open(mtl_path);
        
        if (obj.is_open()){
            obj << obj_stream.rdbuf();
            obj.close();
        }
        if (mtl.is_open()){
            mtl << mtl_stream.rdbuf();
            mtl.close();
        }
        
    }
    
    // get all data
    const char* data() const
    {
        return data_;
    }
    
    char* data()
    {
        return data_;
    }
    
    // get message length
    std::size_t length() const
    {
        return header_length + type_length + body_length_;
    }
    
    // get message length
    int type() const
    {
        return message_type;
    }
    
    // get filename if obj/mtl
    string filename() const
    {
        return filename_;
    }

    // get body only
    const char* body() const
    {
        return data_ + header_length;
    }
    
    char* body()
    {
        return data_ + header_length;
    }
    
    // get body length
    std::size_t body_length() const
    {
        return body_length_;
    }
    
    // set body length
    void body_length(std::size_t new_length)
    {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }
    
    // decode message header
    bool decode_header()
    {
        std::istringstream is(std::string(&mesh_msg_data_[0], header_length));
        std::istringstream is2(std::string(&mesh_msg_data_[header_length], type_length));
        if (!(is >> std::hex >> body_length_)) return false;
        if (!(is2 >> std::hex >> message_type)) return false;

        mesh_msg_data_.resize(header_length + type_length +  body_length_);

        return true;
    }
    
    // decode obj/mtl body
    bool decode_body()
    {
        if(message_type == Obj_Mtl_type){
            std::istringstream is_fnl(std::string(&mesh_msg_data_[header_length + type_length], header_length));
            if (!(is_fnl >> std::hex >> filename_length_)) return false;
            std::istringstream is_fn(std::string(&mesh_msg_data_[header_length + type_length + header_length], filename_length_));
            if (!(is_fn >> filename_)) return false;
            return true;
        }
        return false;
    }

    // encode message header
    void encode_header()
    {
        char header[header_length + 1] = "";
        std::sprintf(header, "%4lu", body_length_);
        std::memcpy(data_, header, header_length);
    }
    
private:
    std::vector<char> mesh_msg_data_ = std::vector<char>(header_length + type_length);
    std::string filename_;
    int message_type = -1;
    char data_[header_length + max_body_length];
    std::size_t body_length_;
    std::size_t filename_length_;
};

#endif
