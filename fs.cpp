#include "fs.h"
#include <sstream>
#include <cstring>
#include <bitset>

FS::FS() : currentDirectoryEntry(rootDirEntry)
{
    std::cout << "Starting Filesystem... \n";

	ReadFatFromDisk();

    std::srand(time(nullptr));
}

FS::~FS()
{
    std::cout << "Exiting Filesystem... \n";
}

int FS::format()
{
    // Format FAT
    this->fat[ROOT_BLOCK] = FAT_EOF;
    this->fat[FAT_BLOCK] = FAT_EOF;

    memset(this->fat.data() + 2 * sizeof(std::uint16_t), 0, 2046 * sizeof(std::uint16_t));

    // Format Root-Directory
    std::array<dir_entry, 64> root{};
    root[0] =
    {
        .file_name = ".",
        .size = 0,
        .first_blk = 0,
        .type = TYPE_DIR,
        .access_rights = READ | WRITE | EXECUTE
    };

    root[1] =
    {
        .file_name = "..",
        .size = 0,
        .first_blk = 0,
        .type = TYPE_DIR,
        .access_rights = READ | WRITE | EXECUTE
    };

    // Initialize Current Directory
    this->currentDirectoryEntry = this->rootDirEntry;

    this->disk.write(ROOT_BLOCK, reinterpret_cast<std::uint8_t *>(root.data()));
    this->disk.write(FAT_BLOCK, reinterpret_cast<std::uint8_t *>(this->fat.data()));

    return 0;
}

