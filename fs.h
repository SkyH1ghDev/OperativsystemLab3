#include <iostream>
#include <cstdint>
#include "disk.h"
#include <string>
#include <vector>
#include <array>
#include <map>
#include <memory>

#ifndef __FS_H__
#define __FS_H__

#define ROOT_BLOCK 0
#define FAT_BLOCK 1
#define FAT_FREE 0
#define FAT_EOF -1
#define FAT_TEMP_RESERVED -2

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0b100
#define WRITE 0b10
#define EXECUTE 0b1

struct dir_entry
{
	char file_name[56]; // name of the file / sub-directory
	uint32_t size; // size of the file in bytes
	uint16_t first_blk; // index in the FAT for the first block of the file
	uint8_t type; // directory (1), file (0) or initialized (-1)
	uint8_t access_rights; // read (0b100), write (0b01), execute (0b1)
};


class FS
{
private:
	Disk disk;
	// size of a FAT entry is 2 bytes
	int16_t *fat;

	/*
		Custom Created Private Variables
	*/

	template<class T>
	struct TreeNode
	{
		T value;
		std::string name;
		int fatIndex;
		std::vector<std::shared_ptr<TreeNode>> children;
		TreeNode *parent;
	};

	enum FilepathType
	{
		Relative,
		Absolute
	};

	std::map<int, std::string> rightsMap =
			{
					{0,       "-"},
					{READ,    "r-"},
					{WRITE,   "-w-"},
					{EXECUTE, "--x"},
					{READ | WRITE,           "rw-"},
					{READ | EXECUTE,         "r-x"},
					{WRITE | EXECUTE,        "-wx"},
					{READ | WRITE | EXECUTE, "rwx"}
			};


	// Root-node of the entire directory tree
	TreeNode<std::vector<dir_entry>> directoryTree;

	// Node of the current working directory
	TreeNode<std::vector<dir_entry>> *directoryTreeWorkingDirectoryPtr;

	// The filepath to the working directory
	std::string currentFilepath = "/";


	/*
		Custom Created Private Functions
	*/

	// Returns the correct starting node of the directory tree based on whether the filepath is relative or absolute'
	//
	// -----
	//
	// FilepathType filepathType - The type of the filepath (Relative / Absolute)
	TreeNode<std::vector<dir_entry>> *
	GetStartingDirectory(FilepathType filepathType)
	{
		switch (filepathType)
		{
			case Absolute:
				return &this->directoryTree;

			case Relative:
				return this->directoryTreeWorkingDirectoryPtr;
		}
	}

	// Moves down the directoryTree trying to follow the filepath. Fails if filepath is invalid in some way
	//
	// -----
	//
	// std::vector<std::string const &directoryFilenameVector - The vector of filenames of directories. Exclude the filename of the non-directoryNode file (if applicable)
	// TreeNode<std::vector<dir_entry>> const &dirTreeRoot - The tree node from where traversal will begin
	// std::vector<dir_entry>** directoryNode - The address of the directoryNode at the end of the filepath is assigned to this variable
	int TraverseDirectoryTree(std::vector<std::string> const &directoryFilenameVector, FilepathType const &filepathType,
	                          TreeNode<std::vector<dir_entry>> **directoryNode)
	{
		TreeNode<std::vector<dir_entry>> *dirTreeRootPtr = GetStartingDirectory(filepathType);

		if (directoryFilenameVector.empty())
		{
			*directoryNode = dirTreeRootPtr;
			return 0;
		}

		TreeNode<std::vector<dir_entry>> *currDirectoryNode = dirTreeRootPtr;

		for (int i = 0; i < directoryFilenameVector.size(); ++i)
		{
			bool directoryExists = false;

			for (std::shared_ptr<TreeNode<std::vector<dir_entry>>> const &node: currDirectoryNode->children)
			{
				if (node->name == directoryFilenameVector.at(i))
				{
					currDirectoryNode = node.get();
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

		return 0;
	}

	// Creates a directory node, appends it to the directory tree and returns it.
	//
	// -----
	//
	// std::vector<dir_entry> const &value - the directory
	// std::string const &name - the name of the directory
	// TreeNode<std::vector<dir_entry>> *parentNode - pointer to the parent node
	static TreeNode<std::vector<dir_entry>>
	MakeDirectoryTreeNode(std::string const &name, int const &fatIndex,
	                      TreeNode<std::vector<dir_entry>> *parentNode)
	{
		TreeNode<std::vector<dir_entry>> newNodePtr{};

		newNodePtr.value = std::vector<dir_entry>{};
		newNodePtr.name = name;
		newNodePtr.parent = parentNode;
		newNodePtr.fatIndex = fatIndex;
		newNodePtr.children = std::vector<std::shared_ptr<TreeNode<std::vector<dir_entry>>>>{};

		return newNodePtr;
	}

	void ReadDirectoriesFromFat();

	void ReadFatFromDisk();

	// FS()

	void WriteFatToMemory();

	// ~FS()

	void FormatBlocks();

	void FormatRoot();

	// Format()

	static std::vector<std::string> SplitFilepath(std::string const &filepath);

	static std::string GetFilenameFromFilepath(std::string const &filepath);

	int CheckValidCreate(std::string const &filepath);

	static void SaveInputToString(int &length, std::string &inputString);

	static std::vector<std::string> DivideStringIntoBlocks(std::string const &inputString);

	int FindFreeMemoryBlocks(int const &totalRequired, std::vector<int> &indexVector);

	static dir_entry MakeDirEntry(std::string const &filename, int const &size, int const &firstBlock, int const &type,
	                              int const &accessRights);

	int
	WriteFileToMemory(std::vector<dir_entry> &directory, int const &directoryFatIndex, dir_entry const &dirEntry,
	                  std::vector<int> const &indexVector,
	                  std::vector<std::string> const &blockVector);

	// Create()

	std::string ReadFileBlocksFromMemory(dir_entry const &file);

	int
	GetDirEntry(std::string const &filepath, dir_entry **file);

	// Cat()

	// Ls()

	// Cp()

	// Mv()

	void FreeMemoryBlocks(dir_entry &file);

	// Rm()

	std::vector<int> GetBlockIndices(dir_entry &file);

	// Append()

	int WriteDirectoryToMemory(std::vector<dir_entry> &parentDirectory, dir_entry const &dirEntry,
	                           std::vector<dir_entry> &directory);

	// Mkdir()
public:
	FS();

	~FS();

	// formats the disk, i.e., creates an empty file system
	int format();

	// create <filepath> creates a new file on the disk, the data content is
	// written on the following rows (ended with an empty row)
	int create(const std::string &filepath);

	// cat <filepath> reads the content of a file and prints it on the screen
	int cat(const std::string &filepath);

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

	int filenameVector;
};

#endif // __FS_H__
