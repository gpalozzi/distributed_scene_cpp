//
//  main.cpp
//  tester
//
//  Created by Gabriele Palozzi on 31/10/14.
//  Copyright (c) 2014 Gabriele Palozzi. All rights reserved.
//

#include <iterator>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

struct comparatorStruct {
    bool operator()(std::pair<const int, int>& a, std::pair<const int,int>& b){
        if (a.first == b.first and a.second == b.second) return true;
        return a.first < b.first;
//        return a.first < b.first or a.second < b.second;

    }
};


int main(int argc, const char * argv[]) {
    //II - defining two maps for comparison its keys using initialization lists
    std::map<int, int> firstMap = {  {1,1}, {2,4}, {3,9}, {4,16}, {5,25} };
    std::map<int, int> secondMap = { {1,1}, {2,3}, {4,12}, {10,3} };
    
    //III - declaring map which will contain difference
    std::map<int, int> diffMap;
    std::map<int, int> diffMap2;

    //IV - calculating difference between firstMap and secondMap basing
    //on map keys using comparatorStruct and write that difference to diffMap
    std::set_intersection(firstMap.begin(), firstMap.end(), secondMap.begin(), secondMap.end(), std::inserter(diffMap, diffMap.begin()), comparatorStruct());
    std::set_intersection(secondMap.begin(), secondMap.end(), firstMap.begin(), firstMap.end(), std::inserter(diffMap2, diffMap2.begin()), comparatorStruct());

    //V - writing difference map using auto C++11 feature
    for( auto it = diffMap.begin(); it != diffMap.end(); it++)
        std::cout << "key: " << it->first << "; value: " << it->second << "\n";
    
    std::cout << "\n";
    
    for( auto it = diffMap2.begin(); it != diffMap2.end(); it++)
        std::cout << "key: " << it->first << "; value: " << it->second << "\n";


}
