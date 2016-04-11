//
//  timing_log.cpp
//  dist_scene
//
//  Created by Gabriele Palozzi on 06/11/14.
//  Copyright (c) 2014 Gabriele Palozzi. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <chrono>
#include <map>
#include <string>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>

using namespace std;

void restore_timelog(map<string,long long> &m, const string& filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the schedule from the archive
    ia >> m;
}

void print_timelog(map<string,long long> &m){
    for (auto record : m)
        cout << '\t' << record.first << " - " << record.second << endl;
}

double time_passed(long long from, long long to){
    return (to - from) / (double)1000000000;
}

double to_kb(long long size){
    return size / (double)1024;
}

void print_session_log(){
    //  timelogs filenames
    string server_filename = "../log/server_timing_v35";
    string sender_filename = "../log/sender_timing_v35";
    string receiver_filename = "../log/receiver_timing_v35";
    
    // timelogs structurs (map)
    map<string,long long> server_log;
    map<string,long long> sender_log;
    map<string,long long> receiver_log;
    
    // restore log in map
    restore_timelog(server_log, server_filename);
    restore_timelog(sender_log, sender_filename);
    restore_timelog(receiver_log, receiver_filename);
    
    cout << "server log:" << endl;
    print_timelog(server_log);
    cout << "sender log:" << endl;
    print_timelog(sender_log);
    cout << "receiver log:" << endl;
    print_timelog(receiver_log);
    
    cout << "\nserver local stats:" << endl;
    cout << "-\tserver parsing: " << time_passed(server_log["parse_scene[start]"], server_log["parse_scene[end]"]) << " s" << endl;
    cout << "-\tserver recive_mesh: " << time_passed(server_log["recive_mesh[start]"], server_log["recive_mesh[end]"]) << " s" << endl;
    cout << "-\tserver send all: " << time_passed(server_log["send_all[start]"], server_log["send_all[end]"]) << " s" << endl;
    cout << "\t-\tserver send_client_1: " << time_passed(server_log["send_client_1[start]"], server_log["send_client_1[end]"]) << " s" << endl;
    cout << "-\tserver deserialize_from_msg: " << time_passed(server_log["deserialize_from_msg[start]"], server_log["deserialize_from_msg[end]"]) << " s" << endl;
    cout << "-\tserver apply_diff: " << time_passed(server_log["apply_diff[start]"], server_log["apply_diff[end]"]) << " s" << endl;
    
    cout << "\nsender local stats:" << endl;
    cout << "-\tsender parsing: " << time_passed(sender_log["parse_scene[start]"], sender_log["parse_scene[end]"]) << " s" << endl;
    cout << "-\tsender deserialize_mesh: " << time_passed(sender_log["deserialize_mesh[start]"], sender_log["deserialize_mesh[end]"]) << " s" << endl;
    cout << "-\tsender apply_diff: " << time_passed(sender_log["apply_diff[start]"], sender_log["apply_diff[end]"]) << " s" << endl;
    cout << "-\tsender send_mesh: " << time_passed(sender_log["send_mesh[start]"], sender_log["send_mesh[end]"]) << " s" << endl;
    
    cout << "\nreceiver local stats:" << endl;
    cout << "-\treceiver parsing: " << time_passed(receiver_log["parse_scene[start]"], receiver_log["parse_scene[end]"]) << " s" << endl;
    cout << "-\treceiver recive_mesh: " << time_passed(receiver_log["recive_mesh[start]"], receiver_log["recive_mesh[end]"]) << " s" << endl;
    cout << "-\treceiver deserialize_from_msg: " << time_passed(receiver_log["deserialize_from_msg[start]"], receiver_log["deserialize_from_msg[end]"]) << " s" << endl;
    cout << "-\treceiver apply_diff: " << time_passed(receiver_log["apply_diff[start]"], receiver_log["apply_diff[end]"]) << " s" << endl;
    
    cout << "\nglobal stats:" << endl;
    cout << "\tsend -> receive: " << time_passed(sender_log["send_mesh[start]"], receiver_log["recive_mesh[end]"]) << " s" << endl;
    cout << "\tsender -> server: " << time_passed(sender_log["send_mesh[start]"], server_log["recive_mesh[end]"]) << " s" << endl;
    cout << "\tserver -> receiver: " << time_passed(server_log["send_client_1[start]"], receiver_log["recive_mesh[end]"]) << " s" << endl;
    cout << "\tsender_live -> receiver_live: " << time_passed(sender_log["apply_diff[end]"], receiver_log["apply_diff[end]"]) << " s" << endl;
    cout << "\tmessage size: " << to_kb(sender_log["mesh_size"]) << " kb" << endl;
}

void print_apply_log(const string& filename){
    // timelogs structurs (map)
    map<string,long long> app_log;
    
    // restore log in map
    restore_timelog(app_log, filename);
    
    cout << "server log:" << endl;
    print_timelog(app_log);
    
    cout << "\ntiming:" << endl;
    cout << "-\t save_history: " << time_passed(app_log["save_history[start]"], app_log["save_history[end]"]) << " s" << endl;
    cout << "-\t add_vertex: " << time_passed(app_log["add_vertex[start]"], app_log["add_vertex[end]"]) << " s" << endl;
    cout << "-\t update_vertex: " << time_passed(app_log["update_vertex[start]"], app_log["update_vertex[end]"]) << " s" << endl;
    cout << "-\t delete_vertex: " << time_passed(app_log["delete_vertex[start]"], app_log["delete_vertex[end]"]) << " s" << endl;
    cout << "-\t remove_edges: " << time_passed(app_log["remove_edges[start]"], app_log["remove_edges[end]"]) << " s" << endl;
    cout << "-\t remove_triangle: " << time_passed(app_log["remove_triangle[start]"], app_log["remove_triangle[end]"]) << " s" << endl;
    cout << "-\t remove_quad: " << time_passed(app_log["remove_quad[start]"], app_log["remove_quad[end]"]) << " s" << endl;
    cout << "-\t add_edge: " << time_passed(app_log["add_edge[start]"], app_log["add_edge[end]"]) << " s" << endl;
    cout << "-\t add_triangle: " << time_passed(app_log["add_triangle[start]"], app_log["add_triangle[end]"]) << " s" << endl;
    cout << "-\t add_quad: " << time_passed(app_log["add_quad[start]"], app_log["add_quad[end]"]) << " s" << endl;
    cout << "-\t indexing_vertex: " << time_passed(app_log["indexing_vertex[start]"], app_log["indexing_vertex[end]"]) << " s" << endl;
    cout << "-\t indexing_triangle: " << time_passed(app_log["indexing_triangle[start]"], app_log["indexing_triangle[end]"]) << " s" << endl;
    cout << "-\t indexing_quad: " << time_passed(app_log["indexing_quad[start]"], app_log["indexing_quad[end]"]) << " s" << endl;
    cout << "-\t indexing_edge: " << time_passed(app_log["indexing_edge[start]"], app_log["indexing_edge[end]"]) << " s" << endl;
    cout << "\n-\t indexing: " << time_passed(app_log["indexing[start]"], app_log["indexing[end]"]) << " s" << endl;
    cout << "\n-\t apply_diff: " << time_passed(app_log["apply_diff[start]"], app_log["apply_diff[end]"]) << " s" << endl;


}


int main(int argc, const char * argv[]) {
    print_apply_log("../log/apply_timing_v03");
    cout << "\n----------------------\n";
    print_apply_log("../log/apply_timing_v05");
    cout << "\n----------------------\n";
    print_apply_log("../log/apply_timing_v35");

    return 0;
}
