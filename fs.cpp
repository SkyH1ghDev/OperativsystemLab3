#include <iostream>
#include <vector>
#include <sstream>
#include "fs.h"
#include <random>

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

// Initializes a directory
void FS::InitializeDirectory()
{
   dir_entry directory[64];

    for (int i = 0; i < 64; ++i)
    {
        dir_entry initalizedEntry;
        initalizedEntry.type = -1;

        directory[i] = initalizedEntry;
    }

    directoryVector.push_back(directory);
}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    std::cout << "FS::format()\n";

    FormatBlocks();
    InitializeDirectory(); // Initializes Root-directory

    return 0;
}

// Splits filepath string into substrings of subdirectories and filename and returns vector of them
std::vector<std::string> FS::SplitFilepath(std::string &filepath) const
{
    std::vector<std::string> subStringVector;

    std::stringstream ss(filepath);

    while (ss.good())
    {
        std::string subString;
        getline(ss, subString, '/');
        subStringVector.push_back(subString);
    }

    return subStringVector;
}

// Returns the last substring in subdirectory and filename vector
std::string FS::GetFilenameFromFilepath(std::string &filepath) const
{
    std::vector<std::string> stringVector = SplitFilepath(filepath);

    return stringVector.back();
}

// Checks whether the create-command is valid
int FS::CheckValidCreate(std::string &filepath) const
{
    if (GetFilenameFromFilepath(filepath).length() > 56){
        std::cout << "Filename is too long" << std::endl;
        return -1;
    }

    return 0;
}

// Saves the shell input into a string
void FS::SaveInputToString(int &length, std::string &inputString)
{
    std::string segment;

    while (true)
    {
        getline(std::cin, segment);

        if (segment.empty())
        {
            break;
        }

        length += segment.size() + 1; // + 1 includes the nullterminator
        inputString += inputString.empty() ? segment : "\n" + segment;
    }
}

// Divides input string into 4096-byte sized blocks and returns a vector with these blocks.
std::vector<std::string> FS::DivideStringIntoBlocks(std::string &inputString) const
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

dir_entry FS::MakeDirEntry(std::string &filename, int &size, int &firstBlock, int &type, int &accessRights)
{
    dir_entry DirEntry;

    DirEntry.file_name = filename.c_str();
    DirEntry.size = std::static_cast<u_int32_t>(size);
    DirEntry.first_blk = ()firstBlock;


    return DirEntry;
}

void FS::WriteToMemory(dir_entry* directory, dir_entry &dirEntry, std::vector<int> &indexVector, std::vector<std::string> &blockVector)
{
    for (int i = 0; i < indexVector.size() - 1; ++i)
    {
        this->fat[indexVector.at(i)] = indexVector.at(i + 1);
        disk.write(indexVector.at(i), (uint8_t*)blockVector.at(i).c_str());
    }
    this->fat[indexVector.back()] = FAT_EOF;
    disk.write(indexVector.back(), (u_int8_t*)blockVector.back().c_str());



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

    int length = 0; std::string inputString = "";
    SaveInputToString(length, inputString);

    std::vector<std::string> blockVector = DivideStringIntoBlocks(inputString);

    std::vector<int> indexVector;
    if (FindFreeMemoryBlocks(blockVector.size(), indexVector) == -1)
    {
        return -1;
    }



    WriteToMemory(directoryVector.at(0), )

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";
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
