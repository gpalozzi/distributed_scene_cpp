#ifndef tgl_server_h
#define tgl_server_h


#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <string>
#include <boost/asio.hpp>
#include "message.h"
#include "mesh_msg.hpp"


using boost::asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<op_message> op_message_queue;
typedef std::deque<mesh_msg> mesh_message_queue;

int id_gen = 1;

//----------------------------------------------------------------------

class chat_participant
{
public:
    int id = id_gen++;
    virtual ~chat_participant() {}
    virtual void deliver(const op_message& msg) = 0;
    virtual void deliver_mesh(const mesh_msg& m_msg) = 0;

};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
    void join(chat_participant_ptr participant)
    {
        participants_.insert(participant);
        //for (auto msg: edit_history_)
        //    participant->deliver(msg);
    }
    
    void leave(chat_participant_ptr participant)
    {
        participants_.erase(participant);
    }
    
    void deliver(const op_message& msg, chat_participant_ptr editor)
    {
        if(msg.body_length()){
            // track op history on scene
            edit_history_.push_back(msg);
            // put this op as pending
            pending_op_.push_back(msg);
            while (edit_history_.size() > max_recent_msgs)
                edit_history_.pop_front();
            
            // send op to partecipant
            for (auto participant: participants_)
                if (participant->id != editor->id) {
                    participant->deliver(msg);
                }
        }
    }
    
    void deliver_all(const mesh_msg& m_msg)
    {
        if(m_msg.body_length()){
            // send mesh to partecipant
            timing("send_all[start]");
            for (auto participant: participants_)
                    participant->deliver_mesh(m_msg);
            timing("send_all[end]");
            
        }
    }
    
    void deliver_mesh(const mesh_msg& m_msg, chat_participant_ptr editor)
    {
        if(m_msg.body_length()){
            // track meshes history on scene
            //meshes_history_.push_back(m_msg);
            // put this mesh as pending
            pending_meshes_.push_back(m_msg);
            //while (edit_history_.size() > max_recent_msgs)
            //    edit_history_.pop_front();
            
            // send mesh to partecipant
            timing("send_all[start]");
            for (auto participant: participants_)
                if (participant->id != editor->id) {
                    participant->deliver_mesh(m_msg);
                }
            timing("send_all[end]");

        }
    }
    
    bool has_pending_msg(){
        return !pending_op_.empty();
    }
    
    char* get_next_op(){
        if(!pending_op_.empty())
            return pending_op_.front().body();
        return nullptr;
    }
    
    void remove_first_op(){
        if(!pending_op_.empty())
            return pending_op_.pop_front();
    }
    
    bool has_pending_mesh(){
        return !pending_meshes_.empty();
    }
    
    Mesh* get_next_mesh(){
        if(!pending_meshes_.empty()){
            pending_meshes_.front().mesh(current_mesh);
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
    
    void remove_first_mesh(){
        if(!pending_meshes_.empty())
            return pending_meshes_.pop_front();
    }
    
    void remove_first(){
        if(!pending_meshes_.empty())
            return pending_meshes_.pop_front();
    }
    
    
    
private:
    std::set<chat_participant_ptr> participants_;
    enum { max_recent_msgs = 100 };
    op_message_queue edit_history_;
    op_message_queue pending_op_;
    //mesh_message_queue meshes_history_;
    mesh_message_queue pending_meshes_;
    Mesh current_mesh;
};

//----------------------------------------------------------------------

class edit_session
: public chat_participant,
public std::enable_shared_from_this<edit_session>
{
public:
    edit_session(tcp::socket socket, chat_room& room)
    : socket_(std::move(socket)),
    room_(room)
    {
    }
    
    void start()
    {
        room_.join(shared_from_this());
        
        // ALLOW read mesh and DISALLOW op message
        if(true)
            do_read_mesh_header();
        else
            do_read_header();
    }
    
    void deliver(const op_message& msg)
    {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress)
        {
            do_write();
        }
    }
    
    void deliver_mesh(const mesh_msg& m_msg)
    {
        bool write_in_progress = !write_meshes_.empty();
        write_meshes_.push_back(m_msg);
        if (!write_in_progress)
        {
            timing("send_client_" + to_string(shared_from_this()->id) + "[start]");
            do_write_mesh();
        }
    }
    
private:
    void do_read_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.data(), op_message::header_length),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec && read_msg_.decode_header())
                                    {
                                        do_read_body();
                                    }
                                    else
                                    {
                                        room_.leave(shared_from_this());
                                    }
                                });
    }
    
    void do_read_mesh_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_incoming_mesh_[0], mesh_msg::header_length + mesh_msg::type_length),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec && read_incoming_mesh_.decode_header())
                                    {
                                        timing("recive_mesh[start]");
                                        do_read_mesh_body();
                                    }
                                    else
                                    {
                                        room_.leave(shared_from_this());
                                    }
                                });
    }

    
    void do_read_body()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec)
                                    {
                                        // print recived op
                                        std::cout.write(read_msg_.body(), read_msg_.body_length());
                                        std::cout << "\n";
                                        // deliver op to all partecipant (except sender)
                                        room_.deliver(read_msg_, shared_from_this());
                                        do_read_header();
                                    }
                                    else
                                    {
                                        room_.leave(shared_from_this());
                                    }
                                });
    }

    void do_read_mesh_body()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_incoming_mesh_[mesh_msg::header_length + mesh_msg::type_length], read_incoming_mesh_.body_length()),
                                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                {
                                    if (!ec)
                                    {
                                        timing("recive_mesh[end]");
                                        // print recived op
                                        switch (read_incoming_mesh_.type()) {
                                            case 1: // Mesh
                                                message("mesh recived\n");
                                                break;
                                            case 2: // MeshDiff
                                                message("meshdiff recived\n");
                                                break;
                                            case 7: // OBJ - MTL
                                            {
                                                message("obj/mtl recived\n");
                                                error_if_not(read_incoming_mesh_.decode_body(), "error decoding obj/mtl body\n");
                                            }
                                                break;
                                            default:
                                                break;
                                        }
                                        timing("mesh_size", (long long) read_incoming_mesh_.length() );
                                        // deliver mesh to all partecipant (except sender)
                                        room_.deliver_mesh(read_incoming_mesh_,shared_from_this());
                                        do_read_mesh_header();
                                    }
                                    else
                                    {
                                        room_.leave(shared_from_this());
                                    }
                                });
    }
    
    void do_write()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(write_msgs_.front().data(),
                                                     write_msgs_.front().length()),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
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
                                         room_.leave(shared_from_this());
                                     }
                                 });
    }
    
    void do_write_mesh()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(write_meshes_.front()[0],
                                                     write_meshes_.front().length()),
                                 [this, self](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         write_meshes_.pop_front();
                                         timing("send_client_" + to_string(shared_from_this()->id) + "[end]");
                                         if (!write_meshes_.empty())
                                         {
                                             do_write_mesh();
                                         }
                                     }
                                     else
                                     {
                                         room_.leave(shared_from_this());
                                     }
                                 });
    }
    
    tcp::socket socket_;
    chat_room& room_;
    op_message read_msg_;
    mesh_msg read_incoming_mesh_;
    op_message_queue write_msgs_;
    mesh_message_queue write_meshes_;
};

