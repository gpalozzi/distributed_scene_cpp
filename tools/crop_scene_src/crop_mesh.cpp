// reading a text file
#include <iostream>
#include <fstream>
#include <string>
#include <boost/algorithm/string.hpp>
#include <vector>

using namespace std;
using namespace boost;

// unique ids
typedef unsigned long long timestamp_t;

int main (int argc, char** argv) {
    if (argc != 3)
    {
        std::cerr << "Usage: mesh_cropper <destination> <source> \n";
        return 1;
    }
    string line;
    vector<string> token;
    ofstream output;
    output.open(argv[1], ofstream::app);
    ifstream myfile (argv[2]);
    if (myfile.is_open())
    {
        while (getline (myfile,line)){
            split( token, line, is_any_of( " \n" ), token_compress_on );
            if (token.size() == 9) {
                output << token[0] << " " << token[1] << " "<< token[2] << " "<< token[3] << " "<< token[4] << "\n";
            } else if (token.size() == 10){
                output << token[0] << " " << token[1] << " "<< token[2] << " "<< token[3] << " "<< token[4] << " "<< token[5] << "\n";
            }
            else
                output << line << "\n";
        }
        
        myfile.close();
    }
    else
        cout << "Unable to open file";
    
    output.close();
    
    return 0;
}