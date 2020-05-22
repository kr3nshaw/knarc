#include "Narc.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

using namespace std;

bool Narc::Cleanup(ifstream& ifs, const NarcError& ne)
{
	ifs.close();

	error = ne;

	return false;
}

bool Narc::Cleanup(ofstream& ofs, const NarcError& ne)
{
	ofs.close();

	error = ne;

	return false;
}

NarcError Narc::GetError() const
{
	return error;
}

bool Narc::Pack(const filesystem::path& fileName, const filesystem::path& directory)
{
	ofstream ofs(fileName, ios::binary);

	if (!ofs.good()) { return Cleanup(ofs, NarcError::InvalidOutputFile); }

	vector<FileAllocationTableEntry> fatEntries;

	for (const auto& file : filesystem::directory_iterator(directory))
	{
		fatEntries.push_back(FileAllocationTableEntry
			{
				.Start = 0,
				.End = 0
			});

		if (fatEntries.size() > 1)
		{
			fatEntries.back().Start = fatEntries.rbegin()[1].End;

			if ((fatEntries.rbegin()[1].End % 4) != 0)
			{
				fatEntries.back().Start += 4 - (fatEntries.rbegin()[1].End % 4);
			}
		}

		fatEntries.back().End = fatEntries.back().Start + file.file_size();
	}

	FileAllocationTable fat
	{
		.Id = 0x46415442,
		.ChunkSize = sizeof(FileAllocationTable) + ((uint32_t)fatEntries.size() * sizeof(FileAllocationTableEntry)),
		.FileCount = static_cast<uint16_t>(fatEntries.size()),
		.Reserved = 0x0
	};

	FileNameTable fnt
	{
		.Id = 0x464E5442,
		.ChunkSize = 0x10,
		.SubTableOffset = 0x4,
		.FirstFileId = 0x0,
		.DirectoryCount = 0x1
	};

	// TODO: Actually write FNT sub-tables

	FileImages fi
	{
		.Id = 0x46494D47,
		.ChunkSize = sizeof(FileImages) + fatEntries.back().End
	};

	if ((fi.ChunkSize % 4) != 0)
	{
		fi.ChunkSize += 4 - (fi.ChunkSize % 4);
	}

	Header header
	{
		.Id = 0x4352414E,
		.ByteOrderMark = 0xFFFE,
		.Version = 0x100,
		.FileSize = sizeof(Header) + fat.ChunkSize + fnt.ChunkSize + fi.ChunkSize,
		.ChunkSize = 0x10,
		.ChunkCount = 0x3
	};

	ofs.write(reinterpret_cast<char*>(&header), sizeof(Header));
	ofs.write(reinterpret_cast<char*>(&fat), sizeof(FileAllocationTable));

	for (auto& entry : fatEntries)
	{
		ofs.write(reinterpret_cast<char*>(&entry), sizeof(FileAllocationTableEntry));
	}

	ofs.write(reinterpret_cast<char*>(&fnt), sizeof(FileNameTable));
	ofs.write(reinterpret_cast<char*>(&fi), sizeof(FileImages));

	for (const auto& file : filesystem::directory_iterator(directory))
	{
		ifstream ifs(file.path(), ios::binary | ios::ate);

		if (!ifs.good())
		{
			ifs.close();

			return Cleanup(ofs, NarcError::InvalidInputFile);
		}

		streampos length = ifs.tellg();
		unique_ptr<char[]> buffer(new char[length]);

		ifs.seekg(0);
		ifs.read(buffer.get(), length);
		ifs.close();

		ofs.write(buffer.get(), length);

		if ((ofs.tellp() % 4) != 0)
		{
			for (int i = 4 - (ofs.tellp() % 4); i-- > 0; )
			{
				uint8_t ff = 0xFF;
				ofs.write(reinterpret_cast<char*>(&ff), sizeof(uint8_t));
			}
		}
	}

	ofs.close();

	return error == NarcError::None ? true : false;
}

