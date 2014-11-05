// reading a text file
#include <iostream>
#include <fstream>
#include <string>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <sys/time.h>
#include "mesh_parser.h"

using namespace std;
using namespace boost;

// unique ids
typedef unsigned long long timestamp_t;

int main (int argc, char** argv) {
    if (argc != 3)
    {
        std::cerr << "Usage: mesh_parser <destination> <source> \n";
        return 1;
    }
    string line;
    vector<string> token;
    int counter = 0;
    ofstream output;
    output.open(argv[1], ofstream::app);
    ifstream myfile (argv[2]);
    if (myfile.is_open())
    {
        getline(myfile,line);
        //vertex parser
        counter = atoi(line.c_str());
        cout << "vertex " << counter << "\n";
        output << "\t\t\"n_vertex\": " << counter << ",\n";
        output << "\t\t\"vertex\": [\n";
        while ( getline (myfile,line) && counter-- > 0  )
        {
            split( token, line, is_any_of( " \n" ), token_compress_on );
            if (counter == 0)
                output << "\t\t\t{ \"_id_\": " << token[0] << ", " << "\"pos\": [" << token[1] << "," << token[2] << "," << token[3] << "] }\n";
            else
                output << "\t\t\t{ \"_id_\": " << token[0] << ", " << "\"pos\": [" << token[1] << "," << token[2] << "," << token[3] << "] },\n";
        }
        output << "\t\t],\n";
        
        //edges parser
        counter = atoi(line.c_str());
        cout << "edges " << counter << "\n";
        output << "\t\t\"n_edges\": " << counter << ",\n";
        output << "\t\t\"edges\": [\n";
        while ( getline (myfile,line) && counter-- > 0  )
        {
            split( token, line, is_any_of( " \n" ), token_compress_on );
            if (counter == 0)
                output << "\t\t\t{ \"_id_\": " << token[0] << ", " << "\"ver_ids\": [" << token[1] << "," << token[2] << "] }\n";
            else
                output << "\t\t\t{ \"_id_\": " << token[0] << ", " << "\"ver_ids\": [" << token[1] << "," << token[2] << "] },\n";
        }
        output << "\t\t],\n";
        
        //triangle parser
        bool check = true;
        
        auto cur = myfile.tellg();
        int t = 0;
        int tot = counter = atoi(line.c_str());
        cout << "faces " << counter << "\n";
        output << "\t\t\"triangle\": [\n";
        while (counter-- > 0 && getline (myfile,line))
        {
            if (counter == tot -1) cout << "first face ->" << line << "\n";
            if (counter == 0) cout << "last face ->" << line << "\n";
            split( token, line, is_any_of( " \n" ), token_compress_on );
            if (token.size() == 9){
                if (check){
                    check = false;
                    cout << "first triangle ->" << line << "\n";
                }

                output << "\t\t\t{ \"_id_\": " << token[0] << ", " << "\"ver_ids\": [" << token[2] << "," << token[3] << "," << token[4] << "] },\n";
                t++;
            }
            if (token.size() != 9 && token.size() != 10){
                cout << "ERROR not a face ->" << line << "\n";
            }
        }
        output << "\t\t],\n";
        output << "\t\t\"n_triangle\": " << t << ",\n";

        cout << "\t- triangle " << t << "\n";

        //quad parser
        check = true;
        myfile.seekg(cur, myfile.beg);
        counter = tot;
        int q = 0;
        output << "\t\t\"n_quad\": " << tot-t << ",\n";
        output << "\t\t\"quad\": [\n";
        while (counter-- > 0 && getline (myfile,line))
        {
            if (counter == 0) cout << "last face ->" << line << "\n";
            split( token, line, is_any_of( " \n" ), token_compress_on );
            if (token.size() == 10){
                if (check){
                    check = false;
                    cout << "first quad ->" << line << "\n";
                }
                output << "\t\t\t{ \"_id_\": " << token[0] << ", " << "\"ver_ids\": [" << token[2] << "," << token[3] << "," << token[4] << "," << token[5] << "] },\n";
                q++;
            }
        }
        output << "\t\t],\n";

        cout << "\t- quad " << q << "\n";
        if (tot == q+t)
            cout << "check OK --> " << tot << " = " << q << " + " << t << "\n";
        else
            cerr << "check BAD --> " << tot << " != " << q << " + " << t << "\n";
        
        myfile.close();
    }
    else
        cout << "Unable to open file";
    
    output.close();

    return 0;
}