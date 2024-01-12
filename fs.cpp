#include <sstream>
#include "fs.h"
#include <random>
#include <cstring>
#include <algorithm>

void FS::ReadFatFromDisk()
{
	this->fat = new int16_t[BLOCK_SIZE];

	disk.read(FAT_BLOCK, (uint8_t *) this->fat);
}

void FS::ReadDirectoriesFromFat()
{

}

FS::FS()
{
	std::cout << "FS::FS()... Creating file system\n";
	ReadFatFromDisk();
	ReadDirectoriesFromFat();

	srand(time(nullptr));
}

void FS::WriteFatToMemory()
{
	disk.write(FAT_BLOCK, (uint8_t *) this->fat);
}

FS::~FS()
{
	WriteFatToMemory();

	delete[] this->fat;
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
void FS::FormatRoot()
{
	std::vector<dir_entry> root{};
	root.reserve(64);

	this->directoryTree.value = root;
	this->directoryTree.name = "/";
	this->directoryTree.parent = nullptr;
	this->directoryTree.fatIndex = ROOT_BLOCK;
	this->directoryTree.children = std::vector<std::shared_ptr<TreeNode<std::vector<dir_entry>>>>{};

	this->directoryTreeWorkingDirectoryPtr = &this->directoryTree;
}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
	FormatBlocks();
	FormatRoot(); // Initializes Root-directory

	return 0;
}

// Splits filepath string into substrings of subdirectories and filename 
// and returns vector of them (Does not include the root ("") as a substring)
//
// -----
//
// std::string const &filepath - The filepath 
std::vector<std::string> FS::SplitFilepath(std::string const &filepath)
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

// Returns the last substring in subdirectory and filename vector
//
// -----
//
// std::string const &filepath - The filepath 
std::string FS::GetFilenameFromFilepath(std::string const &filepath)
{
	std::vector<std::string> stringVector = SplitFilepath(filepath);
	return stringVector.back();
}

