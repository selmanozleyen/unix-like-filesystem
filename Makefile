CC = g++
CFLAGS = -Wextra -Wall -pedantic --std=c++11 -g
FILE_SYSTEM = file_system.cpp file_system.h
ARG_READER = args_reader.cpp args_reader.h

all: make_file_system operations

make_file_system: make_file_system.cpp  $(FILE_SYSTEM) $(ARG_READER)
	$(CC) $(CFLAGS) -o makeFileSystem make_file_system.cpp $(FILE_SYSTEM) $(ARG_READER)

operations: file_system_oper.cpp  $(FILE_SYSTEM) $(ARG_READER)
	$(CC) $(CFLAGS) -o fileSystemOper file_system_oper.cpp $(FILE_SYSTEM) $(ARG_READER)

clean:
	rm makeFileSystem  fileSystemOper
	
