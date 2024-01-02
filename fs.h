#include <iostream>
#include <cstdint>
#include "disk.h"
#include <string>
#include <vector>
#include <array>
#include <map>

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
	int16_t *fat = new int16_t[BLOCK_SIZE / 2];

	/*
		Custom Created Private Variables
	*/

	template<class T>
	struct TreeNode
	{
		T value;
		std::string name;
		std::vector<TreeNode> children;
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
	TreeNode<std::vector<dir_entry>> directoryTreeWorkingDirectory;

	// The filepath to the working directory
	std::string currentFilepath = "";


	/*
		Custom Created Private Functions
	*/

	void FormatBlocks();

	void InitializeRoot();

	// Format()

	static std::vector<std::string> SplitFilepath(std::string const &filepath);

	static std::string ConcatenateFilepath(std::vector<std::string> const &filenames);

	static std::string GetFilenameFromFilepath(std::string const &filepath);

	int CheckValidCreate(std::string const &filepath) const;

	static void SaveInputToString(int &length, std::string &inputString);

	static std::vector<std::string> DivideStringIntoBlocks(std::string const &inputString);

	int FindFreeMemoryBlocks(int const &amount, std::vector<int> &indexVector);

	static dir_entry MakeDirEntry(std::string const &filename, int const &size, int const &firstBlock, int const &type,
	                              int const &accessRights);

	int WriteToMemory(std::vector<dir_entry> &directory, dir_entry const &dirEntry, std::vector<int> const &indexVector,
	                  std::vector<std::string> const &blockVector);

	// Create()

	std::string ReadBlocksFromMemory(dir_entry &file);

	static int TraverseDirectoryTree(std::string const &filepath, TreeNode<std::vector<dir_entry>> &dirTreeRoot,
	                                 std::vector<dir_entry> **directory);

	static int
	GetFileDirEntry(std::string const &filepath, TreeNode<std::vector<dir_entry>> &dirTreeRoot, dir_entry &file);

	// Cat()

	// Ls()

	// Cp()

	// Mv()

	void FreeMemoryBlocks(dir_entry &file);

	// Rm()


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
};

#endif // __FS_H__