// Checks whether the create-command is valid
//
// -----
//
// std::string const &filepath - The filepath
int FS::CheckValidCreate(std::string const &filepath)
{
	std::string filename = GetFilenameFromFilepath(filepath);

	if (filename.length() >= 56)
	{
		std::cout << "Filename is too long" << std::endl;
		return -1;
	}

	if (filename[0] == '.')
	{
		std::cout << "Filename can't start with '.'" << std::endl;
		return -1;
	}

	FilepathType filepathType = filepath[0] == '/' ? Absolute : Relative;
	TreeNode<std::vector<dir_entry>> *startingNode = GetStartingDirectory(filepathType);

	for (auto file: startingNode->value)
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
void FS::SaveInputToString(int &size, std::string &inputString)
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

// Divides input string into 4096-byte sized blocks and returns a vector with these blocks.
//
// -----
//
// std::string const &inputString - The string that was input in the shell
std::vector<std::string> FS::DivideStringIntoBlocks(std::string const &inputString)
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
int FS::FindFreeMemoryBlocks(int const &totalRequired, std::vector<int> &indexVector)
{
	std::vector<int> tempIndexVector;

	int alreadyExisting = indexVector.size();

	// Makes it possible to append with already existing blocks instead of
	// constantly freeing blocks and looking for new ones
	int additionalRequired = totalRequired - alreadyExisting;

	for (int i = 0; i < additionalRequired; ++i)
	{
		int randomIndex = rand() % 2045 + 2;

		for (int j = randomIndex;; ++j)
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

				for (auto index: tempIndexVector)
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

	for (int blockIndex: tempIndexVector)
	{
		indexVector.push_back(blockIndex);
	}

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
dir_entry FS::MakeDirEntry(std::string const &filename, int const &size, int const &firstBlock, int const &type,
                           int const &accessRights)
{
	dir_entry DirEntry{};

	strcpy(DirEntry.file_name, filename.c_str());
	DirEntry.size = static_cast<uint32_t>(size);
	DirEntry.first_blk = static_cast<uint16_t>(firstBlock);
	DirEntry.type = static_cast<uint8_t>(type);
	DirEntry.access_rights = static_cast<uint8_t>(accessRights);

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
int
FS::WriteFileToMemory(std::vector<dir_entry> &directory, int const &directoryFatIndex, dir_entry const &dirEntry,
                      std::vector<int> const &indexVector,
                      std::vector<std::string> const &blockVector)
{
	if (directory.size() == 64)
	{
		std::cout << "Directory is already at max capacity (64)" << std::endl;
		return -1;
	}

	for (int i = 0; i < indexVector.size() - 1; ++i)
	{
		this->fat[indexVector.at(i)] = indexVector.at(i + 1);
		disk.write(indexVector.at(i), (uint8_t *) blockVector.at(
				i).c_str()); // Neither static_cast nor reinterpret_cast work. C-style conversion works though.
	}

	this->fat[indexVector.back()] = FAT_EOF;

	std::string const &lastBlock = blockVector.back();

	disk.write(indexVector.back(), (uint8_t *) lastBlock.c_str());

	directory.push_back(dirEntry);

	for (int i = 0; i < directory.size(); ++i)
	{
		disk.write(directoryFatIndex, (uint8_t *) (directory.data() + i));
	}

	return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string const &filepath)
{
	if (CheckValidCreate(filepath) == -1)
	{
		return -1;
	}

	int size = 0;
	std::string inputString;
	SaveInputToString(size, inputString);

	std::vector<std::string> blockVector = DivideStringIntoBlocks(inputString);


	std::vector<int> indexVector;
	if (FindFreeMemoryBlocks(blockVector.size(), indexVector) == -1)
	{
		return -1;
	}

	std::vector<std::string> filenameVector = SplitFilepath(filepath);
	FilepathType filepathType = filepath[0] == '/' ? Absolute : Relative;
	std::vector<std::string> directoryFilenames(filenameVector.begin(), filenameVector.end() - 1);
	TreeNode<std::vector<dir_entry>> *directoryNodePtr{};

	if (TraverseDirectoryTree(directoryFilenames, filepathType, &directoryNodePtr) == -1)
	{
		return -1;
	}

	std::string filename = GetFilenameFromFilepath(filepath);

	dir_entry dirEntry = MakeDirEntry(filename, size, indexVector.at(0), TYPE_FILE, READ | WRITE | EXECUTE);

	WriteFileToMemory(directoryNodePtr->value, directoryNodePtr->fatIndex, dirEntry, indexVector, blockVector);

	return 0;
}

// Reads the contents of a file
//
// -----
//
// dir_entry &file - The file that is to be read.
std::string FS::ReadFileBlocksFromMemory(dir_entry const &file)
{
	int16_t currentBlock = file.first_blk;
	std::cout << file.first_blk;
	std::string readString;

	while (currentBlock != FAT_EOF)
	{
		char readBlock[BLOCK_SIZE];

		disk.read(currentBlock, (uint8_t *) readBlock);

		readString += std::string((char *) readBlock);

		currentBlock = this->fat[currentBlock];
	}

	return readString;
}

// Gets the file at the end of a filepath.
//
// -----
//
// std::string const &filepath - The filepath
// dir_entry **file - The pointer to the file at the end of the filepath is assigned to this variable
int FS::GetDirEntry(std::string const &filepath, dir_entry **file)
{
	std::vector<std::string> filenameVector = SplitFilepath(filepath);

	if (filenameVector.empty())
	{
		std::cout << "The file does not exist" << std::endl;
		return -1;
	}

	FilepathType filepathType = filepath[0] == '/' ? Absolute : Relative;
	std::vector<std::string> directoryFilenames(filenameVector.begin(), filenameVector.end() - 1);
	TreeNode<std::vector<dir_entry>> *workingDirectoryNodePtr;

	if (TraverseDirectoryTree(directoryFilenames, filepathType, &workingDirectoryNodePtr) == -1)
	{
		return -1;
	}

	if (workingDirectoryNodePtr->value.empty())
	{
		std::cout << "The file does not exist" << std::endl;
		return -1;
	}

	for (dir_entry &tempFile: workingDirectoryNodePtr->value)
	{
		if (tempFile.file_name == filenameVector.back())
		{
			*file = &tempFile;
			return 0;
		}

	}

	std::cout << "The file does not exist" << std::endl;
	return -1;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string const &filepath)
{
	dir_entry *filePtr{};

	if (GetDirEntry(filepath, &filePtr) == -1)
	{
		return -1;
	}

	if (filePtr->type == TYPE_DIR)
	{
		std::cout << "Can't read file of type directory" << std::endl;
		return -1;
	}

	std::cout << ReadFileBlocksFromMemory(*filePtr) << std::endl;

	return 0;
}

// ls lists the content in the current directory (files and sub-directories)
int FS::ls()
{
	std::cout << "Filename\t\t\tType\t\tAccessrights\t\tSize (Bytes)\t\tFirst Block \n";

	for (dir_entry &file: this->directoryTreeWorkingDirectoryPtr->value)
	{
		std::cout << file.file_name << "\t\t\t\t"
		          << (file.type == TYPE_DIR ? "Dir" : "File") << "\t\t"
		          << rightsMap[file.access_rights] << "\t\t\t"
		          << (file.type == TYPE_DIR ? "-" : std::to_string(file.size)) << "\t\t\t"
		          << file.first_blk << std::endl;
	}

	return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{
	if (CheckValidCreate(destpath) == -1)
	{
		return -1;
	}

	std::vector<std::string> sourceFilenameVector = SplitFilepath(sourcepath);

	dir_entry *sourceFilePtr{};

	if (GetDirEntry(sourcepath, &sourceFilePtr) == -1)
	{
		return -1;
	}

	std::string sourceString = ReadFileBlocksFromMemory(*sourceFilePtr);

	std::vector<std::string> blockVector = DivideStringIntoBlocks(sourceString);

	std::vector<int> indexVector;

	if (FindFreeMemoryBlocks(blockVector.size(), indexVector) == -1)
	{
		return -1;
	}

	std::string copyFilename = GetFilenameFromFilepath(destpath);

	dir_entry copyDirEntry = MakeDirEntry(copyFilename, sourceFilePtr->size, indexVector.at(0), TYPE_FILE,
	                                      READ | WRITE | EXECUTE);

	std::vector<std::string> destFilenameVector = SplitFilepath(destpath);
	std::vector<std::string> destDirectoryFilenameVector(destFilenameVector.begin(), destFilenameVector.end() + 1);
	FilepathType filepathType = destpath[0] == '/' ? Absolute : Relative;
	TreeNode<std::vector<dir_entry>> *directoryNodePtr;

	if (TraverseDirectoryTree(destDirectoryFilenameVector, filepathType, &directoryNodePtr) == -1)
	{
		return -1;
	}

	WriteFileToMemory(directoryNodePtr->value, directoryNodePtr->fatIndex,
	                  copyDirEntry,
	                  indexVector, blockVector);


	return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
	if (CheckValidCreate(destpath) == -1)
	{
		return -1;
	}

	std::vector<std::string> sourceFilenameVector = SplitFilepath(sourcepath);
	std::vector<std::string> destFilenameVector = SplitFilepath(destpath);

	dir_entry *sourceFilePtr{};

	if (GetDirEntry(sourcepath, &sourceFilePtr) == -1)
	{
		return -1;
	}

	FilepathType sourceFilepathType = sourcepath[0] == '/' ? Absolute : Relative;
	TreeNode<std::vector<dir_entry>> *sourceDirectoryNodePtr;
	std::vector<std::string> sourceDirectoryFilenameVector = std::vector<std::string>(sourceFilenameVector.begin(),
	                                                                                  sourceFilenameVector.end() - 1);

	if (TraverseDirectoryTree(sourceDirectoryFilenameVector, sourceFilepathType, &sourceDirectoryNodePtr) == -1)
	{
		return -1;
	}

	FilepathType destFilepathType = destpath[0] == '/' ? Absolute : Relative;
	TreeNode<std::vector<dir_entry>> *destDirectoryNodePtr;
	std::vector<std::string> destDirectoryFilenameVector = std::vector<std::string>(destFilenameVector.begin(),
	                                                                                destFilenameVector.end() - 1);

	if (TraverseDirectoryTree(destDirectoryFilenameVector, destFilepathType, &destDirectoryNodePtr) == -1)
	{
		return -1;
	}

	strcpy(sourceFilePtr->file_name, GetFilenameFromFilepath(destpath).c_str());

	for (int i = 0; i < sourceDirectoryNodePtr->value.size(); ++i)
	{
		if (strcmp((char *) sourceFilePtr->file_name, (char *) sourceDirectoryNodePtr->value.at(i).file_name) == 0)
		{
			sourceDirectoryNodePtr->value.erase(sourceDirectoryNodePtr->value.begin() + i);
		}
	}

	destDirectoryNodePtr->value.push_back(*sourceFilePtr);

	return 0;
}

void FS::FreeMemoryBlocks(dir_entry &file)
{
	int16_t currentBlock = file.first_blk;

	if (this->fat[currentBlock] == FAT_EOF)
	{
		this->fat[currentBlock] = FAT_FREE;
		return;
	}

	while (true)
	{
		int16_t nextBlock = this->fat[currentBlock];

		this->fat[currentBlock] = FAT_FREE;

		if (this->fat[nextBlock] == FAT_EOF)
		{
			this->fat[nextBlock] = FAT_FREE;
			return;
		}

		currentBlock = nextBlock;
	}
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
	dir_entry *file{};

	if (GetDirEntry(filepath, &file) == -1)
	{
		return -1;
	}

	FreeMemoryBlocks(*file);

	std::vector<std::string> filenameVector = SplitFilepath(filepath);
	FilepathType filepathType = filepath[0] == '/' ? Absolute : Relative;
	std::vector<std::string> directoryFilenames(filenameVector.begin(), filenameVector.end() - 1);
	TreeNode<std::vector<dir_entry>> *currentDirectoryNodePtr;

	if (TraverseDirectoryTree(directoryFilenames, filepathType,
	                          &currentDirectoryNodePtr) == -1)
	{
		return -1;
	}

	for (int i = 0; i < currentDirectoryNodePtr->value.size(); ++i)
	{
		if (strcmp((char *) file->file_name, (char *) currentDirectoryNodePtr->value.at(i).file_name) == 0)
		{
			currentDirectoryNodePtr->value.erase(currentDirectoryNodePtr->value.begin() + i);
			break;
		}
	}

	return 0;
}

// Returns std::vector<int> of the block indices that a file has reserved
//
// -----
//
// dir_entry file - The file
std::vector<int> FS::GetBlockIndices(dir_entry &file)
{
	int currentIndex = file.first_blk;
	std::vector<int> indexVector;

	while (this->fat[currentIndex] != FAT_EOF)
	{
		indexVector.push_back(currentIndex);
	}

	indexVector.push_back(currentIndex);

	return indexVector;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
	dir_entry *sourceFilePtr{};

	if (GetDirEntry(filepath1, &sourceFilePtr) == -1)
	{
		return -1;
	}

	std::string sourceString = ReadFileBlocksFromMemory(*sourceFilePtr);

	dir_entry *destFilePtr{};

	if (GetDirEntry(filepath2, &destFilePtr) == -1)
	{
		return -1;
	}

	std::string destString = ReadFileBlocksFromMemory(*destFilePtr);

	std::string totalString = destString + "\n" + sourceString;


	std::cout << "destString: " << destString << "\n";
	std::cout << "sourceString: " << sourceString << "\n";

	destFilePtr->size = totalString.length() + 1;

	std::vector<std::string> filenameVector = SplitFilepath(filepath2);
	std::vector<std::string> directoryVector(filenameVector.begin(), filenameVector.end() - 1);
	FilepathType destFilepathType = filepath2[0] == '/' ? Absolute : Relative;
	TreeNode<std::vector<dir_entry>> *directoryNodePtr;

	if (TraverseDirectoryTree(directoryVector, destFilepathType, &directoryNodePtr) == -1)
	{
		return -1;
	}

	dir_entry destFileCopy = *destFilePtr;

	// Must temporarily erase the dir_entry as it would cause a duplicates otherwise
	for (int i = 0; i < directoryNodePtr->value.size(); ++i)
	{
		if (strcmp((char *) directoryNodePtr->value.at(i).file_name, (char *) destFilePtr->file_name) == 0)
		{
			directoryNodePtr->value.erase(directoryNodePtr->value.begin() + i);
			break;
		}
	}

	std::vector<int> indexVector = GetBlockIndices(destFileCopy);
	std::vector<std::string> blockVector = DivideStringIntoBlocks(totalString);

	if (WriteFileToMemory(directoryNodePtr->value, directoryNodePtr->fatIndex, destFileCopy, indexVector,
	                      blockVector) == -1)
	{
		return -1;
	}

	return 0;
}

int FS::WriteDirectoryToMemory(std::vector<dir_entry> &parentDirectory, dir_entry const &dirEntry,
                               std::vector<dir_entry> &directory)
{
	if (parentDirectory.size() == 64)
	{
		std::cout << "Directory already at max capacity (64)" << std::endl;
		return -1;
	}

	dir_entry dirEntryArray[64] = {0};
	std::copy(directory.begin(), directory.end(), dirEntryArray);

	disk.write(dirEntry.first_blk, (uint8_t *) dirEntryArray);
	std::cout << (uint8_t *) dirEntryArray;
	this->fat[dirEntry.first_blk] = FAT_EOF;

	parentDirectory.push_back(dirEntry);

	// Forgor to write parent directory to disk

	return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
	if (CheckValidCreate(dirpath) == -1)
	{
		return -1;
	}

	std::vector<int> indexVector{};
	if (FindFreeMemoryBlocks(1, indexVector) == -1)
	{
		return -1;
	}

	dir_entry directoryDirEntry = MakeDirEntry(GetFilenameFromFilepath(dirpath), 0, indexVector.front(), TYPE_DIR,
	                                           READ | WRITE | EXECUTE);

	std::vector<std::string> filenameVector = SplitFilepath(dirpath);
	std::vector<std::string> directoryFilenameVector(filenameVector.begin(), filenameVector.end() - 1);
	FilepathType filepathType = dirpath[0] == '/' ? Absolute : Relative;
	TreeNode<std::vector<dir_entry>> *parentDirectoryNodePtr{};
	if (TraverseDirectoryTree(directoryFilenameVector, filepathType, &parentDirectoryNodePtr) == -1)
	{
		return -1;
	}

	TreeNode<std::vector<dir_entry>> newDirectoryNode = MakeDirectoryTreeNode(GetFilenameFromFilepath(dirpath),
	                                                                          directoryDirEntry.first_blk,
	                                                                          parentDirectoryNodePtr);

	parentDirectoryNodePtr->children.push_back(std::make_shared<TreeNode<std::vector<dir_entry>>>(newDirectoryNode));

	if (WriteDirectoryToMemory(parentDirectoryNodePtr->value, directoryDirEntry, newDirectoryNode.value) == -1)
	{
		return -1;
	}

	return 0;
}

std::string FS::ConcatenateFilepath(std::stack<std::string> &filenameStack)
{
	std::string filepath{};

	if (filenameStack.empty())
	{
		return "/";
	}

	for (int i = 0;
	     i <= filenameStack.size(); ++i) // needs to be i <= for some reason. Otherwise it doesn't prepend last element
	{
		std::cout << filenameStack.top() << std::endl;
		filepath = "/" + filenameStack.top() + filepath; // Needs to be prepended
		filenameStack.pop();
	}

	return filepath;
}

std::string FS::GetUpdatedCurrentDirectoryPath(std::vector<std::string> const &directoryFilenameVector,
                                               FilepathType const &filepathType)
{
	std::stack<std::string> directoryFilenameStack;

	if (filepathType == Relative)
	{
		std::vector<std::string> currentFilepathFilenameVector = SplitFilepath(this->currentFilepath);

		for (std::string const &string: currentFilepathFilenameVector)
		{
			std::cout << string << std::endl;
			directoryFilenameStack.push(string);
		}
	}

	for (std::string const &string: directoryFilenameVector)
	{
		if (string == "..")
		{
			directoryFilenameStack.pop();
		}
		else
		{
			directoryFilenameStack.push(string);
		}
	}

	return ConcatenateFilepath(directoryFilenameStack);
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
	std::vector<std::string> directoryFilenameVector = SplitFilepath(dirpath);
	FilepathType filepathType = dirpath[0] == '/' ? Absolute : Relative;
	TreeNode<std::vector<dir_entry>> *newCurrentDirectoryPtr{};
	if (TraverseDirectoryTree(directoryFilenameVector, filepathType, &newCurrentDirectoryPtr) == -1)
	{
		return -1;
	}
	this->directoryTreeWorkingDirectoryPtr = newCurrentDirectoryPtr;

	this->currentFilepath = GetUpdatedCurrentDirectoryPath(directoryFilenameVector, filepathType);

	return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
	std::cout << this->currentFilepath << std::endl;

	return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
	std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
	return 0;
}
