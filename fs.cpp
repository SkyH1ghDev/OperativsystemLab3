#include <iostream>
#include <vector>
#include <sstream>
#include "fs.h"
#include <random>
#include <cstring>
#include <algorithm>

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    srand(time(NULL));
    }

FS::~FS()
{

}

// Formats the blocks of the disk
void FS::FormatBlocks()
{
    this->fat[ROOT_BLOCK] = FAT_EOF;
    this->fat[FAT_BLOCK] = FAT_EOF;

    for (int i = 2; i < BLOCK_SIZE / 2; ++i)
    {
        this->fat[i] = FAT_FREE;
    }
}

// Initializes the root directory
void FS::InitializeRoot()
{
    std::vector<dir_entry> root;
    root.reserve(64);

    this->directoryTree.value = root;
    this->directoryTree.name = "";
    this->directoryTreeWorkingDirectory.value = root;
}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    std::cout << "FS::format()\n";

    FormatBlocks();
    InitializeRoot(); // Initializes Root-directory

    return 0;
}

// Splits filepath string into substrings of subdirectories and filename 
// and returns vector of them (Does not include the root ("") as a substring)
//
// -----
//
// std::string const &filepath - The filepath 
int FS::SplitFilepath(std::string const &filepath, std::vector<std::string> &subStringVector) const
{
    std::stringstream ss(filepath);

    while (ss.good())
    {
        std::string subString;
        getline(ss, subString, '/');
        if (subString != "")
        {
            subStringVector.push_back(subString);
        }
    }

    if (subStringVector.empty())
    {
        std::cout << "Cannot parse invalid filepath" << std::endl;
        return -1;
    }

    return 0;
}

std::string FS::ConcatenateFilepath(std::vector<std::string> const &filenames) const
{
    std::string filepath = "/";

    for (std::string filename : filenames)
    {
        filepath += filename + "/";
    }

    return filepath;
}

// Returns the last substring in subdirectory and filename vector
//
// -----
//
// std::string const &filepath - The filepath 
int FS::GetFilenameFromFilepath(std::string const &filepath, std::string &filename) const
{
    std::vector<std::string> stringVector; 
    if (SplitFilepath(filepath, stringVector) == -1)
    {
        return -1;       
    }

    filename = stringVector.back();
    return 0;
}

// Checks whether the create-command is valid
//
// -----
//
// std::string const &filepath - The filepath
int FS::CheckValidCreate(std::string const &filepath) const
{

    std::string filename;
    if (GetFilenameFromFilepath(filepath, filename) == -1)
    {
        return -1;
    }

    if (filename.length() > 56){
        std::cout << "Filename is too long" << std::endl;
        return -1;
    }

    for (auto file : this->directoryTreeWorkingDirectory.value)
    {
        if (file.file_name == filename)
        {
            std::cout << "File already exists in this directory" << std::endl;
            return -1;
        }
    }

    return 0;
}

// Saves the shell input into a string
//
// -----
//
// int &size - The size (length) of the input string including null-terminators
// std::string &inputString - The string that was input into the shell
void FS::SaveInputToString(int &size, std::string &inputString) const
{
    std::string segment;

    while (true)
    {
        getline(std::cin, segment);

        if (segment.empty())
        {
            break;
        }

        size += segment.size() + 1; // + 1 includes the nullterminator
        inputString += inputString.empty() ? segment : "\n" + segment;
    }
}

// Divides input string into 4096-byte sized blocks and returns a vector with these blocks.
//
// -----
//
// std::string const &inputString - The string that was input in the shell
std::vector<std::string> FS::DivideStringIntoBlocks(std::string const &inputString) const
{
    int maxIndex = static_cast<int>(inputString.size() / BLOCK_SIZE);

    std::vector<std::string> stringVec;

    for (int i = 0; i < maxIndex; ++i)
    {
        stringVec.push_back(inputString.substr(i * BLOCK_SIZE, BLOCK_SIZE));
    }
    stringVec.push_back(inputString.substr(maxIndex * BLOCK_SIZE));   
    
    return stringVec;
}

// Returns whether there is a sufficient amount of available free memory blocks
//
// -----
//
// int const &amount - The amount of blocks to be allocated
// std::vector<int> &indexVector - Vector of the indices of the allocated blocks
int FS::FindFreeMemoryBlocks(int const &amount, std::vector<int> &indexVector)
{
    std::vector<int> tempIndexVector;

    for (int i = 0; i < amount; ++i)
    {
        int randomIndex = rand() % 2045 + 2;

        for (int j = randomIndex; ; ++j)
        {
            if (this->fat[j] == FAT_FREE)
            {
                tempIndexVector.push_back(j);
                this->fat[j] = FAT_TEMP_RESERVED;
                break;
            }

            if (j == randomIndex - 1)
            {
                std::cout << "Not enough free memory in filesystem" << std::endl;

                for (auto index : tempIndexVector)
                {
                    this->fat[index] = FAT_FREE;
                }

                return -1;
            }

            if (j == BLOCK_SIZE / 2 - 1)
            {
                j = 2;
            }
        }
    }

    indexVector = tempIndexVector;

    return 0;
}

