//
//  client.hpp
//  tgl
//
//  Created by Gabriele Palozzi on 23/10/14.
//  Copyright (c) 2014 Sapienza. All rights reserved.
//

#ifndef tgl_client_hpp
#define tgl_client_hpp

//
// editor_client.cpp
//


#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include "message.h"
#include "scene_distributed.h"
#include "mesh_msg.hpp"

using boost::asio::ip::tcp;

typedef std::deque<op_message> op_message_queue;
typedef std::deque<mesh_msg> mesh_message_queue;

class editor_client
{
public:
    // constructor
    editor_client(boost::asio::io_service& io_service,
                  tcp::resolver::iterator endpoint_iterator)
    : io_service_(io_service),
    socket_(io_service)
    {
        // accept incoming communication
        do_connect(endpoint_iterator);
    }
    
    // write msg
    void write(const op_message& msg)
    {
        io_service_.post(
                         [this, msg]()
                         {
                             bool write_in_progress = !write_msgs_.empty();
                             // push into write obj
                             write_msgs_.push_back(msg);
                             if (!write_in_progress)
                             {
                                 do_write();
                             }
                         });
    }
    
    // send mesh on socket
    void write(const Mesh* mesh)
    {
        io_service_.post(
                         [this, mesh]()
                         {
                             
                             // build serialized mesh
                             // build header
                             auto m_msg = mesh_msg(mesh);
                             // check if write in progress
                             bool write_in_progress = !write_meshes_.empty();
                             // push into mesh to write
                             write_meshes_.push_back(m_msg);
                             if (!write_in_progress)
                             {
                                 // write on socket
                                 // need to unify do_write and do_write_mesh function!!!!
                                 do_write_mesh();
                             }
                         });
    }
    
    // send submesh on socket
    void write(const SubMesh* submesh)
    {
        io_service_.post(
                         [this, submesh]()
                         {
                             
                             // build serialized mesh
                             // build header
                             auto m_msg = mesh_msg(submesh);
                             // check if write in progress
                             bool write_in_progress = !write_meshes_.empty();
                             // push into mesh to write
                             write_meshes_.push_back(m_msg);
                             if (!write_in_progress)
                             {
                                 // write on socket
                                 // need to unify do_write and do_write_mesh function!!!!
                                 do_write_mesh();
                             }
                         });
    }
    
    bool has_pending_mesh(){
        return !pending_meshes_.empty();
    }
    
    Mesh* get_next_mesh(){
        if(!pending_meshes_.empty()){
            current_mesh = *pending_meshes_.front().as_mesh();
            return &current_mesh;
        }
        return nullptr;
    }
    
    mesh_msg* get_next_pending(){
        if(!pending_meshes_.empty()){
            return &pending_meshes_.front();

        }
        return nullptr;
    }
    
    void remove_first(){
        if(!pending_meshes_.empty())
            return pending_meshes_.pop_front();
    }
    
    void close()
    {
        io_service_.post([this]() { socket_.close(); });
    }
    
private:
    void do_connect(tcp::resolver::iterator endpoint_iterator)
    {
        boost::asio::async_connect(socket_, endpoint_iterator,
                                   [this](boost::system::error_code ec, tcp::resolver::iterator)
                                   {
                                       if (!ec)
                                       {
                                           //do_read_header();
                                           // need to unify do_read_header and do_read_mesh_header function!!!!
                                           do_read_mesh_header();
                                       }
                                   });
    }
    
    void do_read_header()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.data(), op_message::header_length),
                                [this](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec && read_msg_.decode_header())
                                    {
                                        do_read_body();
                                    }
                                    else
                                    {
                                        socket_.close();
                                    }
                                });
    }
    
    void do_read_mesh_header()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_incoming_mesh_[0], mesh_msg::header_length + mesh_msg::type_length),
                                [this](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec && read_incoming_mesh_.decode_header())
                                    {
                                        do_read_mesh_body();
                                    }
                                    else
                                    {
                                        socket_.close();
                                    }
                                });
    }
    
    
    void do_read_body()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                                [this](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec)
                                    {
                                        std::cout.write(read_msg_.body(), read_msg_.body_length());
                                        std::cout << "\n";
                                        // put in pending mesh to resolve
                                        do_read_header();
                                    }
                                    else
                                    {
                                        socket_.close();
                                    }
                                });
    }
    
    void do_read_mesh_body()
    {
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_incoming_mesh_[mesh_msg::header_length + mesh_msg::type_length],
                                                    read_incoming_mesh_.body_length()),
                                [this](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec)
                                    {
                                        std::cout << "message recived\n";
                                        // ack?
                                        pending_meshes_.push_back(read_incoming_mesh_);
                                        do_read_mesh_header();
                                    }
                                    else
                                    {
                                        socket_.close();
                                    }
                                });
    }
    
    void do_write()
    {
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(write_msgs_.front().data(),
                                                     write_msgs_.front().length()),
                                 [this](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         write_msgs_.pop_front();
                                         if (!write_msgs_.empty())
                                         {
                                             do_write();
                                         }
                                     }
                                     else
                                     {
                                         socket_.close();
                                     }
                                 });
    }
    
    // write on socket
    void do_write_mesh()
    {
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(write_meshes_.front()[0],
                                                     write_meshes_.front().length()),
                                 [this](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         write_meshes_.pop_front();
                                         if (!write_meshes_.empty())
                                         {
                                             do_write_mesh();
                                         }
                                     }
                                     else
                                     {
                                         socket_.close();
                                     }
                                 });
    }
    
private:
    boost::asio::io_service& io_service_;
    tcp::socket socket_;
    op_message read_msg_;
    mesh_msg read_incoming_mesh_;
    op_message_queue write_msgs_;
    mesh_message_queue write_meshes_;
    mesh_message_queue pending_meshes_;
    Mesh current_mesh;
};

#endif
