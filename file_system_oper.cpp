//
//
// Created by selman.ozleyen2017 on 16.05.2020.

#include "args_reader.h"

#include <iostream>

using namespace std;
int main(int argc, const char ** argv){
    args_reader::file_oper(argc,argv);
    try {
    }
    catch (exception& e){
        cerr << e.what() << endl;
    }

    return 0;
}
