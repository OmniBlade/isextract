#include "isextract.h"
#include <iostream>

void printUse()
{
    std::cout << "Useage is \"isextract [mode] [file] (dir)\"\n"
              << "mode options are \'x\' for extract and \'l\' for list.\n";
}

int main(int argc, char** argv)
{
    std::string mode;
    std::string filepath;
    std::string outdir = "./";
    InstallShield infile;
    
    if(argc < 3) {
        printUse();
        return 0;
    }
    
    mode = argv[1];
    filepath = argv[2];
    
    if(argc >= 4) {
        outdir = argv[3];
    }
    
    try {
        infile.open(filepath);
    } catch (const char* msg) {
        std::cout << "Error: " << msg << "\n";
        return -1;
    }
    
    if(mode == "x"){
        infile.extractAll(outdir);
    } else if(mode == "l") {
        infile.listFiles();
    } else {
        printUse();
    }
    
    return 0;
}
