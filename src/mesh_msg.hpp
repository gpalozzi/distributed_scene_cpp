//
//  mesh_msg.hpp
//  tgl
//
//  Created by Gabriele Palozzi on 22/10/14.
//  Copyright (c) 2014 Sapienza. All rights reserved.
//

#ifndef tgl_mesh_msg_hpp
#define tgl_mesh_msg_hpp
//
// op_message.h
// ~~~~~~~~~~~~~~~~
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

class mesh_msg
{
public:
    enum { header_length = 8 };
    enum { max_body_length = 512 };
    
    mesh_msg()
    : body_length_(0)
    {
    }
    
    mesh_msg(const Mesh* mesh )
    {
        // Serialize the mesh.
        std::ostringstream archive_stream;
        boost::archive::text_oarchive archive(archive_stream);
        archive << *mesh;
        
        body_length_ = archive_stream.str().size();
        
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << body_length_;
        
        if(!header_stream || header_stream.str().size() != header_length){
            message("Header size > max header length for mesh (id: %llu)", mesh->_id_);
            body_length_ = 0;
        }
        else{
            mesh_msg_data_.resize(body_length_ + header_length);
            
            //copy header & mesh data
            std::memcpy(&mesh_msg_data_[0], header_stream.str().c_str(), header_length);
            std::memcpy(&mesh_msg_data_[header_length], archive_stream.str().c_str(), body_length_);
        }
    }
    
    // get vector mesh position
    char* operator[](int i) { if (i < mesh_msg_data_.size()-1) return &mesh_msg_data_[i]; else message("Access to member out of bound"); return nullptr; }
    
    // get all data
    const void mesh(Mesh &mesh) const
    {
        std::string archive_data(&mesh_msg_data_[header_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> mesh;
    }
    
     void mesh(Mesh &mesh)
    {
        std::string archive_data(&mesh_msg_data_[header_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> mesh;
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
        return header_length + body_length_;
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
        if (!(is >> std::hex >> body_length_)) return false;
        mesh_msg_data_.resize(body_length_ + header_length);
        return true;
    }
    
    // encode message header
    void encode_header()
    {
        char header[header_length + 1] = "";
        std::sprintf(header, "%4lu", body_length_);
        std::memcpy(data_, header, header_length);
    }
    
private:
    std::vector<char> mesh_msg_data_ = std::vector<char>(header_length);
    char data_[header_length + max_body_length];
    std::size_t body_length_;    
};

#endif