//----------------------------------------------------------------------

class editor_server
{
public:
    editor_server(boost::asio::io_service& io_service,
                const tcp::endpoint& endpoint)
    : io_service_(io_service),
    acceptor_(io_service, endpoint),
    socket_(io_service)
    {
        std::cout << "Server started...\n";
        do_accept();
    }
    
    bool has_pending_op(){
        return room_.has_pending_msg();
    }
    
    char* get_next_pending_op(){
        return room_.get_next_op();
    }
    
    void remove_front_op(){
        return room_.remove_first_op();
    }
    
    void remove_first(){
        return room_.remove_first();
    }

    
    bool has_pending_mesh(){
        return room_.has_pending_mesh();
    }
    
    Mesh* get_next_mesh(){
        return room_.get_next_mesh();
    }
    
    mesh_msg* get_next_pending(){
        return room_.get_next_pending();
    }

    
    void remove_first_mesh(){
        return room_.remove_first_mesh();
    }
    
    template <typename T>
    void write_all(const T obj)
    {
                             auto m_msg = mesh_msg(obj);
                             room_.deliver_all(m_msg);
    }
    
    void close()
    {
        io_service_.post([this]() { socket_.close(); });
    }
    
private:
    void do_accept()
    {
        acceptor_.async_accept(socket_,
                               [this](boost::system::error_code ec)
                               {
                                   if (!ec)
                                   {
                                       // init edit session
                                       std::make_shared<edit_session>(std::move(socket_), room_)->start();
                                   }
                                   
                                   do_accept();
                               });
    }
    
    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    chat_room room_;
};

#endif

