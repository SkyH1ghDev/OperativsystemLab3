#include <iostream>
#include <vector>
#include <sstream>
#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{

}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    std::cout << "FS::format()\n";
    this->fat[ROOT_BLOCK] = FAT_EOF;
    this->fat[FAT_BLOCK] = FAT_EOF;

    for (int i = 2; i < BLOCK_SIZE / 2; ++i)
    {
        this->fat[i] = FAT_FREE;
    }

    return 0;
}

// Splits filepath string into substrings of subdirectories and filename and returns vector of them
std::vector<std::string> FS::SplitFilepath(std::string &filepath)
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
std::string FS::GetFilenameFromFilepath(std::string &filepath)
{
    std::vector<std::string> stringVector = SplitFilepath(filepath);

    return stringVector.back();
}

// Checks whether the create-command is valid
int FS::CheckValidCreate(std::string &filepath)
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

// Divides input string into 4096-byte sized blocks.
void FS::DivideStringIntoBlocks(std::string &inputString)
{
    int maxIndex = static_cast<int>(inputString.size() / BLOCK_SIZE);

    std::vector<std::string> stringVec;

    for (int i = 0; i < maxIndex; ++i)
    {
        stringVec.push_back(inputString.substr(i * BLOCK_SIZE, BLOCK_SIZE));
    }
    stringVec.push_back(inputString.substr(maxIndex * BLOCK_SIZE));   int maxIndex = static_cast<int>(inputString.size() / BLOCK_SIZE);

    std::vector<std::string> stringVec;

    for (int i = 0; i < maxIndex; ++i)
    {
        stringVec.push_back(inputString.substr(i * BLOCK_SIZE, BLOCK_SIZE));
    }
    stringVec.push_back(inputString.substr(maxIndex * BLOCK_SIZE));
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





    std::cout << "String: " << inputString << std::endl;
    
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
