#pragma once

#include <cstdint>
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

struct FileImages
{
	uint32_t Id;
	uint32_t ChunkSize;
};

struct Narc
{
	public:
		NarcError GetError() const;

		bool Pack(const std::string& fileName, const std::string& directory);
		bool Unpack(const std::string& fileName, const std::string& directory);

	private:
		NarcError error = NarcError::None;
};
