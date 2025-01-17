#include <cstdint>
#include "disk.h"
#include <string>
#include <map>
#include <memory>
#include <array>
#include <stack>
#include <vector>

#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0

#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1
#define FAT_RESERVED -2

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0b100
#define WRITE 0b10
#define EXECUTE 0b1

struct dir_entry
{
    char file_name[56]; // name of the file / sub-directory
    std::uint32_t size; // size of the file in bytes
    std::uint16_t first_blk; // index in the FAT for the first block of the file
    std::uint8_t type; // directory (1), file (0) or initialized (-1)
    std::uint8_t access_rights; // read (0b100), write (0b01), execute (0b1)
};


class FS
{
private:
    Disk disk;

    // size of a FAT entry is 2 bytes
    std::array<std::int16_t, 2048> fat{};

    dir_entry rootDirEntry = {"/", 0, 0, TYPE_DIR, READ | WRITE | EXECUTE};
    dir_entry currentDirectoryEntry;

    std::stack<std::string> currentPath{};

private:
    std::map<int, std::string> rightsMap =
    {
        {0, "-"},
        {READ, "r-"},
        {WRITE, "-w-"},
        {EXECUTE, "--x"},
        {READ | WRITE, "rw-"},
        {READ | EXECUTE, "r-x"},
        {WRITE | EXECUTE, "-wx"},
        {READ | WRITE | EXECUTE, "rwx"}
    };

    int ReserveMemory(const std::int16_t& numBlocks, std::vector<std::int16_t>& memoryBlocks);

    void FreeMemory(const dir_entry& dirEntry);

    void WriteFatToDisk();

    void ReadFatFromDisk();

    void WriteDirectoryToDisk(const std::array<dir_entry, 64>& directory, const dir_entry& directoryEntry);

    std::array<dir_entry, 64> ReadDirectoryFromDisk(const dir_entry& directoryEntry);

    void WriteFileToDisk(const std::vector<int16_t>& memoryBlocks, const std::vector<std::string>& inputStringBlocks);

    std::string ReadFileFromDisk(const dir_entry& dirEntry);

    std::vector<std::string> TokenizePath(const std::string& filepath) const;

    dir_entry GetStartingDirectoryEntry(const std::string& filepath) const;

    int GetDirEntry(const std::vector<std::string>& tokens, const std::array<dir_entry, 64>& startingDirectory,
                    dir_entry& directoryEntry, const std::int8_t& accessRights);

    int CheckValidCreate(const std::vector<std::string>& tokens, const std::array<dir_entry, 64>& startingDirectory);

    void SaveInputToString(std::size_t& size, std::string& inputString);

    std::vector<std::string> DivideStringIntoBlocks(const std::string& inputString);

    std::vector<std::int16_t> GetUsedMemoryBlockIndices(const dir_entry& dirEntry);

public:
    FS();

    ~FS();

    // formats the disk, i.e., creates an empty file system
    int format();

    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(const std::string& filepath);

    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(const std::string& filepath);

    // ls lists the content in the current directory (files and sub-directories)
    int ls();

    // cp <sourcepath> <destpath> makes an exact copy of the file
    // <sourcepath> to a new file <destpath>
    int cp(std::string sourcepath, std::string destpath);

    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);

    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);

    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);

    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);

    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the current directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
