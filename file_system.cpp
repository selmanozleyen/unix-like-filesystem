//
// Created by selman.ozleyen2017 on 17.05.2020.
//

#include <fstream>
#include "file_system.h"
#include "args_reader.h"
#include <ctime>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <utility>


using namespace std;


const char file_system::months[][4]= {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Assumes the parameters are correct */
file_system::file_system(size_t block_size, size_t inode_count) {

    inodes.resize(inode_count);
    sb.inode_count = (uint16_t)inode_count;
    sb.free_inode_count = ((uint16_t)inode_count) - 1;
    sb.block_size = block_size;
    sb.inode_pos = 1;
    block_size_byte = 1024 * block_size;
    node_cap = block_size_byte / 2 - 1;
    block_cap = node_cap + 1;
    size_t inodes_block_count = ceil(((double)inode_size * inode_count) / ((double)block_size_byte));
    size_t inodes_pos_end = sb.inode_pos + inodes_block_count;

    sb.inode_head = inodes_pos_end;
    size_t free_inodes_block_count = ceil((2 * ((double)inode_count) - 2) / ((double)block_size_byte - 2));
    sb.inode_tail = sb.inode_head + free_inodes_block_count - 1;
    if (sb.inode_head > sb.inode_tail)
        throw length_error("Error calculating the block positions.");

    sb.root_dir_address = sb.inode_tail + 1;
    size_t total_blocks = (1024) / block_size;
    // After filling inodes
    size_t last_free_block = total_blocks - 2;
    // position of the first free block
    size_t fb_pos = sb.root_dir_address + 1;
    size_t node_address_count = block_size_byte / 2 - 2;
    for (size_t i = fb_pos, j = 0; i < last_free_block + 1; ++i, ++j) {
        if (j == node_address_count + 1) {
            j = 0;
            last_free_block--;
        }
    }
    sb.fb_head = total_blocks - 1;
    sb.fb_tail = last_free_block + 1;
    sb.fb_count = sb.fb_tail - fb_pos;
    //fill root dir inode
    if (fb_pos > sb.fb_tail || sb.fb_count < 1)
        throw invalid_argument("I-node count is too big.");
    init_inode(0);
    // one for . and one for ..
    inodes[0].size = data_block::dir_entry_size*2;
    inodes[0].ba[0] = sb.root_dir_address;
    inodes[0].type = dir_type;
    inodes[0].link_count = 1;
}

void file_system::create_file(const char* filename_arg)  {

    /* Initial Layout Order: SB -> INODES -> INODES_LIST -> ROOT_DIR -> FREE BLOCKS -> FREE_BLOCKS_LIST*/

    ofstream file;
    file.open(filename_arg, ios::binary | ios::out);
    file.exceptions ( std::ios::failbit | std::ios::badbit );
    file.write((char*)&sb, sizeof(superblock));
    if (sizeof(superblock) < block_size_byte) {
        // Note: Won't work on machines where char is not 1 byte.
        size_t remaining = block_size_byte - sizeof(superblock);
        char* temp = new char[remaining];
        file.write(temp, remaining);
        delete[] temp;
    }
    const inode* iarr = inodes.data();
    size_t inode_blks = sb.inode_head - sb.inode_pos;
    if (sb.inode_head < sb.inode_pos)
        throw std::length_error("Couldn't calculate inode_blocks.");
    file.write((char*)iarr, sizeof(inode) * sb.inode_count);
    if (inode_blks * block_size_byte > sizeof(inode) * sb.inode_count) {
        size_t remaining = inode_blks * block_size_byte - sizeof(inode) * sb.inode_count;
        char* temp = new char[remaining];
        file.write(temp, remaining);
        delete[] temp;
    }
    size_t free_inodes_blocks = sb.root_dir_address - sb.inode_head;
    if (sb.root_dir_address < sb.inode_tail)
        throw std::length_error("Couldn't calculate free inode blocks.");

    // Going to write block by block the free block nodes
    char * zero_chars = new char[block_size_byte];
    for (size_t i = 0; i < block_size_byte;i++) {
        zero_chars[i] = 0;
    }
    size_t cap = block_size_byte / 2 - 1;
    uint16_t j = 1;
    for (size_t i = sb.inode_head; i < free_inodes_blocks + sb.inode_head; ++i) {
        //filling the block first
        data_block temp(zero_chars,0,block_size_byte,0);
        for (size_t k = 0; k < cap; ++k) {
            if (sb.free_inode_count + 1 > j)
                temp.push_address(j++);
            else
                break;
        }
        // if it is not the last loop
        if (i + 1 < free_inodes_blocks)
            temp.set_address(node_cap,i+1);

        file.write(temp.arr, block_size_byte);
    }
    // Root directory data block currently has . and .. dir entries
    data_block temp(zero_chars,0,block_size_byte,sb.root_dir_address);
    init_directory(temp,0,0);
    file.write((char*)temp.arr, block_size_byte);
    data_block zero(zero_chars,0,block_size_byte,0);
    // Root directory is written it turn for writing free blocks
    for (size_t i = sb.root_dir_address + 1; i < sb.fb_tail; ++i) {
        file.write((char*)zero.arr, block_size_byte);
    }

    // this is the address for the first free block
    j = sb.root_dir_address + 1;
    // Now writing the free block nodes
    for (uint16_t i = sb.fb_tail; i <= sb.fb_head; ++i) {
        data_block temp1(zero_chars,0,block_size_byte,0);
        for (size_t k = 0; k < cap; k++) {
            if (sb.fb_tail > j)
                temp1.push_address(j++);
            else
                break;
        }
        if (i != sb.fb_head)
            temp1.set_address(node_cap,i+1);
        file.write(temp1.arr, block_size_byte);
    }
    delete[] zero_chars;
    file.close();
}

void file_system::init_inode(size_t i) {
    set_inode_time(i);
    inodes[i].type = empty_type;
    inodes[i].size = 0;

    inodes[i].link_count = 0;

    inodes[i].si = 0;
    inodes[i].di = 0;
    inodes[i].ti = 0;

    for (auto & j : inodes[i].ba){
        j = 0;
    }

}

std::vector<char> file_system::create_dir_entry(uint16_t index,const std::string& dirname) const {
    if (dirname.size() > 6)
        throw invalid_argument("Directory name cannot be longer than 6 characters.");
    if ((dirname.find('/') != string::npos) || (dirname.find(' ') != string::npos))
        throw invalid_argument("Directory name is invalid.");
    vector<char> res{0,0,0,0,0,0,0,0};
    res[1] = (uint8_t)(index % data_block::one_byte);
    res[0] = (uint8_t)((index >> 8) % data_block::one_byte);
    size_t i = 0;
    for(char s: dirname)
        res[2+(i++)] = s;
    for (i = i+2; i < data_block::dir_entry_size; ++i)
        res[i] = 0;

    return res;
}
//size will be evaluated with its first 24 bits
void file_system::add_inode_size(size_t index, uint32_t size)
{
    inode & i = inodes[index];
    i.size+=size;
}

file_system::file_system(const char* filename) {
    this->filename = filename;
    fstream file(filename,ios::binary| ios::out | ios::in);
    // reading the superblock
    file.read((char*)&sb, sizeof(sb));
    block_size_byte = (sb.block_size << 10);
    node_cap = block_size_byte / 2 - 1;
    block_cap = node_cap + 1;
    //reading inodes

    file.seekg(sb.inode_pos * block_size_byte);
    auto* temp_inodes = new inode[sb.inode_count];
    file.read((char*)temp_inodes, ((size_t)sb.inode_count)* ((size_t) inode_size));
    for (size_t i = 0; i < sb.inode_count; ++i) {
        inodes.emplace_back(temp_inodes[i]);
    }
    //file.seekg(43488);
    //file.read((char *)&temp_inodes[0],32);

    delete[] temp_inodes;
    file.close();
}

uint16_t file_system::get_dir_inode(std::string path) {
    if (path == "/")
        return 0;
    // remove the front '/'
    path = path.erase(0,1);
    return get_dir_inode_helper(path, inodes[0]);
}

uint16_t file_system::get_dir_inode_helper(std::string& path,const inode& i) {
    size_t slash_i = path.find('/');
    //load the blocks that inode is referring to
    //resets inode blocks
    inode_blocks.clear();
    load_inode_blocks(i);
    // if we are on the last level
    bool last_level = string::npos == slash_i || path.size() == slash_i + 1;
    string searched = path;
    if (last_level) {
        //if it is a directory remove the last '/'
        if (path.size() == slash_i + 1) {
            searched.pop_back();
        }
    }
    else {
        // select next dir name
        searched = string(path, 0, slash_i);
        // delete the selected one
        path = path.substr(slash_i, path.size() - slash_i);
    }

    for (auto& in : inode_blocks) {
        for (size_t j = 0; j < in.get_dir_entry_count(); ++j) {
            if (in.get_entry_name(j) == searched) {
                if (last_level) {
                    //resets inode blocks
                    size_t res = in.get_entry_inode_no(j);
                    inode_blocks.clear();
                    return res;
                }
                else {
                    string new_path = string(path,1,path.size());
                    return get_dir_inode_helper(new_path, inodes[in.get_entry_inode_no(j)]);
                }
            }
        }
    }
    throw invalid_argument("No such directory.");
}

// changes inode blocks
void file_system::load_inode_blocks(inode i) {
    size_t size = get_inode_size(i);
    auto block_count = (size_t) ceil((double)size / (double)block_size_byte);
    size_t rem_block_count = block_count;
    size_t j = 0;
    for (j = 0; j < direct_count && rem_block_count > 1; ++j) {
        load_by_block_no(i.ba[j], block_size_byte);
        inode_blocks.emplace_back(temp_blocks.back());
        temp_blocks.pop_back();
        size -= block_size_byte;
        rem_block_count--;
    }
    if (rem_block_count == 1 && j < direct_count) {
        load_by_block_no(i.ba[j], size);
        inode_blocks.emplace_back(temp_blocks.back());
        temp_blocks.pop_back();
        return;
    }
    if (rem_block_count == 0)
        return;
    // if there is still blocks to load use indirect blocks
    if (i.si == 0)
        throw logic_error("I-node structure and the attributes doesn't match");
    load_inode_blocks_helper(i.si, &size, &rem_block_count, 1);
    if (rem_block_count == 0)
        return;
    // If there is still blocks remaining use double indirect blocks
    if (i.di == 0)
        throw logic_error("I-node structure and the attributes doesn't match");
    load_inode_blocks_helper(i.di, &size, &rem_block_count, 2);
    if (rem_block_count == 0)
        return;
    // If there is still blocks remaining use double indirect blocks
    if (i.ti == 0)
        throw logic_error("I-node structure and the attributes doesn't match");
    load_inode_blocks_helper(i.ti, &size, &rem_block_count, 3);
    if (rem_block_count == 0)
        return;
}

void file_system::load_by_block_no(size_t bno, size_t size = 0) {
    ifstream file(filename,ios::binary| ios::in);
    file.exceptions ( std::ios::failbit | std::ios::badbit );
    file.seekg(bno*block_size_byte);
    char* temp = new char[block_size_byte];
    file.read(temp, block_size_byte);
    file.close();
    data_block res(temp, size, block_size_byte, bno);
    temp_blocks.emplace_back(res);
    delete[] temp;
}

void file_system::write_block(const data_block& b) const
{
    ofstream file(filename,ios::binary| ios::out | ios::in);
    file.exceptions ( std::ios::failbit | std::ios::badbit );
    file.seekp(b.bno*block_size_byte);
    file.write((char *)b.arr, block_size_byte);
    file.close();
}

void file_system::write_superblock()
{
    ofstream file(filename, ios::binary| ios::out | ios::in);
    file.exceptions ( std::ios::failbit | std::ios::badbit );
    file.write((char *)&sb, block_size_byte);
    file.close();
}

void file_system::write_inode(uint16_t ino)
{
    // find which block ino is in
    size_t ino_addr = ino*sizeof(inode) + sb.inode_pos * block_size_byte;
    fstream file(filename, ios::binary| ios::out | ios::in);
    file.exceptions ( std::ios::failbit | std::ios::badbit );
    file.seekp(ino_addr);
    file.write((char *)&inodes[ino],sizeof(inode));
    file.close();
}

uint32_t file_system::get_inode_size(const inode& i) {
    return i.size;
}

void file_system::load_inode_blocks_helper(size_t bno, size_t* size, size_t* rem_blocks, size_t level) {
    if (*rem_blocks == 0) {
        return;
    }
    if (level == 0) {
        if (*rem_blocks == 0) {
            load_by_block_no(bno, *size);
            inode_blocks.push_back(temp_blocks.back());
            temp_blocks.pop_back();
            *rem_blocks = 0;
            *size = 0;
            return;
        }
        load_by_block_no(bno, block_size_byte);
        inode_blocks.push_back(temp_blocks.back());
        temp_blocks.pop_back();
        *size -= block_size_byte;
        *rem_blocks -= 1;
    }
    else {
        load_by_block_no(bno, block_size_byte);
        data_block temp = temp_blocks.back();
        temp_blocks.pop_back();
        for (size_t i = 0; i < block_cap && *rem_blocks > 0; ++i) {
            load_inode_blocks_helper(temp.get_address(i), size, rem_blocks, level - 1);
        }
    }
}

void file_system::mkdir(const std::string& arg) {
    size_t parent;
    string name,path;
    // check the parameter
    new_file_args(arg, path, name, parent);
    //get free inode
    uint16_t newi = get_free_inode();
    init_inode(newi);
    data_block temp(block_size_byte);
    init_directory(temp,newi,parent);
    write(newi,0,data_block::dir_entry_size*2,temp.arr);
    vector<char> dir_ent = create_dir_entry(newi,name);
    //write the data blocks
    uint32_t pos = get_inode_size(inodes[parent]);
    write(parent,pos,data_block::dir_entry_size,dir_ent.data());
    set_inode_time(parent);
    add_inode_size(parent,data_block::dir_entry_size);
    write_inode(newi);
    write_inode(parent);
}

void file_system::write(uint16_t inode_index, uint32_t pos, uint32_t size,const char* buf)
{
    //cursor for the buffer
    size_t buf_pos = 0;
    inode & in = inodes[inode_index];
    // store the remaining block to write the remainder to a block specially
    size_t cur_block;
    size_t file_size = get_inode_size(inodes[inode_index]);
    if(file_size >= max_file_size)
        throw std::runtime_error("File is too large.");
    if(pos > file_size){
        throw std::runtime_error("File point cannot be greater than size.");
    }
    size_t off_di = block_cap*block_size_byte + direct_count*block_size_byte;
    size_t off_ti = off_di + block_cap*block_cap*block_size_byte;
    if((pos >> (10 + sb.block_size)) < direct_count+1){
        cur_block = pos >> (10 + sb.block_size);
    }
        // if it starts from double indirect
    else if( (pos >= off_di) && (pos - off_di) <
                                (block_cap*block_cap*block_size_byte))
    {
        cur_block = direct_count;
    }
        // if it starts from triple indirect
    else if((pos >= off_ti) && (pos - off_ti)  <
                               (block_cap*block_cap*block_cap*block_size_byte))
    {
        cur_block = direct_count+1;
    }
    else{
        throw runtime_error("Position is too large.");
    }
    // block by block operation
    for (; cur_block < direct_count && size > 0; cur_block++){
        // if there are no blocks allocated yet
        if(in.ba[cur_block] == 0){
            //will write the inode later
            in.ba[cur_block] = get_free_block();
        }
        load_by_block_no(in.ba[cur_block]);
        data_block & temp = temp_blocks.back();

        // assign the buf to the block
        for (size_t i = pos % block_size_byte; i < block_size_byte && size > 0 ; ++i, ++pos){
            temp.arr[i] = buf[buf_pos++];
            size--;
        }
        // put it to the buffer to write
        inode_blocks.emplace_back(temp);
        temp_blocks.pop_back();
    }
    // if the job is done it can exit
    if(size == 0){
        write_inode_blocks_buffer();
        write_inode(inode_index);
        write_superblock();
        return;
    }
    // now it is part for the recursive indirect part
    // if there is none allocate
    size_t off = direct_count*block_size_byte;
    size_t wb_size = write_buffer_size;
    if(cur_block == direct_count){
        if(in.si == 0){
            in.si = get_free_block();
        }
        size_t rel_block = ((pos - off) >> (10 + sb.block_size)) % block_size_byte;
        write_helper(in.si,&pos,&size,buf,rel_block,1,off,&wb_size,&buf_pos);
        cur_block++;
    }
    // if the job is done it can exit
    if(size == 0){
        write_inode_blocks_buffer();
        write_inode(inode_index);
        write_superblock();
        return;
    }
    if(cur_block == direct_count+1){
        if(in.di == 0){
            in.di = get_free_block();
        }
        off += block_size_byte*block_size_byte/2;
        size_t rel_block = ((pos - off) >> 2*(10 + sb.block_size)) % block_size_byte;
        if(pos < off)
            throw overflow_error("Overflow happened probably file is too big.");
        write_helper(in.di,&pos,&size,buf,rel_block,2,off,&wb_size,&buf_pos);
        cur_block++;
    }
    // if the job is done it can exit
    if(size == 0){
        write_inode_blocks_buffer();
        write_inode(inode_index);
        write_superblock();
        return;
    }
    if(cur_block == 9){
        if(in.ti == 0){
            in.ti = get_free_block();
        }
        off += block_size_byte*block_size_byte*block_size_byte/4;
        size_t rel_block = ((pos - off) >> 3*(10 + sb.block_size)) % block_size_byte;
        if(pos < off)
            throw overflow_error("Overflow happened probably file is too big.");
        write_helper(in.ti,&pos,&size,buf,rel_block,3,off,&wb_size,&buf_pos);
    }
    write_superblock();
    write_inode(inode_index);
}

void file_system::write_helper(uint16_t address, uint32_t * pos, uint32_t * size,
                               const char * buf, size_t rel_block, size_t level,size_t off,size_t* wb_size,size_t * buf_pos)
{
    if(*size == 0)
        return;
    if(level == 0){
        load_by_block_no(address);
        data_block &temp = temp_blocks.back();
        // assign the buf to the block
        for (size_t i = *pos % block_size_byte; i < block_size_byte && *size > 0 ; ++i){
            temp.arr[i] = buf[(*buf_pos)++];
            ++(*pos);
            --(*size);
        }
        // put it to the buffer to write
        inode_blocks.emplace_back(temp);
        temp_blocks.pop_back();
        --(*wb_size);
        // if it is time write the buffer
        if(*wb_size == 0){
            write_inode_blocks_buffer();
            *wb_size = write_buffer_size;
        }
        return;
    }
    else{
        load_by_block_no(address);
        data_block temp = temp_blocks.back();
        temp_blocks.pop_back();
        size_t block_count = block_size_byte >> 1;
        uint16_t next_add = 0;
        size_t new_rel = 0;
        for (; rel_block < block_count; rel_block++)
        {
            next_add = temp.get_address(rel_block);
            if(!next_add){
                next_add = get_free_block();
                temp.set_address(rel_block, next_add);
                inode_blocks.emplace_back(temp);
                ++(*wb_size);
            }
            if(level > 1){
                new_rel = ((*pos - off) >> (level-1)*sb.block_size) % block_size_byte;
            }
            write_helper(next_add,pos,size,buf,new_rel,level-1,off,wb_size,buf_pos);
            if(*size == 0)
                return;
        }
    }
}

void file_system::write_inode_blocks_buffer()
{
    for (const auto& in : inode_blocks)
        write_block(in);
    inode_blocks.clear();
}

uint16_t file_system::get_free_inode() {
    if(sb.free_inode_count == 0){
        write_superblock();
        throw runtime_error("No more inodes left.");
    }
    size_t  tail_size = sb.free_inode_count*2 % (block_size_byte-2);
    load_by_block_no(sb.inode_tail,tail_size);
    data_block& fb = temp_blocks.back();
    size_t node_size = fb.get_fb_size();
    if(node_size == 0){
        temp_blocks.pop_back();
        size_t temp = sb.inode_tail;
        sb.inode_tail = fb.get_address(node_cap);
        // put this node to free blocks
        put_free_block(temp);
        // get the new tail
        load_by_block_no(sb.inode_tail);
        fb = temp_blocks.back();
    }
    // get new inode number
    uint16_t res = fb.pop_address();
    write_block(fb);
    temp_blocks.pop_back();
    sb.free_inode_count--;
    write_superblock();
    return res;
}

void file_system::put_free_inode(uint16_t index)
{
    // get the tail block
    load_by_block_no(sb.inode_tail);
    data_block& fb = temp_blocks.back();
    size_t node_size = fb.get_fb_size();
    fb.size = node_size*2;
    if(node_size == node_cap){
        temp_blocks.pop_back();
        uint16_t temp = sb.inode_tail;
        sb.inode_tail = get_free_block();
        load_by_block_no(sb.inode_tail);
        fb = temp_blocks.back();
        fb.set_address(node_cap,temp);
        fb.push_address(index);
        write_block(fb);
    }
    else{
        fb.push_address(index);
        write_block(fb);
    }
    temp_blocks.pop_back();
    sb.free_inode_count++;
    clear_inode(index);
    write_superblock();
    write_inode(index);
}

uint16_t file_system::get_free_block()
{
    if (sb.fb_head == 0 || sb.fb_count == 0) {
        write_superblock();
        throw length_error("No more free blocks left.");
    }
    uint16_t res = 0;
    load_by_block_no(sb.fb_tail);
    data_block& fb = temp_blocks.back();
    size_t free_blocks = fb.get_fb_size();
    fb.size = free_blocks*2;

    sb.fb_count--;
    if (free_blocks == 0) {
        sb.fb_tail = fb.get_address(node_cap);
        if(sb.fb_tail == 0)
            sb.fb_head = 0;
        res = fb.get_bno();
    }
    else {
        res = fb.pop_address();
        write_block(fb);
    }
    temp_blocks.pop_back();
    write_superblock();
    return res;
}

void file_system::put_free_block(uint16_t bno)
{
    load_by_block_no(sb.fb_tail);
    data_block& fb = temp_blocks.back();
    size_t fb_size = fb.get_fb_size();
    fb.size = fb_size*2;
    sb.fb_count++;
    // If the tail is full then the
    // block we are trying to put
    // will be our new tail node
    if(node_cap == fb_size){
        temp_blocks.pop_back();
        // change the tail to block bno
        uint16_t temp = sb.fb_tail;
        sb.fb_tail = bno;
        load_by_block_no(bno);
        fb = temp_blocks.back();
        fb.clear_block();
        fb.set_address(node_cap,temp);
        write_block(fb);
    }
    else{
        fb.push_address(bno);
        write_block(fb);
    }
    write_superblock();
    temp_blocks.pop_back();
}

void file_system::init_directory(data_block & db,uint16_t index, uint16_t parent){
    // modifying the block
    inodes[index].link_count = 1;
    inodes[index].type = dir_type;
    inodes[index].size = 2*data_block::dir_entry_size;
    vector<char> dir_ent = create_dir_entry(index,".");
    vector<char> dir_ent2 = create_dir_entry(parent,"..");
    for (size_t l = 0; l < data_block::dir_entry_size; ++l) {
        db.arr[l] = dir_ent[l];
        db.arr[l+data_block::dir_entry_size] = dir_ent2[l];
    }
}

// given path point to a folder
void file_system::list_folders(const std::string &path) {
    inode_blocks.clear();
    uint16_t path_inode = get_dir_inode(path);
    // checking if it is a folder or not
    if(inodes[path_inode].type != dir_type && inodes[path_inode].type != sym_dir)
        throw std::invalid_argument("Given path doesn't point to a listable object.");
    load_inode_blocks(inodes[path_inode]);
    vector<string> names;
    vector<size_t > inode_nos;
    for(auto &in: inode_blocks){
        size_t dir_count = in.get_dir_entry_count();
        for (size_t j = 2; j < dir_count; ++j) {
            names.push_back(in.get_entry_name(j));
            inode_nos.push_back(in.get_entry_inode_no(j));
        }
    }
    size_t i = 0;
    inode * temp = nullptr;
    for(const auto& line : names){
        temp = &inodes[inode_nos[i]];
        printf("%7u %u %s %2u %.2d:%.2d:%.2d %s\n",temp->size,temp->year,
                months[temp->month],temp->day,temp->hour,temp->min,temp->sec,line.data());
        i++;
    }
    fflush(stdout);
    inode_blocks.clear();
}

void file_system::dumpe2fs()  {
    // block count
    size_t block_count =  1024/sb.block_size;
    // inode count sb.inode_count
    // fb count sb.free_blocks
    // sb.block size
    map<size_t,set<string>> nm;
    // inode and the blocks it occupies
    map<size_t,vector<size_t>> blk_map;
    get_all_occupied_names_blocks(nm,blk_map);
    // getting all free blocks
    vector<size_t> fblocks;
    get_all_free_blocks(fblocks,sb.fb_tail);
    // getting all free inodes
    vector<size_t> finodes;
    size_t dir_count = 0;
    get_all_free_inodes(finodes,&dir_count);

    cout << GREEN << "Block Count: " << RESET << block_count << endl;
    cout << GREEN << "Inode Count: " << RESET << sb.inode_count << endl;
    cout << GREEN << "Free Block Count: " << RESET << sb.fb_count  << endl;
    cout << GREEN << "Free Inode Count: " << RESET << sb.free_inode_count  << endl;
    cout << GREEN "Number Of Files: " RESET<< blk_map.size() - dir_count << endl;
    cout << GREEN "Number Of Directories: " RESET<< dir_count << endl;
    cout << GREEN "Block Size (KB): " RESET<< sb.block_size << endl;
    size_t j = 0;
    cout << "Free Blocks: " << " (" <<fblocks.size() << "): ";
    for(auto i: fblocks){
        cout << i << ", ";
        j++;
        if(j == 30){
            cout << endl;
            j = 0;
        }
    }
    cout << endl;
    cout << "Free Inodes:" << " (" <<finodes.size() << "): ";
    j = 0;
    for(auto i: finodes){
        cout << i << ", ";
        j++;
        if(j == 30){
            cout << endl;
            j = 0;
        }
    }
    cout << endl;
    cout << GREEN "Occupied Inodes List: " RESET<< endl;
    for(auto& in : blk_map){
        cout << GREEN"-----------------------------" RESET << endl;
        cout << "Inode: " << in.first << endl;
        cout << "Occupied Blocks: ";
        j = 0;
        for(auto &ob: in.second){
            cout << ob << ", ";
            j++;
            if(j == 30){
                cout << endl;
                j = 0;
            }
        }
        cout << endl << "Occupied Names: ";
        for(auto &on: nm[in.first]){
            cout << on << ", ";
            j++;
            if(j == 30){
                cout << endl;
                j = 0;
            }
        }
        cout << endl;
        cout <<GREEN "-----------------------------" RESET<< endl;
    }

}

void file_system::get_all_occupied_names_blocks(map<size_t, set<string>> &name_map,
                                                map<size_t, vector<size_t>> &blk_map)  {
    // listing all occupied inodes
    vector<size_t> all_dirs;
    for (size_t i = 0; i < inodes.size(); ++i) {
        if (inodes[i].year != 0){
            if(inodes[i].type == file_system::dir_type)
                all_dirs.push_back(i);
            name_map[i]  = set<string>();
            blk_map[i] = vector<size_t>();
        }
    }
    for (auto& dir: all_dirs){
        //clears the inode blocks
        inode_blocks.clear();
        load_inode_blocks(inodes[dir]);
        //iterates through all blocks
        for (auto& in : inode_blocks) {
            size_t dir_count = in.get_dir_entry_count();
            for (size_t j = 2; j < dir_count; ++j) {
                name_map[in.get_entry_inode_no(j)].insert(in.get_entry_name(j));
            }
        }
    }
    name_map[0].insert("/");
    for(auto& pair: blk_map){
        load_occupied_inode_blocks(pair.first,pair.second);
    }

}

void file_system::get_all_free_blocks(std::vector<size_t> &res, size_t pos) {
    if(pos == 0)
        return;
    load_by_block_no(pos);
    data_block& temp = temp_blocks.back();
    size_t address_count = temp.get_fb_size();
    if(address_count == 0 && sb.fb_tail == pos){
        if(sb.fb_head == sb.fb_tail){
            res.push_back(pos);
            temp_blocks.pop_back();
            return;
        }
        size_t next = temp.get_address(node_cap);
        res.push_back(pos);
        temp_blocks.pop_back();
        get_all_free_blocks(res,next);
    }
    size_t next = temp.get_address(node_cap);
    for (size_t i = 0; i < address_count; ++i) {
        res.push_back(temp.get_address(i));
    }
    temp_blocks.pop_back();
    if(pos != sb.fb_head)
        get_all_free_blocks(res,next);
}

void file_system::get_all_free_inodes(std::vector<size_t> &res,size_t * dir_count) {
    for (size_t i = 0; i < inodes.size(); ++i) {
        if (inodes[i].type == empty_type)
            res.push_back(i);
        else if(inodes[i].type == dir_type || inodes[i].type == sym_dir)
            ++(*dir_count);
    }
}

void file_system::load_occupied_inode_blocks(size_t index, vector<size_t> &res) {
    inode in = inodes[index];
    for (uint16_t i : in.ba) {
        if(i != 0)
            res.push_back(i);
        else
            return;
    }
    load_occupied_inode_blocks_helper(index,res,in.si,1);
    load_occupied_inode_blocks_helper(index,res,in.di,2);
    load_occupied_inode_blocks_helper(index,res,in.ti,3);
}

void file_system::load_occupied_inode_blocks_helper(size_t index, vector<size_t> &res, size_t address, size_t level) {
    if(address == 0)
        return;
    if(level == 0){
        res.push_back(address);
    }
    else{
        load_by_block_no(address);
        res.push_back(address);
        data_block blk = temp_blocks.back();
        temp_blocks.pop_back();
        for (size_t i = 0; i < block_cap; ++i) {
            if(blk.get_address(i) != 0)
                load_occupied_inode_blocks_helper(index,res,blk.get_address(i),level-1);
        }
    }
}

void file_system::
test2(int argc,const char ** argv) {
    int bs,ic;
    args_reader::mfs(argc,argv,&bs,&ic);
    auto * fs = new file_system(bs,ic);
    fs->create_file(argv[3]);
    delete fs;

    auto * fs2 = new file_system("file.dat");
    cout << "mkdir" << endl;
    fs2->fsck();
    fs2->mkdir("/f1");
    fs2->fsck();
    delete fs2;
    for (size_t i = 0; i < 12; ++i) {
        auto * fs3 = new file_system("file.dat");
        cout << "mkdir" << "/f1/"+to_string(i) << endl;
        fs3->mkdir("/f1/"+to_string(i));
        fs2->fsck();
        delete fs3;
    }
    auto * fs3 = new file_system("file.dat");
    cout << "write /lol.p Makefile" << endl;
    fs2->fsck();
    fs3->copy_file("/lol.p","Makefile");
    fs2->fsck();
    fs3->dumpe2fs();
    fs3->read_file("/lol.p","out.txt");
    fs3->list_folders("/");
    delete fs3;
    auto * fs4 = new file_system("file.dat");
    fs4->fsck();
    fs4->rmdir("/f1/0");
    fs4->rmdir("/f1/1");
    fs4->rmdir("/f1/2");
    fs4->fsck();
    fs4->list_folders("/f1");
    fs4->dumpe2fs();
    fs4->copy_file("/f1/f2","os_midterm.cbp");
    fs4->dumpe2fs();
    fs4->hard_link("/f1/f2","/f1/l1");
    fs4->dumpe2fs();
    fs4->copy_file("/f1/l1","CMakeCache.txt");
    fs4->list_folders("/f1");
    fs4->soft_link("/f1/l1","/l2");
    fs4->copy_file("/l2","in2.txt");
    fs4->dumpe2fs();
    fs4->list_folders("/");
    fs4->del("/f1/l1");
    fs4->dumpe2fs();
    fs4->list_folders("/f1");
    fs4->del("/f1/f2");
    fs4->dumpe2fs();
    fs4->list_folders("/f1");
    fs4->list_folders("/");
    fs4->del("/l2");
    fs4->list_folders("/");
    delete fs4;

}

void file_system::copy_file(const std::string& path, const char * fname) {
    ifstream file(fname, ios::binary| ios::in | ios::ate);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    auto fsize = file.tellg();
    if(fsize > max_file_size)
        throw invalid_argument("Given file exceeds the size of the file system disk.");
    vector<char> buf(fsize);
    file.seekg(0);
    file.read(buf.data(),fsize);
    file.close();
    // write this buffer to the given path
    write_str_to_file(path, buf, false);
}

void file_system::write_str_to_file(const string &arg, std::vector<char> &buf, bool error_when_exist) {
    string path,name;
    size_t parent;
    // sets these values
    bool file_exists = new_file_args(arg, path, name, parent, error_when_exist);

    if(file_exists){
        // get the existing file
        size_t child = get_dir_inode(arg);
        size_t to_write = child;
        if(inodes[child].type == sym_file){
            vector<char>path_to_link(inodes[child].size);
            copy_system_file_to_buf(child,path_to_link.data(),inodes[child].size);
            to_write = get_dir_inode(path_to_link.data());
        }
        // set time for parent and child
        set_inode_time(parent);
        set_inode_time(to_write);
        // update child size
        empty_inode_blocks(to_write);
        inodes[to_write].size = 0;
        write(to_write,0,buf.size(),buf.data());
        inodes[to_write].size = buf.size();
        write_inode(parent);
        write_inode(to_write);
    }
    else{
        // now allocate inode and call write function
        uint16_t newi = get_free_inode();
        // set time for parent and child
        set_inode_time(parent);
        init_inode(newi);
        //init file attributes
        init_file(newi,buf.size());
        write(newi,0,buf.size(),buf.data());
        vector<char> dir_ent = create_dir_entry(newi,name);
        //write the data blocks
        uint32_t pos = get_inode_size(inodes[parent]);
        write(parent,pos,data_block::dir_entry_size,dir_ent.data());
        add_inode_size(parent,data_block::dir_entry_size);
        write_inode(newi);
        write_inode(parent);
    }
}

bool file_system::new_file_args(const std::string &arg, std::string &path, std::string &name, size_t &parent,
                                bool error_when_exists) {
    size_t last_slash = arg.find_last_of('/');
    if (last_slash == string::npos)
        throw invalid_argument("Please enter a full directory.");

    path = string(arg, 0, last_slash + 1);
    name = string(arg, last_slash + 1, arg.size() - last_slash);
    if (name.size() > 6)
        throw invalid_argument("File/Directory names should be at most 6 characters.");
    if ((name.find('/') != string::npos) || (name.find(' ') != string::npos))
        throw invalid_argument("File/Directory name is invalid.");
    if (path.find(' ') != string::npos)
        throw  invalid_argument("Given path is invalid");

    //first go to the location
    if(last_slash != 0 && last_slash == (path.size() - 1) )
        path.pop_back();
    parent =  get_dir_inode(path);
    inode i = inodes[parent];
    if (i.type != dir_type && i.type != sym_dir)
        throw invalid_argument("File or directory doesn't exist.");

    //clears the inode blocks
    inode_blocks.clear();
    load_inode_blocks(i);
    //check if file name is valid
    for (auto &in : inode_blocks) {
        size_t dir_count = in.get_dir_entry_count();
        for (size_t j = 0; j < dir_count; ++j) {
            if (in.get_entry_name(j) == name) {
                if(error_when_exists)
                    throw invalid_argument("File or directory name already exists.");
                else
                    return true;
            }
        }
    }
    inode_blocks.clear();

    return false;
}



void file_system::init_file(uint16_t index, size_t fsize) {
    // modifying the block
    inodes[index].link_count = 1;
    inodes[index].type = file_type;
    inodes[index].size = fsize;
}

void file_system::read_file(std::string path, const char *fname) {
    size_t file_inode = get_dir_inode(std::move(path));
    size_t file_size = inodes[file_inode].size;
    vector<char> buf(file_size);
    copy_system_file_to_buf(file_inode,buf.data(),file_size);
    ofstream file(fname,ios::out | ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.write(buf.data(),file_size);
    file.close();
}

void file_system::copy_system_file_to_buf(size_t iinode, char *buf, size_t size) {
    inode_blocks.clear();
    load_inode_blocks(inodes[iinode]);
    for(auto& in: inode_blocks){
        for (size_t i = 0; i < block_size_byte & size > 0; ++i) {
            *buf = in.arr[i];
            ++buf;
            --size;
        }
    }
    inode_blocks.clear();
}

void file_system::set_inode_time(size_t i) {
    std::time_t gt = std::time(nullptr);
    std::tm* t = std::localtime(&gt);

    inodes[i].year = 1900 + t->tm_year;
    inodes[i].month = t->tm_mon;
    inodes[i].day = t->tm_mday;

    inodes[i].hour = t->tm_hour;
    inodes[i].min = t->tm_min;
    inodes[i].sec = t->tm_sec;
}

void file_system::rmdir(const std::string &arg) {
    size_t to_rm,parent;
    string name,path;
    // check the parameter
    check_file_to_delete(arg,path,name,to_rm,parent);
    // the path should give an empty directory
    if(inodes[to_rm].type != dir_type && inodes[to_rm].type != sym_dir)
        throw invalid_argument("Given path doesn't show a directory.");
    if(inodes[to_rm].size != 2*data_block::dir_entry_size)
        throw invalid_argument("Given directory is not empty.");
    // must inform parent to remove the directory entry
    remove_dir_entry(parent,name);
    // clear the inode before freeing it
    clear_inode(to_rm);
    put_free_inode(to_rm);
    write_inode(to_rm);
}
// gives the inode to file and its parent
void file_system::check_file_to_delete(const std::string& arg, std::string &path, std::string &name, size_t &to_rm,
        size_t & parent) {
    size_t last_slash = arg.find_last_of('/');
    if (last_slash == string::npos)
        throw invalid_argument("Please enter a full directory.");

    path = string(arg, 0, last_slash + 1);
    name = string(arg, last_slash + 1, arg.size() - last_slash);
    if (name.size() > 6)
        throw invalid_argument("File/Directory names should be at most 6 characters.");
    if ((name.find('/') != string::npos) || (name.find(' ') != string::npos))
        throw invalid_argument("File/Directory name is invalid.");
    if (path.find(' ') != string::npos)
        throw  invalid_argument("Given path is invalid");
    if (name == "." || name == "..")
        throw  invalid_argument("Given file is invalid");
    //first go to the location
    to_rm = get_dir_inode(arg);
    parent =  get_dir_inode(path);
    if(to_rm == 0)
        throw invalid_argument("Root directory cannot be removed.");
}

void file_system::remove_dir_entry(size_t iindex, const std::string &name) {
    // get the last entry first to replace it with the one to be removed
    size_t fsize = inodes[iindex].size;
    vector<char> buf(fsize);
    copy_system_file_to_buf(iindex,buf.data(),fsize);
    size_t dir_count = fsize/(data_block::dir_entry_size);
    size_t done = 0, to_rm_pos = 1;
    for (int i = 1;i < dir_count && !done; ++i) {
        if(data_block::get_entry_name_from_arr(i,buf.data()) == name){
            done = 1;
            to_rm_pos = i;
        }
    }
    if(!done)
        throw logic_error("File that is supposed to be here is not here.(System Corrupted Create Another System)");
    buf.erase(buf.begin()+to_rm_pos*data_block::dir_entry_size,
            buf.begin()+to_rm_pos*data_block::dir_entry_size+data_block::dir_entry_size);
    // empties all allocated blocks
    empty_inode_blocks(iindex);
    write(iindex,0,buf.size(),buf.data());
    if(inodes[iindex].size < data_block::dir_entry_size)
        throw logic_error("Inode attributes are corrupted.");
    inodes[iindex].size  -= data_block::dir_entry_size;
    set_inode_time(iindex);
    write_inode(iindex);
}

void file_system::empty_inode_blocks(size_t iindex) {
    vector<size_t> blocks;
    load_occupied_inode_blocks(iindex,blocks);
    vector<size_t> block_set(blocks);
    for(auto bno: block_set)
        put_free_block(bno);
    for(auto& ba: inodes[iindex].ba)
        ba = 0;
    inodes[iindex].si = 0;
    inodes[iindex].di = 0;
    inodes[iindex].ti = 0;
}

void file_system::clear_inode(size_t index) {
    empty_inode_blocks(index);
    inodes[index].size = 0;
    inodes[index].type = empty_type;
    inodes[index].day = 0;
    inodes[index].month = 0;
    inodes[index].year = 0;
    inodes[index].link_count = 0;
}

void file_system::hard_link(const std::string& src, const std::string& dest) {
    string link_file_name,link_file_path;
    size_t link_parent = 0;
    new_file_args(dest, link_file_path, link_file_name, link_parent);
    size_t src_index = get_dir_inode(src);
    // adding a directory entry to link path contains | inode - dirname |
    auto dir_entry = create_dir_entry(src_index,link_file_name);
    write(link_parent,inodes[link_parent].size,data_block::dir_entry_size,dir_entry.data());
    // updating the parent inode
    add_inode_size(link_parent,data_block::dir_entry_size);
    set_inode_time(link_parent);
    // updating src inode
    // todo: might check the link count)
    inodes[src_index].link_count++;
    // writing the changes to the disk
    write_inode(src_index);
    write_inode(link_parent);
}

void file_system::soft_link(const std::string &src, const std::string &dest) {
    string link_name,link_path;
    size_t link_parent_inode=0;
    new_file_args(dest,link_path,link_name,link_parent_inode);
    // get inode of src just to check if it exists or not
    get_dir_inode(src);
    vector<char> buf(src.begin(),src.end());
    write_str_to_file(dest,buf,true);
    size_t link_index = get_dir_inode(dest);
    inodes[link_index].type = sym_file;
    write_inode(link_index);
}

void file_system::del(const string & arg) {
    string path,name;
    size_t to_rm,parent;
    check_file_to_delete(arg,path,name,to_rm,parent);
    // the path should give a file
    if(inodes[to_rm].type != file_type && inodes[to_rm].type != sym_file)
        throw invalid_argument("Given path doesn't show a file.");
    // delete the entry in the parent dir with this name
    remove_dir_entry(parent,name);
    // decrement link count
    inode& i = inodes[to_rm];
    if(i.link_count == 0){
        // if this exception is given operations will be undone
        // so the filesystem.dat will be broken
        throw logic_error("This file inode is corrupted.");
    }
    else if(i.link_count == 1){
        clear_inode(to_rm);
        put_free_inode(to_rm);
    }
    else{
        i.link_count--;
    }
    write_inode(to_rm);
}

void file_system::fsck() {
    // getting all free blocks
    vector<size_t> fblocks;
    get_all_free_blocks(fblocks,sb.fb_tail);
    map<size_t,set<string>> nm;
    // inode and the blocks it occupies
    map<size_t ,vector<size_t>> blk_map;
    get_all_occupied_names_blocks(nm,blk_map);
    nm.clear(); // not going to be used
    // allocating space for the maps
    // all blocks
    vector<size_t> oblocks;
    for(auto& blk: blk_map)
        oblocks.insert(oblocks.end(),blk.second.begin(),blk.second.end());
    // mapping for free and occupied blocks
    map<size_t,size_t> free_map;
    for(auto& blk: fblocks)
        free_map[blk]++;
    map<size_t,size_t> full_map;
    for(auto& blk: oblocks)
        full_map[blk]++;
    for(auto& blk: fblocks)
        full_map[blk] = full_map[blk];
    for(auto& blk: oblocks)
        free_map[blk] = free_map[blk];
    // to map occupied inodes first get free inodes
    map<size_t,size_t> full_inodes,free_inodes;
    for (size_t i = 0; i < sb.inode_count; ++i) {
        if(inodes[i].type != empty_type)
            full_inodes[i]++;
        full_inodes[i] = full_inodes[i];
    }
    vector<size_t> free_inodes_list;
    get_all_free_inodes_rec(free_inodes_list,sb.inode_tail);
    for(auto& i: free_inodes_list)
        free_inodes[i]++;
    for (size_t i = 0; i < sb.inode_count; ++i)
        free_inodes[i] = free_inodes[i];
    size_t newline = 0;
    cout << "Left digit shows number of free blocks or free inodes association occurs in free list for the given data block,"
            << endl << "right digit shows occupied blocks or inodes associated." << endl
            << "Data Blocks "<<"("<< free_map.size() << ")"<< endl;
    for(auto& blk: free_map){
        newline++;
        printf("Data Block %4lu:" GREEN "|%lu%lu|    " RESET,blk.first,blk.second,full_map[blk.first]);
        if(newline == 5){
            cout << endl;
            newline = 0;
        }
    }
    cout << endl;
    newline = 0;
    cout << "Inodes " << "(" << free_inodes.size() << ")" << endl;
    for(auto& i: free_inodes){
        newline++;
        printf("Inode %4lu:" GREEN "|%lu%lu|    " RESET,i.first,i.second,full_inodes[i.first]);
        if(newline == 7){
            cout << endl;
            newline = 0;
        }
    }
    cout << endl;

}

void file_system::get_all_free_inodes_rec(std::vector<size_t> &res, size_t pos) {
    if(pos == 0)
        return;
    load_by_block_no(pos);
    data_block& temp = temp_blocks.back();
    size_t address_count = temp.get_fb_size();
    if(address_count == 0){
       throw logic_error("Free inode list is corrupted.");
    }
    size_t next = temp.get_address(node_cap);
    for (size_t i = 0; i < address_count; ++i) {
        res.push_back(temp.get_address(i));
    }
    temp_blocks.pop_back();
    if(pos != sb.inode_tail)
        get_all_free_inodes_rec(res,next);

}

data_block::~data_block() {
    delete[] arr;
}

data_block::data_block(const data_block& d) {
    size = d.size;
    cap = d.cap;
    bno = d.bno;
    arr = new char[d.cap];
    for (size_t i = 0; i < d.cap; ++i) {
        arr[i] = d.arr[i];
    }
}

data_block::data_block(const char* iarr, size_t size, size_t cap, size_t bno) {
    this->size = size;
    this->cap = cap;
    this->bno = bno;
    arr = new char[cap];
    for (size_t i = 0; i < cap; ++i) {
        arr[i] = iarr[i];
    }
}

data_block& data_block::operator=(const data_block& d) {
    if(this == &d)
        return *this;
    this->size = d.size;
    this->cap = d.cap;
    delete[] arr;
    arr = new char[cap];
    for (size_t i = 0; i < cap; ++i) {
        arr[i] = d.arr[i];
    }
    this->bno = d.bno;
    return *this;
}

size_t data_block::get_entry_inode_no(size_t index) const{
    if (index * 8 + 8 > size)
        throw range_error("Directory entry index is invalid.");
    size_t res = 0;
    res = (uint8_t)arr[index * 8 + 1];
    res += ((uint8_t)arr[index * 8] << 8);

    return res;
}

size_t data_block::get_address(size_t index) const{
    if (index * 2 + 2 > cap)
        throw range_error("Address entry index is invalid.");
    size_t res = 0;
    res = (unsigned char)arr[index * 2 + 1];
    res += ((uint16_t)arr[index * 2] << 8);

    return res;
}

size_t data_block::get_bno() {
    return bno;
}

size_t data_block::get_fb_size() const {
    size_t i = 0;
    for (i = 0; i < cap/2-1; i++)
    {
        if (!arr[2 * i] && !arr[2 * i + 1]) {
            break;
        }
    }
    return i;
}

std::string data_block::get_entry_name(size_t index) const {
    if (index * 8 + 8 > size)
        throw invalid_argument("Directory entry index is invalid.");

    return get_entry_name_from_arr(index, arr);
}

size_t data_block::get_dir_entry_count() const{
    if (size % 8 != 0)
        throw logic_error("Data block directory entries are corrupted.");
    return size / 8;
}

void data_block::push_address(size_t address) {
    // TODO use constant
    size = size + 2;
    if (size > cap) {
        throw std::invalid_argument("Capacity of block node is full.");
    }
    arr[size - 1] = (uint8_t) (address % one_byte);
    arr[size - 2] = (uint8_t)(address >> 8);
}

size_t data_block::pop_address() {
    if (size < 2) {
        throw std::invalid_argument("Block node is empty.");
    }
    size = size - 2;
    size_t res = 0;
    res = (uint8_t)arr[size+1];
    res += (((uint16_t)arr[size]) << 8);
    arr[size] = 0;
    arr[size+1] = 0;
    return res;
}

void data_block::set_address(size_t index, uint16_t address)
{
    if (index+1 > cap) {
        throw std::range_error("Given index is out of range.");
    }
    arr[2*index + 1] = uint8_t(address % one_byte);
    arr[2*index] = uint8_t(address >> 8);
}

void data_block::clear_block()
{
    for (size_t i = 0; i < cap; i++){
        arr[i] = 0;
    }
    size = 0;
}

data_block::data_block(size_t blk_size) {
    arr = new char[blk_size];
    for (size_t i = 0; i < blk_size;i++) {
        arr[i] = 0;
    }
    cap  = blk_size;
    size = 0;
    bno = 0;
}

string data_block::get_entry_name_from_arr(size_t index, char *arr) {
    string res(arr+8 * index + 2,dir_entry_size);
    return string(res.data(),strlen(res.data()));
}
