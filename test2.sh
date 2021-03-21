make clean
make
dd  bs=1018K seek=1 of=linuxFile.data count=0
./makeFileSystem 1 10 mySystem.dat
./fileSystemOper mySystem.dat write "/f1" linuxFile.data
./fileSystemOper mySystem.dat dumpe2fs
./fileSystemOper mySystem.dat fsck
./fileSystemOper mySystem.dat del "/f1"
./fileSystemOper mySystem.dat dumpe2fs
./fileSystemOper mySystem.dat fsck