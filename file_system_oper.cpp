//
//
// Created by selman.ozleyen2017 on 16.05.2020.

#include "args_reader.h"
#include <iostream>
#include <cstring>
#pragma pack (1)

using namespace std;
int main(int argc, const char ** argv){
    try {
        args_reader::file_oper(argc,argv);
    }
    catch (exception& e){
        if(errno)
            cerr << "Error: "<< strerror(errno) << endl;
        cerr << e.what() << endl;
    }

    return 0;
}
