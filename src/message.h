//
// op_message.h
// ~~~~~~~~~~~~~~~~
//

#ifndef OP_MESSAGE_H
#define OP_MESSAGE_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

class op_message
{
public:
    enum { header_length = 4 };
    enum { max_body_length = 512 };
    enum { operation_length = 1 };
    enum { max_object_id_length = 11 };
    enum { max_subobjects_id_length = 500 };
    

    op_message()
    : body_length_(0)
    {
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
        char header[header_length + 1] = "";
        std::strncat(header, data_, header_length);
        body_length_ = std::atoi(header);
        if (body_length_ > max_body_length)
        {
            body_length_ = 0;
            return false;
        }
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
    char data_[header_length + max_body_length];
    std::size_t body_length_;
    std::size_t object_id_length_;
    std::size_t subobject_id_length_;

};

#endif // OP_MESSAGE_H