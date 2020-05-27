//
// Created by selman.ozleyen2017 on 16.05.2020.
//

#ifndef OS_MIDTERM_ARGS_READER_H
#define OS_MIDTERM_ARGS_READER_H


class args_reader {
public:
    static void mfs(int argc, const char **argv, int *bs, int *ic);
    static void file_oper(int argc,const char ** argv);
private:
    args_reader() = default;
    static const int argc_no = 4;


};


#endif //OS_MIDTERM_ARGS_READER_H
