//
//  main.cpp
//  tester
//
//  Created by Gabriele Palozzi on 31/10/14.
//  Copyright (c) 2014 Gabriele Palozzi. All rights reserved.
//

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <tuple>
#include <vector>
#include "common.h"
#include <boost/algorithm/string.hpp>
#include "obj_parser.h"


using namespace std;
int main(int argc, const char * argv[]) {
    
    string file = "/Users/gpalozzi/Desktop/shuttle.obj";
    auto scene = load_obj_scene(file);
   
    message("done\n");
    return 0;
//    std::chrono::time_point<std::chrono::system_clock> start, end;
//    start = std::chrono::system_clock::now();
//    end = std::chrono::system_clock::now();
//    
//    int elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>
//    (end-start).count();
//    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
//    
//    std::cout << "finished computation at " << std::ctime(&end_time)
//    << "elapsed time: " << elapsed_seconds << "s\n";

//    std::chrono::time_point<std::chrono::system_clock,std::chrono::duration<long long, std::milli>> end = std::chrono::system_clock::now();
//    std::time_t end_time = std::chrono::system_clock::to_time_t(end);
//
//    std::cout << "end time before " << std::ctime(&end_time);
//
//    std::string filename = "../test/chronotest";
//    std::ofstream output;
//    output.open(filename, std::ofstream::app);
//    if (output.is_open()){
//        output << (long long)end_time;
//        output.close();
//    }
//    
//    std::string line;
//    std::ifstream myfile (filename);
//    if (myfile.is_open()){
//        while ( getline (myfile,line)){
//            std::time_t clone = atoll(line.c_str());
//            std::cout << "end time after " << std::ctime(&clone);
//        }
//        myfile.close();
//    }
//    std::time_t t = std::time(nullptr);
//    auto start_time = std::chrono::system_clock::now();
//    long long as = start_time.time_since_epoch().count();
//    std::cout << "local: " << as << '\n';
//    std::time_t a = std::chrono::system_clock::to_time_t(start_time);
//    auto sec = std::localtime(&a)->tm_sec;
//    auto min = std::localtime(&a)->tm_min;
//    std::cout << "waiting..." << '\n';
//    while (not (min == 51 and sec == 20)) {
//        start_time = std::chrono::system_clock::now();
//        a = std::chrono::system_clock::to_time_t(start_time);
//        sec = std::localtime(&a)->tm_sec;
//        min = std::localtime(&a)->tm_min;
//    }
//    
//    std::cout << "local: " << sec << '\n';
    
//    std::chrono::time_point<std::chrono::high_resolution_clock,std::chrono::microseconds> start_time = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now());
//    auto test = std::chrono::high_resolution_clock::now();
//    std::cout << start_time.time_since_epoch().count() << " - " << test.time_since_epoch().count() << '\n';
//    std::time_t t = std::time(nullptr);
//    auto sec = std::localtime(&t)->tm_sec;
//    auto min = std::localtime(&t)->tm_min;
//    while (not (sec == 0)) {
//        start_time = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now());
//        t = std::time(nullptr);
//        sec = std::localtime(&t)->tm_sec;
//        min = std::localtime(&t)->tm_min;
//    }
//    
//    long long as = start_time.time_since_epoch().count();
//    std::cout << "time_point: " << as << '\n';
    
//    long long past = 114210804519587;
//    std::chrono::time_point<std::chrono::system_clock,std::chrono::duration<long long, std::chrono::microseconds>> start_time = std::chrono::high_resolution_clock::now();
//    std::time_t t = std::time(nullptr);
//    auto sec = std::localtime(&t)->tm_sec;
//    auto min = std::localtime(&t)->tm_min;
//    while (not (sec == 0)) {
//        start_time = std::chrono::high_resolution_clock::now();
//        t = std::time(nullptr);
//        sec = std::localtime(&t)->tm_sec;
//        min = std::localtime(&t)->tm_min;
//    }
//    
//    float time_passed = (start_time.time_since_epoch().count() - past) / 1000000000.0f;
//    std::cout << "past: " << past << '\n';
//    std::cout << "duration in s: " << time_passed << '\n';
    
//    std::time_t t = std::time(nullptr);
//    auto sec = std::localtime(&t)->tm_sec;
//    auto limit = (sec + 5) % 60;
//    while (not (sec == limit)) {
//        t = std::time(nullptr);
//        sec = std::localtime(&t)->tm_sec;
//    }
//
//    
//    std::chrono::time_point<std::chrono::high_resolution_clock,std::chrono::microseconds> start_micro = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now());
//    std::chrono::high_resolution_clock::time_point read_begin = std::chrono::high_resolution_clock::now();
//    long long l_start = start_micro.time_since_epoch().count();
//    long long simple_start = read_begin.time_since_epoch().count();
//
//    t = std::time(nullptr);
//    sec = std::localtime(&t)->tm_sec;
//    while (not (sec == (limit + 5) % 60)) {
//        t = std::time(nullptr);
//        sec = std::localtime(&t)->tm_sec;
//    }
//
//    std::chrono::time_point<std::chrono::high_resolution_clock,std::chrono::microseconds> end_micro = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now());
//    std::chrono::high_resolution_clock::time_point read_end = std::chrono::high_resolution_clock::now();
//    long long l_end = end_micro.time_since_epoch().count();
//    long long simple_end = read_end.time_since_epoch().count();
//    
//    auto d = read_end.time_since_epoch();
//
//    float time_long = (l_end - l_start) / 1000000.0f;
//    float time_long_simple = (simple_end - simple_start) / (float)std::chrono::high_resolution_clock::period::den;
//    double time_long_simple_d = (simple_end - simple_start) / (double)std::chrono::high_resolution_clock::period::den;
//
//    double time_point_t = (end_micro - start_micro).count() / 1000000.0f;
//    double time = std::chrono::duration_cast<std::chrono::microseconds>(end_micro - start_micro).count() / 1000000.0f;
//    std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(end_micro - start_micro);
//    double time_ms = ms.count() / 1000000.0f;
//    std::cout << "long: " << time_long << '\n';
//    std::cout << "simple_long: " << time_long_simple << '\n';
//    std::cout << "simple_long_double: " << time_long_simple_d << '\n';
//    std::cout << "time_point: " << time_point_t << '\n';
//    std::cout << "duration_cast: " << time << '\n';
//    std::cout << "microsecond: " <<  time_ms  << '\n';
//
//    std::cout << "done" << '\n';
//
//
//    auto start_time = std::chrono::high_resolution_clock::now();
//
//    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::microseconds> start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now());
//    
//    typedef std::chrono::high_resolution_clock::time_point deadline_t;
//    
//    deadline_t soon = start_time + std::chrono::seconds(2);
//    while (std::chrono::high_resolution_clock::now() < soon) { }
//    
//    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::microseconds> end = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now());
//
//    auto dur = start.time_since_epoch().count();
//    std::cout << start.time_since_epoch().count() << '\n';
//    
//    auto time_in_seconds = std::chrono::duration_cast<std::chrono::microseconds>(start_time.time_since_epoch()).count() / 1000000.0f;
//
//    std::cout << time_in_seconds << '\n';
    return 0;

};
