//
// Created by selman.ozleyen2017 on 16.05.2020.
//

#include <iostream>
#include <cstring>
#include "args_reader.h"
#include "file_system.h"
#pragma pack (1)

using namespace std;
int main(int argc, const char ** argv){
    try {
        int bs,ic;
        args_reader::mfs(argc,argv,&bs,&ic);
        file_system fs(bs,ic);
        fs.create_file(argv[3]);
    }
    catch (exception& e){
        if(errno)
            cerr << "Error: "<< strerror(errno) << endl;
        cerr << e.what() << endl;
    }

    return 0;
}