int FS::create(const std::string& filepath)
{
    std::vector<std::string> tokens = TokenizePath(filepath);
    dir_entry startingDirEntry = GetStartingDirectoryEntry(filepath);
    std::array<dir_entry, 64> startingDirectory = ReadDirectoryFromDisk(startingDirEntry);

    // Check so that creation is valid
    if (CheckValidCreate(tokens, startingDirectory) == -1)
    {
        return -1;
    }

    // Get the directory where the file is getting created
    std::vector<std::string> creationTokens = {tokens.begin(), tokens.end() - 1};

    dir_entry creationDirEntry;
    GetDirEntry(creationTokens, startingDirectory, creationDirEntry, READ | WRITE);
    std::array<dir_entry, 64> creationDirectory = ReadDirectoryFromDisk(creationDirEntry);

    // Search the directory for available space
    std::int8_t fileIndex = -1;
    for (int i = 0; i < creationDirectory.max_size(); ++i)
    {
        if (creationDirectory.at(i).file_name[0] == '\0')
        {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1)
    {
        std::cout << "Directory Is At Max Capacity (64) \n";
        return -1;
    }

    std::size_t size = 0;
    std::string inputString = "";
    SaveInputToString(size, inputString);

    std::vector<std::string> inputStringBlocks = DivideStringIntoBlocks(inputString);

    std::vector<std::int16_t> reservedMemoryIndexVector;
    if (ReserveMemory(inputStringBlocks.size(), reservedMemoryIndexVector) == -1)
    {
        std::cout << "Insufficient available space for this file\n";
        return -1;
    }

    dir_entry newFileDirEntry;

    tokens.back().copy(newFileDirEntry.file_name, 56);
    newFileDirEntry.size = size;
    newFileDirEntry.first_blk = reservedMemoryIndexVector.at(0);
    newFileDirEntry.type = TYPE_FILE;
    newFileDirEntry.access_rights = READ | WRITE | EXECUTE;

    creationDirectory[fileIndex] = newFileDirEntry;
    WriteDirectoryToDisk(creationDirectory, creationDirEntry);

    WriteFileToDisk(reservedMemoryIndexVector, inputStringBlocks);

    return 0;
}

int FS::cat(const std::string& filepath)
{
    std::vector<std::string> tokens = TokenizePath(filepath);
    dir_entry startingDirectoryEntry = GetStartingDirectoryEntry(filepath);
    std::array<dir_entry, 64> startingDirectory = ReadDirectoryFromDisk(startingDirectoryEntry);

    dir_entry directoryEntry;
    if (GetDirEntry(tokens, startingDirectory, directoryEntry, READ) == -1)
    {
        "Could Not Find File\n";
        return -1;
    }

    if (directoryEntry.type == TYPE_DIR)
    {
        std::cout << "Unable To Read Directory As File\n";
        return -1;
    }

    std::string readString = ReadFileFromDisk(directoryEntry);

    std::cout << readString << "\n";

    return 0;
}

int FS::ls()
{
    std::array<dir_entry, 64> currentDirectory = ReadDirectoryFromDisk(this->currentDirectoryEntry);

    std::cout.width(8);
    std::cout << "Filename";
    std::cout.width(20);
    std::cout << "Type";
    std::cout.width(20);
    std::cout << "Access Rights";
    std::cout.width(20);
    std::cout << "Size (Bytes)";
    std::cout.width(20);
    std::cout << "First Block" << std::endl;

    for (const dir_entry& dirEntry: currentDirectory)
    {
        if (dirEntry.file_name[0] != '\0'
            && dirEntry.file_name[0] != '.')
        {
            std::cout.width(8);
            std::cout << dirEntry.file_name;
            std::cout.width(20);
            std::cout << (dirEntry.type == TYPE_DIR ? "Dir" : "File");
            std::cout.width(20);
            std::cout << rightsMap[dirEntry.access_rights];
            std::cout.width(20);
            std::cout << (dirEntry.type == TYPE_DIR ? "-" : std::to_string(dirEntry.size));
            std::cout.width(20);
            std::cout << dirEntry.first_blk << std::endl;
        }
    }

    return 0;
}

int FS::cp(std::string sourcepath, std::string destpath)
{
    std::vector<std::string> sourceTokens = TokenizePath(sourcepath);
    dir_entry sourceStartingDirEntry = GetStartingDirectoryEntry(sourcepath);
    std::array<dir_entry, 64> sourceStartingDirectory = ReadDirectoryFromDisk(sourceStartingDirEntry);

    dir_entry sourceDirEntry;
    if (GetDirEntry(sourceTokens, sourceStartingDirectory, sourceDirEntry, READ) == -1)
    {
        std::cout << "Source File Could Not Be Found\n";
        return -1;
    }

    if (sourceDirEntry.type == TYPE_DIR)
    {
        std::cout << "Cannot Copy Directory\n";
        return -1;
    }

    std::vector<std::string> destTokens = TokenizePath(destpath);
    dir_entry destStartingDirEntry = GetStartingDirectoryEntry(destpath);
    std::array<dir_entry, 64> destStartingDirectory = ReadDirectoryFromDisk(destStartingDirEntry);

    dir_entry destDirEntry;
    if (GetDirEntry(destTokens, destStartingDirectory, destDirEntry, READ | WRITE) == 0)
    {
        if (destDirEntry.type == TYPE_FILE)
        {
            std::cout << "File Already Exists\n";
            return -1;
        }

        std::array<dir_entry, 64> destDirectory = ReadDirectoryFromDisk(destDirEntry);

        std::int8_t fileIndex = -1;
        for (int i = 0; i < destDirectory.max_size(); ++i)
        {
            if (destDirectory.at(i).file_name[0] == '\0')
            {
                fileIndex = i;
                break;
            }
        }

        if (fileIndex == -1)
        {
            std::cout << "Directory Is At Max Capacity (64) \n";
            return -1;
        }

        std::string inputString = ReadFileFromDisk(sourceDirEntry);

        std::vector<std::string> inputStringBlocks = DivideStringIntoBlocks(inputString);

        std::vector<std::int16_t> reservedMemoryIndexVector;
        if (ReserveMemory(inputStringBlocks.size(), reservedMemoryIndexVector) == -1)
        {
            std::cout << "Insufficient available space for this file\n";
            return -1;
        }

        dir_entry sourceDirEntryCopy = sourceDirEntry;

        std::memset(sourceDirEntryCopy.file_name, '\0', 56);
        sourceTokens.back().copy(sourceDirEntryCopy.file_name, 56);

        sourceDirEntryCopy.first_blk = reservedMemoryIndexVector.at(0);

        destDirectory[fileIndex] = sourceDirEntryCopy;
        WriteDirectoryToDisk(destDirectory, destDirEntry);

        WriteFileToDisk(reservedMemoryIndexVector, inputStringBlocks);
    } else
    {
        if (CheckValidCreate(destTokens, destStartingDirectory) == -1)
        {
            return -1;
        }

        std::vector<std::string> creationTokens = {destTokens.begin(), destTokens.end() - 1};

        dir_entry creationDirEntry;
        GetDirEntry(creationTokens, destStartingDirectory, creationDirEntry, READ | WRITE);
        std::array<dir_entry, 64> creationDirectory = ReadDirectoryFromDisk(creationDirEntry);

        // Search the directory for available space
        std::int8_t fileIndex = -1;
        for (int i = 0; i < creationDirectory.max_size(); ++i)
        {
            if (creationDirectory.at(i).file_name[0] == '\0')
            {
                fileIndex = i;
                break;
            }
        }

        if (fileIndex == -1)
        {
            std::cout << "Directory Is At Max Capacity (64) \n";
            return -1;
        }

        std::string inputString = ReadFileFromDisk(sourceDirEntry);

        std::vector<std::string> inputStringBlocks = DivideStringIntoBlocks(inputString);

        std::vector<std::int16_t> reservedMemoryIndexVector;
        if (ReserveMemory(inputStringBlocks.size(), reservedMemoryIndexVector) == -1)
        {
            std::cout << "Insufficient available space for this file\n";
            return -1;
        }

        dir_entry sourceDirEntryCopy = sourceDirEntry;

        std::memset(sourceDirEntryCopy.file_name, '\0', 56);
        destTokens.back().copy(sourceDirEntryCopy.file_name, 56);

        sourceDirEntryCopy.first_blk = reservedMemoryIndexVector.at(0);

        creationDirectory[fileIndex] = sourceDirEntryCopy;
        WriteDirectoryToDisk(creationDirectory, creationDirEntry);

        WriteFileToDisk(reservedMemoryIndexVector, inputStringBlocks);
    }

    return 0;
}

int FS::mv(std::string sourcepath, std::string destpath)
{
    std::vector<std::string> sourceTokens = TokenizePath(sourcepath);
    dir_entry sourceStartingDirEntry = GetStartingDirectoryEntry(sourcepath);
    std::array<dir_entry, 64> sourceStartingDirectory = ReadDirectoryFromDisk(sourceStartingDirEntry);

    dir_entry sourceDirEntry;
    if (GetDirEntry(sourceTokens, sourceStartingDirectory, sourceDirEntry, 0) == -1)
    {
        std::cout << "Source File Could Not Be Found\n";
        return -1;
    }

    if (sourceDirEntry.type == TYPE_DIR)
    {
        std::cout << "Cannot Copy Directory\n";
        return -1;
    }

    std::vector<std::string> destTokens = TokenizePath(destpath);
    dir_entry destStartingDirEntry = GetStartingDirectoryEntry(destpath);
    std::array<dir_entry, 64> destStartingDirectory = ReadDirectoryFromDisk(destStartingDirEntry);

    dir_entry destDirEntry;
    if (GetDirEntry(destTokens, destStartingDirectory, destDirEntry, READ | WRITE) == 0)
    {
        if (destDirEntry.type == TYPE_FILE)
        {
            std::cout << "File Already Exists\n";
            return -1;
        }

        std::vector<std::string> sourceParentTokens = {sourceTokens.begin(), sourceTokens.end() - 1};

        dir_entry sourceParentDirEntry;
        GetDirEntry(sourceParentTokens, sourceStartingDirectory, sourceParentDirEntry, READ | WRITE);

        std::array<dir_entry, 64> sourceParentDirectory = ReadDirectoryFromDisk(sourceParentDirEntry);

        for (int i = 2; i < sourceParentDirectory.max_size(); ++i)
        {
            if (std::string(sourceDirEntry.file_name) == std::string(sourceParentDirectory.at(i).file_name))
            {
                sourceParentDirectory.at(i) = {};
                break;
            }

            if (i == sourceParentDirectory.max_size() - 1)
            {
                std::cout << "Directory Is At Max Capacity (64)\n";
                return -1;
            }
        }

        std::array<dir_entry, 64> destDirectory = ReadDirectoryFromDisk(destDirEntry);

        for (int i = 2; i < destDirectory.max_size(); ++i)
        {
            if (std::string(destDirectory.at(i).file_name) == std::string(sourceDirEntry.file_name))
            {
                std::cout << "File Already Exists\n";
                return -1;
            }
        }

        WriteDirectoryToDisk(sourceParentDirectory, sourceParentDirEntry);

        destDirectory = ReadDirectoryFromDisk(destDirEntry);

        for (int i = 2; i < destDirectory.max_size(); ++i)
        {
            if (std::string(destDirectory.at(i).file_name) == "\0")
            {
                destDirectory.at(i) = sourceDirEntry;
                break;
            }

            if (i == destDirectory.max_size() - 1)
            {
                std::cout << "Directory Is At Max Capacity (64)\n";
                return -1;
            }
        }

        WriteDirectoryToDisk(destDirectory, destDirEntry);
    } else
    {
        std::vector<std::string> sourceParentTokens = {sourceTokens.begin(), sourceTokens.end() - 1};

        dir_entry sourceParentDirEntry;
        GetDirEntry(sourceParentTokens, sourceStartingDirectory, sourceParentDirEntry, READ | WRITE);

        std::array<dir_entry, 64> sourceParentDirectory = ReadDirectoryFromDisk(sourceParentDirEntry);

        for (int i = 2; i < sourceParentDirectory.max_size(); ++i)
        {
            if (std::string(sourceDirEntry.file_name) == std::string(sourceParentDirectory.at(i).file_name))
            {
                sourceParentDirectory.at(i) = {};
                break;
            }

            if (i == sourceParentDirectory.max_size() - 1)
            {
                std::cout << "Directory Is At Max Capacity (64)\n";
                return -1;
            }
        }

        std::vector<std::string> destParentTokens = {destTokens.begin(), destTokens.end() - 1};

        dir_entry destParentDirEntry;
        GetDirEntry(destParentTokens, destStartingDirectory, destParentDirEntry, READ | WRITE);

        std::array<dir_entry, 64> destParentDirectory = ReadDirectoryFromDisk(destParentDirEntry);

        for (int i = 2; i < destParentDirectory.max_size(); ++i)
        {
            if (std::string(destParentDirectory.at(i).file_name) == destTokens.back())
            {
                std::cout << "File Already Exists\n";
                return -1;
            }
        }

        WriteDirectoryToDisk(sourceParentDirectory, sourceParentDirEntry);

        destParentDirectory = ReadDirectoryFromDisk(destParentDirEntry);

        for (int i = 2; i < destParentDirectory.max_size(); ++i)
        {
            if (std::string(destParentDirectory.at(i).file_name) == "\0")
            {
                dir_entry sourceDirEntryCopy = sourceDirEntry;

                std::memset(sourceDirEntryCopy.file_name, '\0', 56);
                destTokens.back().copy(sourceDirEntryCopy.file_name, 56);
                destParentDirectory.at(i) = sourceDirEntryCopy;
                break;
            }

            if (i == destParentDirectory.max_size() - 1)
            {
                std::cout << "Directory Is At Max Capacity (64)\n";
                return -1;
            }
        }

        WriteDirectoryToDisk(destParentDirectory, destParentDirEntry);
    }

    return 0;
}

int FS::rm(std::string filepath)
{
    std::vector<std::string> tokens = TokenizePath(filepath);
    dir_entry startingDirectoryEntry = GetStartingDirectoryEntry(filepath);
    std::array<dir_entry, 64> startingDirectory = ReadDirectoryFromDisk(startingDirectoryEntry);

    dir_entry dirEntry;
    if (GetDirEntry(tokens, startingDirectory, dirEntry, 0) == -1)
    {
        std::cout << "Could Not Find The File\n";
        return -1;
    }

    if (dirEntry.type == TYPE_DIR)
    {
        std::array<dir_entry, 64> directory = ReadDirectoryFromDisk(dirEntry);

        // Check if there are any files except ".." and "."
        for (int i = 2; i < directory.max_size(); ++i)
        {
            if (std::string(directory.at(i).file_name) != "\0")
            {
                std::cout << "Directory Is Not Empty. '-r' Flag Not Supported\n";
                return -1;
            }
        }

        if (dirEntry.first_blk == this->currentDirectoryEntry.first_blk)
        {
            std::cout << "Could Not Remove Current Working Directory\n";
            return -1;
        }
    }

    std::int16_t currentIndex = dirEntry.first_blk;
    while (this->fat[currentIndex] != FAT_EOF)
    {
        std::int16_t nextIndex = this->fat[currentIndex];
        this->fat[currentIndex] = FAT_FREE;
        currentIndex = nextIndex;
    }

    this->fat[currentIndex] = FAT_FREE;

    std::vector<std::string> parentTokens = {tokens.begin(), tokens.end() - 1};

    dir_entry parentDirEntry;
    GetDirEntry(parentTokens, startingDirectory, parentDirEntry, READ | WRITE | EXECUTE);

    std::array<dir_entry, 64> parentDirectory = ReadDirectoryFromDisk(parentDirEntry);

    for (int i = 0; i < parentDirectory.max_size(); ++i)
    {
        if (std::string(parentDirectory.at(i).file_name) == std::string(dirEntry.file_name))
        {
            parentDirectory.at(i) = {};
            break;
        }
    }

    WriteDirectoryToDisk(parentDirectory, parentDirEntry);

    return 0;
}

int FS::append(std::string filepath1, std::string filepath2)
{
    std::vector<std::string> sourceTokens = TokenizePath(filepath1);
    dir_entry sourceStartingDirEntry = GetStartingDirectoryEntry(filepath1);
    std::array<dir_entry, 64> sourceStartingDirectory = ReadDirectoryFromDisk(sourceStartingDirEntry);

    dir_entry sourceDirEntry;
    if (GetDirEntry(sourceTokens, sourceStartingDirectory, sourceDirEntry, READ) == -1)
    {
        std::cout << "Source File Could Not Be Found\n";
        return -1;
    }

    if (sourceDirEntry.type == TYPE_DIR)
    {
        std::cout << "Cannot Read Source File Of Type Dir\n";
        return -1;
    }

    std::string sourceString = ReadFileFromDisk(sourceDirEntry);

    std::vector<std::string> destTokens = TokenizePath(filepath2);
    dir_entry destStartingDirEntry = GetStartingDirectoryEntry(filepath2);
    std::array<dir_entry, 64> destStartingDirectory = ReadDirectoryFromDisk(destStartingDirEntry);

    dir_entry destDirEntry;
    if (GetDirEntry(destTokens, destStartingDirectory, destDirEntry, READ | WRITE) == -1)
    {
        std::cout << "Destination File Could Not Be Found\n";
        return -1;
    }

    if (destDirEntry.type == TYPE_DIR)
    {
        std::cout << "Cannot Write To Destination File Of Type Dir\n";
        return -1;
    }

    std::vector<std::string> destParentTokens = {destTokens.begin(), destTokens.end() - 1};

    dir_entry destParentDirEntry;
    GetDirEntry(destParentTokens, destStartingDirectory, destParentDirEntry, READ | WRITE);

    std::array<dir_entry, 64> destParentDirectory = ReadDirectoryFromDisk(destParentDirEntry);

    std::string destString = ReadFileFromDisk(destDirEntry);
    std::string totalString = destString + "\n" + sourceString;

    for (int i = 2; i < destParentDirectory.max_size(); ++i)
    {
        if (std::string(destParentDirectory.at(i).file_name) == std::string(destDirEntry.file_name))
        {
            destParentDirectory.at(i).size = totalString.size() + 1;
            break;
        }
    }

    std::vector<std::int16_t> indexVector = GetUsedMemoryBlockIndices(destDirEntry);
    std::vector<std::string> blockVector = DivideStringIntoBlocks(totalString);

    if (ReserveMemory(blockVector.size(), indexVector) == -1)
    {
        std::cout << "Insufficient Free Memory Available\n";
        return -1;
    }

    WriteDirectoryToDisk(destParentDirectory, destParentDirEntry);
    WriteFileToDisk(indexVector, blockVector);

    return 0;
}

int FS::mkdir(std::string dirpath)
{
    // Check if Create is valid
    std::vector<std::string> tokens = TokenizePath(dirpath);
    dir_entry startingDirectoryEntry = GetStartingDirectoryEntry(dirpath);
    std::array<dir_entry, 64> startingDirectory = ReadDirectoryFromDisk(startingDirectoryEntry);

    if (CheckValidCreate(tokens, startingDirectory) == -1)
    {
        return -1;
    }

    // Get Directory Where New Directory Is To Be Created
    std::vector<std::string> creationTokens = {tokens.begin(), tokens.end() - 1};

    dir_entry creationDirEntry;
    GetDirEntry(creationTokens, startingDirectory, creationDirEntry, READ | WRITE);
    std::array<dir_entry, 64> creationDirectory = ReadDirectoryFromDisk(creationDirEntry);

    // Check If Directory Is Full
    std::int8_t fileIndex = -1;
    for (int i = 0; i < creationDirectory.max_size(); ++i)
    {
        if (creationDirectory.at(i).file_name[0] == '\0')
        {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1)
    {
        std::cout << "Directory Is At Max Capacity (64) \n";
        return -1;
    }

    std::vector<std::int16_t> memoryBlock;
    ReserveMemory(1, memoryBlock);

    this->fat[memoryBlock.at(0)] = FAT_EOF;

    dir_entry newDirEntry;
    tokens.back().copy(newDirEntry.file_name, 56);
    newDirEntry.size = 0;
    newDirEntry.first_blk = memoryBlock.at(0);
    newDirEntry.type = TYPE_DIR;
    newDirEntry.access_rights = READ | WRITE | EXECUTE;

    creationDirectory[fileIndex] = newDirEntry;
    WriteDirectoryToDisk(creationDirectory, creationDirEntry);

    std::array<dir_entry, 64> newDirectory{};

    std::string thisDirName = ".";
    std::memset(newDirEntry.file_name, '\0', 56);
    thisDirName.copy(newDirEntry.file_name, 56);
    newDirectory[0] = newDirEntry;

    std::string parentDirName = "..";
    std::memset(creationDirEntry.file_name, '\0', 56);
    parentDirName.copy(creationDirEntry.file_name, 56);
    newDirectory[1] = creationDirEntry;

    WriteDirectoryToDisk(newDirectory, newDirEntry);

    return 0;
}

int FS::cd(std::string dirpath)
{
    std::vector<std::string> tokens = TokenizePath(dirpath);
    dir_entry startingDirectoryEntry = GetStartingDirectoryEntry(dirpath);
    std::array<dir_entry, 64> startingDirectory = ReadDirectoryFromDisk(startingDirectoryEntry);

    dir_entry dirEntry;
    if (GetDirEntry(tokens, startingDirectory, dirEntry, READ | EXECUTE) == -1)
    {
        std::cout << "Could Not Find File\n";
        return -1;
    }

    if (dirEntry.type == TYPE_FILE)
    {
        std::cout << "Could Not CD Into A File\n";
        return -1;
    }

    this->currentDirectoryEntry = dirEntry;

    if (std::string(startingDirectoryEntry.file_name) == "/")
    {
        std::stack<std::string> newPathStack;

        for (std::string pathSegment: tokens)
        {
            if (pathSegment == "..")
            {
                newPathStack.pop();
                continue;
            }

            if (pathSegment == ".")
            {
                continue;
            }

            newPathStack.push(pathSegment);
        }

        this->currentPath = newPathStack;
    } else
    {
        for (std::string pathSegment: tokens)
        {
            if (pathSegment == "..")
            {
                this->currentPath.pop();
                continue;
            }

            if (pathSegment == ".")
            {
                continue;
            }

            this->currentPath.push(pathSegment);
        }
    }

    return 0;
}

int FS::pwd()
{
    if (this->currentPath.empty())
    {
        std::cout << "/\n";
        return 0;
    }

    std::stack<std::string> currentPathCopy = this->currentPath;

    std::string path;
    while (!currentPathCopy.empty())
    {
        path = "/" + currentPathCopy.top() + path;
        currentPathCopy.pop();
    }

    std::cout << path << "\n";

    return 0;
}

int FS::chmod(std::string accessrights, std::string filepath)
{
    std::vector<std::string> tokens = TokenizePath(filepath);
    dir_entry startingDirectoryEntry = GetStartingDirectoryEntry(filepath);
    std::array<dir_entry, 64> startingDirectory = ReadDirectoryFromDisk(startingDirectoryEntry);

    dir_entry dirEntry;
    if (GetDirEntry(tokens, startingDirectory, dirEntry, 0) == -1)
    {
        std::cout << "Could Not Find File\n";
        return -1;
    }

    dirEntry.access_rights = std::stoi(accessrights);

    std::vector<std::string> parentTokens = {tokens.begin(), tokens.end() - 1};
    dir_entry parentDirectoryEntry;
    GetDirEntry(parentTokens, startingDirectory, parentDirectoryEntry, READ | WRITE);

    std::array<dir_entry, 64> parentDirectory = ReadDirectoryFromDisk(parentDirectoryEntry);

    for (int i = 0; i < parentDirectory.max_size(); ++i)
    {
        if (std::string(parentDirectory.at(i).file_name) == std::string(dirEntry.file_name))
        {
            parentDirectory.at(i) = dirEntry;
            break;
        }
    }

    WriteDirectoryToDisk(parentDirectory, parentDirectoryEntry);

    return 0;
}


/*
 * Helpers
 */


int FS::ReserveMemory(const std::int16_t& numBlocks, std::vector<std::int16_t>& memoryBlocks)
{
    std::vector<int> tempMemoryBlocks;

    int alreadyExisting = memoryBlocks.size();

    // Makes it possible to append with already existing blocks instead of
    // constantly freeing blocks and looking for new ones
    int additionalRequired = numBlocks - alreadyExisting;

    for (int i = 0; i < additionalRequired; ++i)
    {
        int randomIndex = rand() % 2045 + 2;

        for (int j = randomIndex;; ++j)
        {
            if (this->fat[j] == FAT_FREE)
            {
                tempMemoryBlocks.push_back(j);
                this->fat[j] = FAT_RESERVED;
                break;
            }

            if (j == randomIndex - 1)
            {
                std::cout << "Not enough free memory in filesystem" << std::endl;

                for (auto index: tempMemoryBlocks)
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

    for (int blockIndex: tempMemoryBlocks)
    {
        memoryBlocks.push_back(blockIndex);
    }

    return 0;
}

void FS::WriteFatToDisk()
{
    disk.write(FAT_BLOCK, reinterpret_cast<std::uint8_t *>(this->fat.data()));
}

void FS::ReadFatFromDisk()
{
    disk.read(FAT_BLOCK, reinterpret_cast<std::uint8_t *>(this->fat.data()));
}

void FS::WriteDirectoryToDisk(const std::array<dir_entry, 64>& directory, const dir_entry& directoryEntry)
{
    std::uint8_t data[sizeof(dir_entry) * directory.size()] = {};

    std::memcpy(data, directory.data(), sizeof(dir_entry) * directory.size());
    disk.write(directoryEntry.first_blk, data);
    WriteFatToDisk();
}

std::array<dir_entry, 64> FS::ReadDirectoryFromDisk(const dir_entry& directoryEntry)
{
    std::uint8_t data[4096];
    disk.read(directoryEntry.first_blk, data);

    dir_entry* dirData = reinterpret_cast<dir_entry *>(data);
    std::array<dir_entry, 64> directory;

    for (int i = 0; i < directory.size(); ++i)
    {
        directory[i] = dirData[i];
    }

    return directory;
}

void FS::WriteFileToDisk(const std::vector<int16_t>& memoryBlocks, const std::vector<std::string>& inputStringBlocks)
{
    for (int i = 0; i < memoryBlocks.size() - 1; ++i)
    {
        this->fat[memoryBlocks.at(i)] = memoryBlocks.at(i + 1);
        disk.write(memoryBlocks.at(i), (uint8_t *) inputStringBlocks.at(i).c_str());
        // Neither static_cast nor reinterpret_cast work. C-style conversion works though.
    }

    this->fat[memoryBlocks.back()] = FAT_EOF;

    std::string const& lastBlock = inputStringBlocks.back();

    disk.write(memoryBlocks.back(), (uint8_t *) lastBlock.c_str());
    WriteFatToDisk();
}

std::string FS::ReadFileFromDisk(const dir_entry& dirEntry)
{
    int16_t currentBlock = dirEntry.first_blk;
    std::string readString;

    while (currentBlock != FAT_EOF)
    {
        char readBlock[BLOCK_SIZE] = {};

        disk.read(currentBlock, (uint8_t *) readBlock);

        readString += std::string((char *) readBlock);

        currentBlock = this->fat[currentBlock];
    }

    return readString;
}

std::vector<std::string> FS::TokenizePath(const std::string& filepath) const
{
    std::vector<std::string> subStringVector;
    std::stringstream ss(filepath);

    while (ss.good())
    {
        std::string subString;
        getline(ss, subString, '/');
        if (!subString.empty())
        {
            subStringVector.push_back(subString);
        }
    }

    return subStringVector;
}

dir_entry FS::GetStartingDirectoryEntry(const std::string& filepath) const
{
    return filepath[0] == '/' ? this->rootDirEntry : this->currentDirectoryEntry;
}

int FS::GetDirEntry(const std::vector<std::string>& tokens, const std::array<dir_entry, 64>& startingDirectory,
                    dir_entry& directoryEntry, const std::int8_t& accessRights)
{
    std::array<dir_entry, 64> currentDirectory = startingDirectory;

    if (tokens.empty())
    {
        if (currentDirectory.at(0).access_rights & accessRights != accessRights)
        {
            std::cout << "You Do Not Have The Necessary Privileges (" << this->rightsMap[accessRights] <<
                    ") For This Operation\n";
            return -1;
        }

        directoryEntry = currentDirectory.at(0);
        return 0;
    }

    for (int i = 0; i < tokens.size(); ++i)
    {
        bool dirEntryFound = false;

        for (const dir_entry& dirEntry: currentDirectory)
        {
            if (dirEntry.file_name == tokens[i])
            {
                dirEntryFound = true;

                std::int8_t permission = dirEntry.access_rights & accessRights;
                if (permission != accessRights)
                {
                    std::cout << "You Do Not Have The Necessary Privileges (" << this->rightsMap[accessRights] <<
                            ") For This Operation\n";
                    return -1;
                }

                if (i == tokens.size() - 1)
                {
                    directoryEntry = dirEntry;
                    return 0;
                }

                if (dirEntry.type != TYPE_DIR)
                {
                    std::cout << "Invalid Filepath \n";
                    return -1;
                }

                currentDirectory = ReadDirectoryFromDisk(dirEntry);
                break;
            }
        }

        if (dirEntryFound == false)
        {
            return -1;
        }
    }
}

int FS::CheckValidCreate(const std::vector<std::string>& tokens, const std::array<dir_entry, 64>& startingDirectory)
{
    if (tokens.empty())
    {
        std::cout << "Invalid Filepath \n";
        return -1;
    }

    std::string filename = tokens.back();

    if (filename.size() >= 55)
    {
        std::cout << "Filename Is Longer Than 55 Characters \n";
        return -1;
    }

    dir_entry dirEntry;
    if (GetDirEntry(tokens, startingDirectory, dirEntry, READ | WRITE) == 0)
    {
        std::cout << "File Already Exists \n";
        return -1;
    }

    return 0;
}

void FS::SaveInputToString(std::size_t& size, std::string& inputString)
{
    std::string segment;

    while (true)
    {
        getline(std::cin, segment);

        if (segment.empty())
        {
            break;
        }

        size += segment.size() + 1; // + 1 includes the new-line character
        inputString += inputString.empty() ? segment : "\n" + segment;
    }
}

std::vector<std::string> FS::DivideStringIntoBlocks(std::string const& inputString)
{
    std::int16_t maxIndex = static_cast<std::int16_t>(inputString.size() / BLOCK_SIZE);

    std::vector<std::string> stringVec;

    for (int i = 0; i < maxIndex; ++i)
    {
        stringVec.push_back(inputString.substr(i * BLOCK_SIZE, BLOCK_SIZE));
    }
    stringVec.push_back(inputString.substr(maxIndex * BLOCK_SIZE));

    return stringVec;
}

std::vector<std::int16_t> FS::GetUsedMemoryBlockIndices(const dir_entry& dirEntry)
{
    std::int16_t currentIndex = dirEntry.first_blk;
    std::vector<std::int16_t> indexVector;

    while (this->fat[currentIndex] != FAT_EOF)
    {
        indexVector.push_back(currentIndex);
    }

    indexVector.push_back(currentIndex);

    return indexVector;
}
