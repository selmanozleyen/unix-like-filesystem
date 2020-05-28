//
// Created by selman.ozleyen2017 on 16.05.2020.
//

#include <stdexcept>
#include <string>
#include <cmath>
#include "args_reader.h"
#include "file_system.h"

using namespace std;

void args_reader::mfs(int argc, const char **argv, int * bs, int * ic) {
    if(argc_no != argc)
        throw invalid_argument("Invalid argument number.");
    int block_size = stoi(argv[1]);
    double log_of_b = log2(block_size);
    // if it is not a power of two
    if (block_size < 0 || 0 > log2(block_size)  || (log_of_b - floor(log_of_b) != 0)){
        throw invalid_argument("Block size should be an positive integer which is power of two.");
    }
    //Calculating max inode count
    int max_inode = 0;
    double max_i = 0;
    int bs_byte = block_size*(1 << 10);
    max_i+= -2097152;
    max_i+= (double)1048588*bs_byte;
    if (1048588*((double)bs_byte) < 0)
        throw overflow_error("Given numbers are too big when calculating max inode count overflow happened.");
    max_i+= - 5*bs_byte*bs_byte;
    if(5*bs_byte*bs_byte < 0)
        throw overflow_error("Given numbers are too big when calculating max inode count overflow happened.");
    max_i/= 2*(-32+17*bs_byte);
    max_inode = floor(max_i);
    int cur_inode = stoi(argv[2]);
    if(max_inode < cur_inode)
        throw invalid_argument("Given I-node count is too large with the given block size..");
    if (cur_inode < 1)
        throw invalid_argument("Given I-node count is too small with the given block size.");

    *ic = cur_inode;
    *bs = block_size;
}

void args_reader::file_oper(int argc, const char **argv) {
    const char * filename = argv[1];
    if(argc < 3)
        throw invalid_argument("Please check your arguments.");
    if(argv[2] == string("list")){
        if(argc != 4)
            throw invalid_argument("list only needs one argument.");
        file_system fs(filename);
        fs.list_folders(argv[3]);
    }
    else if (argv[2] == string("mkdir")){
        if(argc != 4)
            throw invalid_argument("mkdir only needs one argument.");
        file_system fs(filename);
        fs.mkdir(argv[3]);
    }
    else if (argv[2] == string("rmdir")){
        if(argc != 4)
            throw invalid_argument("rmdir only needs one argument.");
        file_system fs(filename);
        fs.rmdir(argv[3]);
    }
    else if (argv[2] == string("dumpe2fs")){
        if(argc != 3)
            throw invalid_argument("No arguments are required with dump2fs.");
        file_system fs(filename);
        fs.dumpe2fs();
    }
    else if (argv[2] == string("write")){
        if(argc != 5)
            throw invalid_argument("write needs 2 arguments.");
        file_system fs(filename);
        fs.copy_file(argv[3],argv[4]);
    }
    else if (argv[2] == string("read")){
        if(argc != 5)
            throw invalid_argument("read needs 2 arguments.");
        file_system fs(filename);
        fs.read_file(argv[3],argv[4]);
    }
    else if (argv[2] == string("ln")){
        if(argc != 5)
            throw invalid_argument("ln needs 2 arguments.");
        file_system fs(filename);
        fs.hard_link(argv[3],argv[4]);
    }
    else if (argv[2] == string("lnsym")){
        if(argc != 5)
            throw invalid_argument("lnsym needs 2 arguments.");
        file_system fs(filename);
        fs.soft_link(argv[3],argv[4]);
    }
    else if (argv[2] == string("fsck")){
        if(argc != 5)
            throw invalid_argument("No arguments are required with fsck.");
        file_system fs(filename);
        fs.fsck();
    }
    else{
        throw invalid_argument("Unrecognized command.");
    }
}
