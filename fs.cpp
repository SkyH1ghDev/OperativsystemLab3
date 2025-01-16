#include "fs.h"
#include <sstream>
#include <cstring>

FS::FS()
{
    std::cout << "Starting Filesystem... \n";
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

    // Format Root-Directory
    std::array<dir_entry, 64> root {};
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

    // Initialize Current Path Stack And Current Directory
    this->currentDirEntry = this->rootDirEntry;
    this->currentPath.push("/");

    this->disk.write(ROOT_BLOCK, reinterpret_cast<std::uint8_t *>(root.data()));
    this->disk.write(FAT_BLOCK, reinterpret_cast<std::uint8_t *>(this->fat.data()));

    return 0;
}

int FS::create(const std::string& filepath)
{
    std::vector<std::string> tokens = TokenizePath(filepath);
    dir_entry startingDirEntry = GetStartingDirectory(filepath);
    std::array<dir_entry, 64> startingDirectory = ReadDirectoryFromDisk(startingDirEntry);

    // Check so that creation is valid
    if (CheckValidCreate(tokens, startingDirectory) == -1)
    {
        return -1;
    }

    // Get the directory where the file is getting created
    std::vector<std::string> creationTokens = {tokens.begin(), tokens.end() - 1};

    dir_entry creationDirEntry;
    GetDirEntry(creationTokens, startingDirectory, creationDirEntry);
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

    std::vector<int16_t> reservedMemoryIndexVector;
    if (ReserveMemory(inputStringBlocks.size(), reservedMemoryIndexVector) == -1)
    {
        std::cout << "Insufficient available space for this file";
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
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::ls()
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::rm(std::string filepath)
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::mkdir(std::string dirpath)
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::cd(std::string dirpath)
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::pwd()
{
    std::cout << "Test" << std::endl;
    return 0;
}

int FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "Test" << std::endl;
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
    std::uint8_t data[sizeof(dir_entry) * directory.size()] = { };

    std::memcpy(data, directory.data(), sizeof(dir_entry) * directory.size());
    disk.write(directoryEntry.first_blk, data);
}

std::array<dir_entry, 64> FS::ReadDirectoryFromDisk(const dir_entry& directoryEntry)
{
    std::uint8_t data[4096];
    disk.read(directoryEntry.first_blk, data);

    dir_entry* dirData = reinterpret_cast<dir_entry*>(data);
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

dir_entry FS::GetStartingDirectory(const std::string& filepath) const
{
    return filepath[0] == '/' ? this->rootDirEntry : this->currentDirEntry;
}

int FS::GetDirEntry(const std::vector<std::string>& tokens, const std::array<dir_entry, 64>& startingDirectory,
                    dir_entry& directoryEntry)
{
    std::array<dir_entry, 64> currentDirectory = startingDirectory;

    if (tokens.empty())
    {
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

                if (dirEntry.access_rights & READ == 0)
                {
                    std::cout << "You Do Not Have The Privileges To Read The File " << dirEntry.file_name << "\n";
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
    if (GetDirEntry(tokens, startingDirectory, dirEntry) == 0)
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