// Constructs and returns a dir_entry
//
// -----
//
// std::string const &filename - The filename of the file
// int const &size - Size of the file
// int const &firstBlock - The first block of the file
// int const &type - The type of the file (file or directory)
// int const &accessRights - The bitflags of access rights to the file 
dir_entry FS::MakeDirEntry(std::string const &filename, int const &size, int const &firstBlock, int const &type, int const &accessRights)
{
    dir_entry DirEntry; 

    strcpy(DirEntry.file_name, filename.c_str());
    DirEntry.size = static_cast<u_int32_t>(size);
    DirEntry.first_blk = static_cast<u_int16_t>(firstBlock);
    DirEntry.type = static_cast<u_int8_t>(type);
    DirEntry.access_rights = static_cast<u_int8_t>(accessRights);

    return DirEntry;
}

// Writes contents of the blockVector into the disk at the indices
// in the indexVector and adds dirEntry into the correct directory
//
// -----
//
// std::vector<dir_entry> &directory - The directory to be written to
// dir_entry const &dirEntry - The directory entry to be written
// std::vector<int> const &indexVector - The vector of indices of the blocks that the file will occupying
// std::vector<std::string> const &blockVector - The vector with the file data to be written (in blocks)
int FS::WriteToMemory(std::vector<dir_entry> &directory, dir_entry const &dirEntry, std::vector<int> const &indexVector, std::vector<std::string> const &blockVector)
{
    if (directory.size() == 64)
    {
        std::cout << "Directory is already at max capacity (64)" << std::endl;
        return -1;
    }

    for (int i = 0; i < indexVector.size() - 1; ++i)
    {
        this->fat[indexVector.at(i)] = indexVector.at(i + 1);
        disk.write(indexVector.at(i), (uint8_t*)blockVector.at(i).c_str()); // Neither static_cast nor reinterpret_cast work. C-style conversion works though.
    }

    this->fat[indexVector.back()] = FAT_EOF;
    disk.write(indexVector.back(), (uint8_t*)blockVector.back().c_str());

    directory.push_back(dirEntry);

    for (int i = 0; i < directory.size(); ++i)
    {
        disk.write(0, (uint8_t*)(directory.data() + i)); // ONLY WRITES TO ROOT DIRECTORY
    }

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    if (CheckValidCreate(filepath) == -1)
    {
        return -1;
    }

    int size = 0; std::string inputString = "";
    SaveInputToString(size, inputString);

    std::vector<std::string> blockVector = DivideStringIntoBlocks(inputString);

    
    std::vector<int> indexVector;
    if (FindFreeMemoryBlocks(blockVector.size(), indexVector) == -1)
    {
        return -1;
    }
    
    std::string filename;
    if (GetFilenameFromFilepath(filepath, filename) == -1)
    {
        return -1;
    }
    
    dir_entry dirEntry = MakeDirEntry(filename, size, indexVector.at(0), TYPE_FILE, READ | WRITE | EXECUTE);

    WriteToMemory(this->directoryTreeWorkingDirectory.value, dirEntry, indexVector, blockVector);

    return 0;
}

void FS::ReadBlocksFromMemory(dir_entry &file)
{

}

// Assumes that filepath only consists of directories
int FS::TraverseDirectoryTree(std::string const &filepath, TreeNode<std::vector<dir_entry>> const &dirTreeRoot, std::vector<dir_entry> &directory)
{
    std::vector<std::string> filenameVector;
    if (SplitFilepath(filepath, filenameVector) == -1)
    {
        return -1;
    }

    if (filenameVector.empty())
    {
        directory = dirTreeRoot.value;
    }
    
    TreeNode<std::vector<dir_entry>> currDirectory = dirTreeRoot;


    for (int i = 0; i < filenameVector.size(); ++i)
    {
        bool directoryExists = false;

        for (TreeNode<std::vector<dir_entry>> node : currDirectory.children)
        {
            if (node.name == filenameVector.at(i))
            {
                currDirectory = node;
                directoryExists = true;
                break;
            }
        }

        if (!directoryExists)
        {
            std::cout << "Filepath is invalid" << std::endl;
            return -1;
        }
    }

    directory = currDirectory.value;

    return 0;
}

int FS::GetFileDirEntry(std::string const &filepath, TreeNode<std::vector<dir_entry>> const &dirTreeRoot, dir_entry &file)
{
    std::vector<std::string> filenameVector;
    if (SplitFilepath(filepath, filenameVector) == -1)
    {
        return -1;
    }

    std::vector<std::string> directoryFilenames(filenameVector.begin(), filenameVector.end() - 1);
    std::vector<dir_entry> workingDirectory;

    if (TraverseDirectoryTree(ConcatenateFilepath(directoryFilenames), dirTreeRoot, workingDirectory) == -1)
    {
        return -1;
    }

    if (workingDirectory.empty())
    {
        std::cout << "The file does not exist" << std::endl;
        return -1; 
    }

    for (dir_entry tempFile : workingDirectory)
    {
        if (tempFile.file_name == filenameVector.back())
        {
            file = tempFile;
            return 0;
        }

        std::cout << "The file does not exist" << std::endl;
        return -1;
    }

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    dir_entry file;
    
    if (GetFileDirEntry(filepath, this->directoryTreeWorkingDirectory, file) == -1)
    {
        return -1;
    }

    std::cout << file.file_name << std::endl;

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";

    if (directoryTreeWorkingDirectory.value.empty())
    {
        std::cout << "This directory is empty..." << std::endl; 
    }
    else 
    {
        std::cout << "Filename \t\t Size (Bytes) \n";
        for (dir_entry file : directoryTreeWorkingDirectory.value)
        {
            std::cout << file.file_name << " \t\t\t " << file.size << std::endl;
        }
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << "FS::pwd()\n";
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
