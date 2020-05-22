#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

enum class NarcError
{
	None,
	InvalidInputFile,
	InvalidHeaderId,
	InvalidByteOrderMark,
	InvalidVersion,
	InvalidHeaderSize,
	InvalidChunkCount,
	InvalidFileAllocationTableId,
	InvalidFileAllocationTableReserved,
	InvalidFileNameTableId,
	InvalidFileImagesId,
	InvalidOutputFile
};

struct Header
{
	uint32_t Id;
	uint16_t ByteOrderMark;
	uint16_t Version;
	uint32_t FileSize;
	uint16_t ChunkSize;
	uint16_t ChunkCount;
};

struct FileAllocationTable
{
	uint32_t Id;
	uint32_t ChunkSize;
	uint16_t FileCount;
	uint16_t Reserved;
};

struct FileAllocationTableEntry
{
	uint32_t Start;
	uint32_t End;
};

struct FileNameTable
{
	uint32_t Id;
	uint32_t ChunkSize;
	uint32_t SubTableOffset;
	uint16_t FirstFileId;
	uint16_t DirectoryCount;
};

struct FileNameTableEntry
{
	uint8_t NameLength;
	char* Name;
	uint16_t SubDirectoryID;

};

struct FileImages
{
	uint32_t Id;
	uint32_t ChunkSize;
};

class Narc
{
	public:
		NarcError GetError() const;

		bool Pack(const std::filesystem::path& fileName, const std::filesystem::path& directory);
		bool Unpack(const std::filesystem::path& fileName, const std::filesystem::path& directory);

	private:
		NarcError error = NarcError::None;

		bool Cleanup(std::ifstream& ifs, const NarcError& ne);
		bool Cleanup(std::ofstream& ofs, const NarcError& ne);
};
