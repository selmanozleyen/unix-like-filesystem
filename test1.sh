dd if=/dev/urandom of=linuxFile.data bs=1K count=100
make clean
make
./makeFileSystem 4 400 mySystem.dat
./fileSystemOper mySystem.dat mkdir "/usr"
./fileSystemOper mySystem.dat mkdir "/usr/ysa"
./fileSystemOper mySystem.dat mkdir "/bin/ysa"
./fileSystemOper mySystem.dat write "/usr/ysa/file1" linuxFile.data
./fileSystemOper mySystem.dat write "/usr/file2" linuxFile.data
./fileSystemOper mySystem.dat write "/file3" linuxFile.data
./fileSystemOper mySystem.dat list "/"
./fileSystemOper mySystem.dat del "/usr/ysa/file1"
./fileSystemOper mySystem.dat dumpe2fs
./fileSystemOper mySystem.dat read "/usr/file2" linuxFile2.data
md5sum linuxFile2.data linuxFile.data
./fileSystemOper mySystem.dat ln "/usr/file2" "/usr/lf2"
./fileSystemOper mySystem.dat list "/usr"
./fileSystemOper mySystem.dat write "/usr/lf2" linuxFile.data
./fileSystemOper mySystem.dat lnsym "/usr/file2" "/usr/sf2"
./fileSystemOper mySystem.dat list "/usr"
./fileSystemOper mySystem.dat del "/usr/file2"
./fileSystemOper mySystem.dat list "/usr"
./fileSystemOper mySystem.dat write "/usr/sf2" linuxFile.data
./fileSystemOper mySystem.dat dumpe2fs