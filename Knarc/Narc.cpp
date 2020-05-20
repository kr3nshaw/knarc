#include "Narc.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

using namespace std;

void Chdir(const string& path)
{
#ifdef _WIN32
	_chdir(path.c_str());
#else
	chdir(path.c_str());
#endif
}

void Mkdir(const string& path)
{
#ifdef _WIN32
	_mkdir(path.c_str());
#else
	mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
}

NarcError Narc::GetError() const
{
	return error;
}

bool Narc::Pack(const string& fileName, const string& directory)
{
	ofstream ofs(fileName, ios::out | ios::binary);

	if (!ofs.good())
	{
		error = NarcError::InvalidOutputFile;

		goto Cleanup;
	}

	Chdir(directory);

	// TODO: Implement packing

	Cleanup:
	ofs.close();

	return error == NarcError::None ? true : false;
}

bool Narc::Unpack(const string& fileName, const string& directory)
{
	FileAllocationTableEntry* fatEntries = nullptr;
	uint16_t* fntEntries = nullptr;
	char* buffer = nullptr;

	ifstream ifs(fileName, ios::in | ios::binary);

	if (!ifs.good())
	{
		error = NarcError::InvalidInputFile;

		goto Cleanup;
	}

	Header header;
	ifs.read(reinterpret_cast<char*>(&header), sizeof(Header));

	if (header.Id != 0x4352414E)
	{
		error = NarcError::InvalidHeaderId;

		goto Cleanup;
	}

	if (header.ByteOrderMark != 0xFFFE)
	{
		error = NarcError::InvalidByteOrderMark;

		goto Cleanup;
	}

	if (header.Version != 0x100)
	{
		error = NarcError::InvalidVersion;

		goto Cleanup;
	}

	if (header.ChunkSize != 0x10)
	{
		error = NarcError::InvalidHeaderSize;

		goto Cleanup;
	}

	if (header.ChunkCount != 0x3)
	{
		error = NarcError::InvalidChunkCount;

		goto Cleanup;
	}

	FileAllocationTable fat;
	ifs.read(reinterpret_cast<char*>(&fat), sizeof(FileAllocationTable));

	if (fat.Id != 0x46415442)
	{
		error = NarcError::InvalidFileAllocationTableId;

		goto Cleanup;
	}

	if (fat.Reserved != 0x0)
	{
		error = NarcError::InvalidFileAllocationTableReserved;

		goto Cleanup;
	}

	fatEntries = new FileAllocationTableEntry[fat.FileCount];

	for (int i = 0; i < fat.FileCount; ++i)
	{
		ifs.read(reinterpret_cast<char*>(&fatEntries[i]), sizeof(FileAllocationTableEntry));
	}

	FileNameTable fnt;
	ifs.read(reinterpret_cast<char*>(&fnt), sizeof(FileNameTable));

	if (fnt.Id != 0x464E5442)
	{
		error = NarcError::InvalidFileNameTableId;

		goto Cleanup;
	}

	fntEntries = new uint16_t[fnt.DirectoryCount];

	for (int i = 0; i < fnt.DirectoryCount; ++i)
	{
		ifs.read(reinterpret_cast<char*>(&fntEntries[i]), sizeof(uint16_t));
	}

	// TODO: Actually read FNT sub-tables

	ifs.seekg(static_cast<uint64_t>(header.ChunkSize) + static_cast<uint64_t>(fat.ChunkSize) + static_cast<uint64_t>(fnt.ChunkSize));

	FileImages fi;
	ifs.read(reinterpret_cast<char*>(&fi), sizeof(FileImages));

	if (fi.Id != 0x46494D47)
	{
		error = NarcError::InvalidFileImagesId;

		goto Cleanup;
	}

	Mkdir(directory);
	Chdir(directory);

	for (int i = 0; i < fat.FileCount; ++i)
	{
		buffer = new char[fatEntries[i].End - fatEntries[i].Start];
		ifs.read(buffer, fatEntries[i].End - fatEntries[i].Start);

		ostringstream oss;
		oss << setfill('0') << setw(4) << i;

		ofstream ofs(oss.str() + ".bin", ios::out | ios::binary);

		if (!ofs.good())
		{
			error = NarcError::InvalidOutputFile;

			goto Cleanup;
		}

		ofs.write(buffer, fatEntries[i].End - fatEntries[i].Start);
		ofs.close();

		delete[] buffer;
	}

	Cleanup:
	if (buffer)
	{
		delete[] buffer;
	}

	if (fntEntries)
	{
		delete[] fntEntries;
	}

	if (fatEntries)
	{
		delete[] fatEntries;
	}

	ifs.close();

	return error == NarcError::None ? true : false;
}
