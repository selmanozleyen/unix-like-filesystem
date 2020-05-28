//
// Created by selman.ozleyen2017 on 17.05.2020.
//

#ifndef OS_MIDTERM_FILE_SYSTEM_H
#define OS_MIDTERM_FILE_SYSTEM_H

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <map>
#include <set>

/* WARNING: THIS WILL WORK ON MACHINES WHERE ONE CHAR IS A BYTE */

#define RESET   "\033[0m"
#define GREEN   "\033[32m"

class data_block {
public:
    data_block(const char* iarr, size_t size, size_t cap, size_t bno);
    ~data_block();
    explicit data_block(size_t blk_size);
    data_block(const data_block& d);
    data_block& operator=(const data_block& d);

    void push_address(size_t address);
    size_t pop_address();

    void set_address(size_t index, uint16_t address);
    void clear_block();

    size_t get_entry_inode_no(size_t index) const;
    std::string get_entry_name(size_t index) const;
    static std::string get_entry_name_from_arr(size_t index, char *arr);

    size_t get_dir_entry_count() const;
    size_t get_address(size_t index) const;

    size_t get_bno();

    //special for free block nodes.
    size_t get_fb_size() const;

private:
    const static size_t one_byte = 256;
    const static size_t dir_entry_size = 8;
    size_t bno = 0;
    size_t cap = 0;
    size_t size = 0;
    char* arr = nullptr;

    friend class file_system;
};


struct inode {
    //date
    uint16_t year;
    uint8_t month;
    uint8_t day;
    //time
    uint8_t hour;
    uint8_t min;
    uint8_t sec;

    uint32_t size;
    // if it is a dir/file or soft link
    uint8_t type;
    // link count
    uint16_t link_count;
    //direct block addresses
    uint16_t ba[5];
    // single double and triple indirect addresses
    uint16_t si;
    uint16_t di;
    uint16_t ti;
};

struct superblock {
    uint16_t block_size;
    uint16_t root_dir_address;
    uint16_t inode_pos;
    uint16_t inode_count;
    uint16_t free_inode_count;
    uint16_t fb_count;
    uint16_t fb_head;
    uint16_t fb_tail;
};

class file_system {
public:
    // for creating object
    file_system(size_t block_size, size_t inode_count);
    // for getting the instance from the file
    explicit file_system(const char* filename);
    // for creating a file of the object
    void create_file(const char* filename);

    // changes inode blocks
    void mkdir(const std::string& arg);
    // rmdir
    void rmdir(const std::string& arg);
    // changes inode blocks and writes them to the given inode before flushing
    void write(uint16_t inode_index, uint32_t pos, uint32_t size,const char* buf);
    // lists all files in the given directory
    void list_folders(const std::string & path);
    // displays data about the file system
    void dumpe2fs();
    // copies file from linux
    void copy_file(const std::string& path, const char * filename);
    // reads file to linux file
    void read_file(std::string path, const char * filename);
    // adds a directory entry with the same inode and increments link count
    void hard_link(const std::string& src,const std::string& dest);
    // just shows where the file is
    void soft_link(const std::string& src,const std::string& dest);
    // deletes the given file
    void del(const std::string& arg);
    // file system check
    void fsck();


    static void test2(int argc,const char ** argv);
private:
    uint16_t get_dir_inode(std::string path);
    uint16_t get_dir_inode_helper(std::string& path,const inode& i);
    void load_inode_blocks(inode i);
    void load_by_block_no(size_t bno, size_t size);
    void write_str_to_file(const std::string &arg, std::vector<char> &buf, bool error_when_exist);
    void write_block(const data_block& b) const;
    void write_superblock();
    void write_inode(uint16_t ino);
    void write_helper(uint16_t address, uint32_t * pos, uint32_t * size,const char * buf,
                      size_t rel_block,size_t level,size_t off,size_t* wb_size,size_t * buf_pos);
    void write_inode_blocks_buffer();

    void get_all_occupied_names_blocks(std::map<size_t,std::set<std::string>>& name_map,
                                       std::map<size_t,std::vector<size_t>> &blk_map);
    void rec_inode_lookup(std::map<size_t,size_t>& full_inodes);
    void rec_inode_lookup(std::map<size_t, size_t> &full_inodes, size_t pos, std::vector<bool>& visited);
    //helper for read_file method
    void copy_system_file_to_buf(size_t iinode,char * buf,size_t size);



    uint16_t get_free_inode();
    void put_free_inode(uint16_t index);
    uint16_t get_free_block();
    void put_free_block(uint16_t bno);
    // checks if the directory exists and if the file doesn't exists
    // returns true if the file exists
    bool new_file_args(const std::string &arg, std::string &path, std::string &name, size_t &parent,
                       bool error_when_exists = true);
    // checks the dir to delete and if it is valid returns the index to it self and its parents
    void check_file_to_delete(const std::string& arg,std::string& path,std::string& name, size_t& to_rm, size_t& parent);

    void init_inode(size_t index);
    void set_inode_time(size_t index);
    // clears all the blocks and attributes of given inode
    void clear_inode(size_t index);
    //empties all blocks that an inode occupies
    void empty_inode_blocks(size_t iindex);

    void init_directory(data_block & db,uint16_t index, uint16_t parent);
    void init_file(uint16_t index, size_t fsize);


    std::vector<char> create_dir_entry(uint16_t index,const std::string& name) const;
    void remove_dir_entry(size_t iindex,const std::string& name);
    void add_inode_size(size_t index, uint32_t size);
    static uint32_t get_inode_size(const inode& i);
    void load_inode_blocks_helper(size_t bno, size_t* size, size_t* rem_blocks, size_t level);

    void get_all_free_blocks(std::vector<size_t>& res,size_t pos);
    void get_all_free_inodes(std::vector<size_t>& res,size_t * dir_count);
    void get_all_free_inodes_rec(std::vector<size_t>& res,size_t pos);
    void load_occupied_inode_blocks(size_t index, std::vector<size_t> &res);
    void load_occupied_inode_blocks_helper(size_t index, std::vector<size_t> &res, size_t address, size_t level);

    const char* filename = nullptr;
    superblock sb{};
    size_t block_size_byte;
    size_t node_cap;
    size_t block_cap;
    static const size_t inode_size = 32;
    static const size_t direct_count = 5;
    static const size_t write_buffer_size = 64;
    static const int max_file_size = 1 << 20;
    static const char months[][4];
    static const size_t empty_type = 0;
    static const size_t dir_type = 1;
    static const size_t file_type = 2;
    static const size_t sym_dir = 3;
    static const size_t sym_file = 4;
    static const size_t dir_name_size = 6;
    // System RAM simulation
    std::vector<inode> inodes;
    std::vector<data_block> inode_blocks;
    std::vector<data_block> temp_blocks;

};


#endif //OS_MIDTERM_FILE_SYSTEM_H