bool Narc::Unpack(const filesystem::path& fileName, const filesystem::path& directory)
{
	ifstream ifs(fileName, ios::binary);

	if (!ifs.good()) { return Cleanup(ifs, NarcError::InvalidInputFile); }

	Header header;
	ifs.read(reinterpret_cast<char*>(&header), sizeof(Header));

	if (header.Id != 0x4352414E) { return Cleanup(ifs, NarcError::InvalidHeaderId); }
	if (header.ByteOrderMark != 0xFFFE) { return Cleanup(ifs, NarcError::InvalidByteOrderMark); }
	if (header.Version != 0x100) { return Cleanup(ifs, NarcError::InvalidVersion); }
	if (header.ChunkSize != 0x10) { return Cleanup(ifs, NarcError::InvalidHeaderSize); }
	if (header.ChunkCount != 0x3) { return Cleanup(ifs, NarcError::InvalidChunkCount); }

	FileAllocationTable fat;
	ifs.read(reinterpret_cast<char*>(&fat), sizeof(FileAllocationTable));

	if (fat.Id != 0x46415442) { return Cleanup(ifs, NarcError::InvalidFileAllocationTableId); }
	if (fat.Reserved != 0x0) { return Cleanup(ifs, NarcError::InvalidFileAllocationTableReserved); }

	unique_ptr<FileAllocationTableEntry[]> fatEntries(new FileAllocationTableEntry[fat.FileCount]);

	for (int i = 0; i < fat.FileCount; ++i)
	{
		ifs.read(reinterpret_cast<char*>(&fatEntries[i]), sizeof(FileAllocationTableEntry));
	}

	FileNameTable fnt;
	vector<FileNameTableEntry> FileNameTableEntries;
	ifs.read(reinterpret_cast<char*>(&fnt), sizeof(FileNameTable));

	if (fnt.Id != 0x464E5442) { return Cleanup(ifs, NarcError::InvalidFileNameTableId); }

	// Attempt to read subtables.
	if (fnt.ChunkSize > 0x10) // Temporary until I figure out wtf is going on
	{
		uint32_t fimg_check;
		ifs.read(reinterpret_cast<char*>(&fimg_check), 4);
		while (fimg_check != 0x46494D47)
		{
			ifs.seekg((uint32_t)ifs.tellg() - 4);

			FileNameTableEntry e;
			// Get Name Length.
			ifs.read(reinterpret_cast<char*>(&e.NameLength), 1);

			// Create and write to a buffer to store the name.
			unique_ptr<char[]> temp(new char[e.NameLength]);
			ifs.read(reinterpret_cast<char*>(&temp), e.NameLength);
			temp[e.NameLength] = '\0'; // Null terminator

			// Assign the name to the entry.
			e.Name = reinterpret_cast<char*>(&temp);

			// Read SubDirectoryID.
			ifs.read(reinterpret_cast<char*>(&e.SubDirectoryID), 2);

			// Add to Vector.
			FileNameTableEntries.push_back(e);
			ifs.read(reinterpret_cast<char*>(&fimg_check), 4);
		}
	}

	FileImages fi;
	ifs.read(reinterpret_cast<char*>(&fi), sizeof(FileImages));

	if (fi.Id != 0x46494D47) { return Cleanup(ifs, NarcError::InvalidFileImagesId); }

	filesystem::create_directory(directory);
	filesystem::current_path(directory);

	for (int i = 0; i < fat.FileCount; ++i)
	{
		ifs.seekg(header.ChunkSize + fat.ChunkSize + fnt.ChunkSize + 8 + fatEntries[i].Start);

		unique_ptr<char[]> buffer(new char[fatEntries[i].End - fatEntries[i].Start]);
		ifs.read(buffer.get(), fatEntries[i].End - fatEntries[i].Start);

		ostringstream oss;

		if (fnt.ChunkSize > 0x10) // Temporary until I figure out wtf is going on
		{
			oss << FileNameTableEntries.front().Name;
			FileNameTableEntries.erase(FileNameTableEntries.begin());
		}
		else
		{
			oss << fileName.stem().string() << "_" << setfill('0') << setw(8) << i << ".bin";
		}

		ofstream ofs(oss.str(), ios::binary);

		if (!ofs.good())
		{
			ofs.close();

			return Cleanup(ifs, NarcError::InvalidOutputFile);
		}

		ofs.write(buffer.get(), fatEntries[i].End - fatEntries[i].Start);
		ofs.close();
	}

	ifs.close();

	return error == NarcError::None ? true : false;
}
