#ifndef tgl_mesh_msg_hpp
#define tgl_mesh_msg_hpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

class mesh_msg
{
public:
    enum { header_length = 8 };
    enum { type_length = 3 };
    enum { max_body_length = 512 };
    enum { Mesh_type = 1, SubMesh_type = 2, Operation_type = 3 };
    
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
    
    mesh_msg(const SubMesh* submesh )
    {
        // Serialize the mesh.
        std::ostringstream archive_stream;
        boost::archive::text_oarchive archive(archive_stream);
        archive << *submesh;
        
        // Set message body length
        body_length_ = archive_stream.str().size();
        
        // create header
        std::ostringstream header_stream;
        header_stream << std::setw(header_length) << std::hex << body_length_;
        
        // set message type
        std::ostringstream type_stream;
        type_stream << std::setw(type_length) << std::hex << SubMesh_type;
        
        if(!header_stream || header_stream.str().size() != header_length){
            message("Header size > max header length for submesh (id: %llu)", submesh->_id_);
            body_length_ = 0;
        }
        else if (!type_stream || type_stream.str().size() != type_length){
            message("type size > max type length for submesh (id: %llu)", submesh->_id_);
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
            message_type = SubMesh_type;
        }
    }

    // get vector mesh position
    char* operator[](int i) { if (i < mesh_msg_data_.size()-1) return &mesh_msg_data_[i]; else message("Access to member out of bound"); return nullptr; }
    
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

    // get as submesh
    SubMesh* as_submesh() const
    {
        auto submesh = new SubMesh();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *submesh;
        
        return submesh;
    }
    
    SubMesh* as_submesh()
    {
        auto submesh = new SubMesh();
        std::string archive_data(&mesh_msg_data_[header_length + type_length], body_length_);
        std::istringstream archive_stream(archive_data);
        boost::archive::text_iarchive archive(archive_stream);
        archive >> *submesh;
        
        return submesh;
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
    
    // encode message header
    void encode_header()
    {
        char header[header_length + 1] = "";
        std::sprintf(header, "%4lu", body_length_);
        std::memcpy(data_, header, header_length);
    }
    
private:
    std::vector<char> mesh_msg_data_ = std::vector<char>(header_length + type_length);
    int message_type = -1;
    char data_[header_length + max_body_length];
    std::size_t body_length_;    
};

#endif
