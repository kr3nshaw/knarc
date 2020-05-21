#include "Narc.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>

using namespace std;

NarcError Narc::GetError() const
{
	return error;
}

bool Narc::Pack(const filesystem::path& fileName, const filesystem::path& directory)
{
	ofstream ofs(fileName, ios::out | ios::binary);

	FileImages fi;
	FileNameTable fnt;
	FileAllocationTable fat;
	Header header;

	uint32_t displacement_from_zero = 0;
	uint16_t nFiles = 0;
	vector<uint32_t> file_sizes;

	if (!ofs.good())
	{
		error = NarcError::InvalidOutputFile;

		goto Cleanup;
	}

	filesystem::current_path(directory);

	// Build FIMG Section
	fi.Id = 0x46494D47;
	fi.ChunkSize = 0;
	fi.files = new vector<FileImageEntry>;
	for (auto& f : filesystem::directory_iterator(directory))
	{
		ifstream file(f.path(), ios::in | ios::binary);
		cout << f.path();
		fi.ChunkSize += sizeof(file);
		file_sizes.push_back(sizeof(file));
		
		FileImageEntry e;
		// Read file into struct.
		file.read(reinterpret_cast<char*>(&e), sizeof(e));
		fi.files->push_back(e);
		file.close();
		nFiles++;
	}

	// Build FNTB Section
	fnt.Id = 0x464E5442;
	fnt.ChunkSize = 0x10;
	fnt.SubTableOffset = 0x4;
	fnt.FirstFileId = 0x0;
	fnt.DirectoryCount = 0x1;
	// TODO : Generate FNTB for Camelot games.
	
	// Build FATB Section
	fat.Id = 0x46415442;
	fat.Reserved = 0x0;
	fat.FileCount = nFiles;
	fat.Entries = new vector<FileAllocationTableEntry>[fat.FileCount];

	// Determine Chunk Size
	for (std::vector<uint32_t>::size_type i = 0; i != file_sizes.size(); i++) {
		FileAllocationTableEntry e;
		e.Start = displacement_from_zero;
		e.End = displacement_from_zero + file_sizes[i];
		fat.Entries->push_back(e);
		displacement_from_zero += file_sizes[i];
	}

	fat.ChunkSize = displacement_from_zero;

	// Configure NARC
	header.Id = 0x4352414E;
	header.ByteOrderMark = 0xFFFE;
	header.Version = 0x100;
	header.ChunkSize = 0x10;
	header.ChunkCount = 0x3;
	header.FileSize = fnt.ChunkSize + fat.ChunkSize + fi.ChunkSize;

	// Write NARC Header
	ofs.write(reinterpret_cast<char*>(&header.Id), 4);
	ofs.write(reinterpret_cast<char*>(&header.ByteOrderMark), 2);
	ofs.write(reinterpret_cast<char*>(&header.Version), 2);
	ofs.write(reinterpret_cast<char*>(&header.FileSize), 4);
	ofs.write(reinterpret_cast<char*>(&header.ChunkSize), 2);
	ofs.write(reinterpret_cast<char*>(&header.ChunkCount), 2);

	// Write FATB
	ofs.write(reinterpret_cast<char*>(&fat.Id), 4);
	ofs.write(reinterpret_cast<char*>(&fat.ChunkSize), 4);
	ofs.write(reinterpret_cast<char*>(&fat.FileCount), 2);
	ofs.write(reinterpret_cast<char*>(&fat.Reserved), 2);
	for (int i = 0; i < sizeof(fat.Entries); i++)
		ofs.write((char*)&fat.Entries[i], sizeof(fat.Entries[i]));
	ofs.write((char*)&fat.Entries[0], sizeof(fat.Entries));

	// Write FNTB
	ofs.write(reinterpret_cast<char*>(&fnt.Id), 4);
	ofs.write(reinterpret_cast<char*>(&fnt.ChunkSize), 4);
	ofs.write(reinterpret_cast<char*>(&fnt.SubTableOffset), 4);
	ofs.write(reinterpret_cast<char*>(&fnt.FirstFileId), 2);
	ofs.write(reinterpret_cast<char*>(&fnt.DirectoryCount), 2);

	// Write FIMG
	ofs.write(reinterpret_cast<char*>(&fi.Id), 4);
	ofs.write(reinterpret_cast<char*>(&fi.ChunkSize), 4);
	for (std::vector<uint32_t>::size_type i = 0; i != fi.files->size(); i++)
		ofs.write((char*)&fi.files[i], sizeof(fi.files[i]));

	ofs.flush();
	ofs.close();

	Cleanup:
		ofs.close();

	return error == NarcError::None ? true : false;
}

bool Narc::Unpack(const filesystem::path& fileName, const filesystem::path& directory)
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
	ifs.read(reinterpret_cast<char*>(&fat), 12);

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

	fat.Entries = new vector<FileAllocationTableEntry>[fat.FileCount];

	for (int i = 0; i < fat.FileCount; ++i)
	{
		FileAllocationTableEntry e;
		ifs.read(reinterpret_cast<char*>(&e), sizeof(FileAllocationTableEntry));
		fat.Entries[i].push_back(e);
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
		ifs.read(reinterpret_cast<char*>(&fntEntries[i]), sizeof(uint16_t));

	// TODO: Actually read FNT sub-tables

	ifs.seekg(static_cast<uint64_t>(header.ChunkSize) + static_cast<uint64_t>(fat.ChunkSize) + static_cast<uint64_t>(fnt.ChunkSize));

	FileImages fi;
	ifs.read(reinterpret_cast<char*>(&fi), 8);

	if (fi.Id != 0x46494D47)
	{
		error = NarcError::InvalidFileImagesId;

		goto Cleanup;
	}

	filesystem::create_directory(directory);
	filesystem::current_path(directory);

	for (int i = 0; i < fat.FileCount; ++i)
	{
		cout << fat.Entries->at(i).End - fat.Entries->at(i).Start;
		buffer = new char[fat.Entries->at(i).End - fat.Entries->at(i).Start];
		ifs.read(buffer, fat.Entries->at(i).End - fat.Entries->at(i).Start);

		ostringstream oss;
		oss << fileName.stem().string() << "_" << setfill('0') << setw(4) << i << ".bin";
		ofstream ofs(oss.str(), ios::out | ios::binary);
		if (!ofs.good())
		{
			error = NarcError::InvalidOutputFile;

			delete[] buffer;

			goto Cleanup;
		}

		ofs.write(buffer, fat.Entries->at(i).End - fat.Entries->at(i).Start);
		ofs.close();
		delete[] buffer;
	}

	Cleanup:
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