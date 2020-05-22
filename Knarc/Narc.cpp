#include "Narc.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <memory>
#include <regex>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

using namespace std;

void Narc::AlignDword(ofstream& ofs, uint8_t paddingChar)
{
	if ((ofs.tellp() % 4) != 0)
	{
		for (int i = 4 - (ofs.tellp() % 4); i-- > 0; )
		{
			ofs.write(reinterpret_cast<char*>(&paddingChar), sizeof(uint8_t));
		}
	}
}

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
	vector<string> fileNames;

	for (const auto& file : filesystem::directory_iterator(directory))
	{
		fatEntries.push_back(FileAllocationTableEntry
			{
				.Start = 0x0,
				.End = 0x0
			});

		if (fatEntries.size() > 1)
		{
			fatEntries.back().Start = fatEntries.rbegin()[1].End;

			if ((fatEntries.rbegin()[1].End % 4) != 0)
			{
				fatEntries.back().Start += 4 - (fatEntries.rbegin()[1].End % 4);
			}
		}

		fatEntries.back().End = fatEntries.back().Start + static_cast<uint32_t>(file.file_size());

		fileNames.push_back(file.path().filename().string());
	}

	FileAllocationTable fat
	{
		.Id = 0x46415442,
		.ChunkSize = sizeof(FileAllocationTable) + ((uint32_t)fatEntries.size() * sizeof(FileAllocationTableEntry)),
		.FileCount = static_cast<uint16_t>(fatEntries.size()),
		.Reserved = 0x0
	};

	vector<FileNameTableEntry> fntEntries;

	if (!regex_match(filesystem::directory_iterator(directory)->path().string(), regex(".*_\\d{8}\\.bin")))
	{
		// TODO: Actually write FNT sub-tables
		fntEntries.push_back(FileNameTableEntry
			{
				.Offset = 0x8,
				.FirstFileId = 0x0,
				.Utility = 0x1
			});
	}
	else
	{
		fntEntries.push_back(FileNameTableEntry
			{
				.Offset = 0x4,
				.FirstFileId = 0x0,
				.Utility = 0x1
			});
	}

	FileNameTable fnt
	{
		.Id = 0x464E5442,
		.ChunkSize = sizeof(FileNameTable) + (fntEntries.size() * sizeof(FileNameTableEntry))
	};

	if (!regex_match(filesystem::directory_iterator(directory)->path().string(), regex(".*_\\d{8}\\.bin")))
	{
		for (const auto& fn : fileNames)
		{
			fnt.ChunkSize += sizeof(uint8_t) + fn.size();
		}

		fnt.ChunkSize += sizeof(uint8_t);
	}

	if ((fnt.ChunkSize % 4) != 0)
	{
		fnt.ChunkSize += 4 - (fnt.ChunkSize % 4);
	}

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
		.ChunkSize = sizeof(Header),
		.ChunkCount = 0x3
	};

	ofs.write(reinterpret_cast<char*>(&header), sizeof(Header));
	ofs.write(reinterpret_cast<char*>(&fat), sizeof(FileAllocationTable));

	for (auto& entry : fatEntries)
	{
		ofs.write(reinterpret_cast<char*>(&entry), sizeof(FileAllocationTableEntry));
	}

	ofs.write(reinterpret_cast<char*>(&fnt), sizeof(FileNameTable));

	for (auto& entry : fntEntries)
	{
		ofs.write(reinterpret_cast<char*>(&entry), sizeof(FileNameTableEntry));
	}

	if (!regex_match(filesystem::directory_iterator(directory)->path().string(), regex(".*_\\d{8}\\.bin")))
	{
		for (const auto& fn : fileNames)
		{
			uint8_t fnLength = static_cast<uint8_t>(fn.size());

			ofs.write(reinterpret_cast<char*>(&fnLength), sizeof(uint8_t));
			ofs.write(fn.c_str(), fn.size());
		}

		uint8_t zero = 0x00;
		ofs.write(reinterpret_cast<char*>(&zero), sizeof(uint8_t));
	}

	AlignDword(ofs, 0xFF);

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
		unique_ptr<char[]> buffer = make_unique<char[]>(static_cast<unsigned int>(length));

		ifs.seekg(0);
		ifs.read(buffer.get(), length);
		ifs.close();

		ofs.write(buffer.get(), length);

		AlignDword(ofs, 0xFF);
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
	if ((header.Version != 0x0100) && (header.Version != 0x0000)) { return Cleanup(ifs, NarcError::InvalidVersion); }
	if (header.ChunkSize != 0x10) { return Cleanup(ifs, NarcError::InvalidHeaderSize); }
	if (header.ChunkCount != 0x3) { return Cleanup(ifs, NarcError::InvalidChunkCount); }

	FileAllocationTable fat;
	ifs.read(reinterpret_cast<char*>(&fat), sizeof(FileAllocationTable));

	if (fat.Id != 0x46415442) { return Cleanup(ifs, NarcError::InvalidFileAllocationTableId); }
	if (fat.Reserved != 0x0) { return Cleanup(ifs, NarcError::InvalidFileAllocationTableReserved); }

	unique_ptr<FileAllocationTableEntry[]> fatEntries = make_unique<FileAllocationTableEntry[]>(fat.FileCount);

	for (int i = 0; i < fat.FileCount; ++i)
	{
		ifs.read(reinterpret_cast<char*>(&fatEntries[i]), sizeof(FileAllocationTableEntry));
	}

	FileNameTable fnt;
	vector<FileNameTableEntry> FileNameTableEntries;
	ifs.read(reinterpret_cast<char*>(&fnt), sizeof(FileNameTable));

	if (fnt.Id != 0x464E5442) { return Cleanup(ifs, NarcError::InvalidFileNameTableId); }

	vector<FileNameTableEntry> fntEntries;

	do
	{
		fntEntries.push_back(FileNameTableEntry());

		ifs.read(reinterpret_cast<char*>(&fntEntries.back().Offset), sizeof(uint32_t));
		ifs.read(reinterpret_cast<char*>(&fntEntries.back().FirstFileId), sizeof(uint16_t));
		ifs.read(reinterpret_cast<char*>(&fntEntries.back().Utility), sizeof(uint16_t));
	} while (static_cast<uint32_t>(ifs.tellg()) < (header.ChunkSize + fat.ChunkSize + sizeof(FileNameTable) + fntEntries[0].Offset));

	unique_ptr<string[]> fileNames = make_unique<string[]>(0xFFFF);

	for (size_t i = 0; i < fntEntries.size(); ++i)
	{
		ifs.seekg(static_cast<uint64_t>(header.ChunkSize) + fat.ChunkSize + sizeof(FileNameTable) + fntEntries[i].Offset);

		uint16_t fileId = 0x0000;

		for (uint8_t length = 0x80; length != 0x00; ifs.read(reinterpret_cast<char*>(&length), sizeof(uint8_t)))
		{
			if (length <= 0x7F)
			{
				for (int j = 0; j < length; ++j)
				{
					uint8_t c;
					ifs.read(reinterpret_cast<char*>(&c), sizeof(uint8_t));

					fileNames.get()[fntEntries[i].FirstFileId + fileId] += c;
				}

				++fileId;
			}
			else if (length == 0x80)
			{
				// Reserved
			}
			else if (length <= 0xFF)
			{
				length -= 0x80;
				string directoryName;

				for (int j = 0; j < length; ++j)
				{
					uint8_t c;
					ifs.read(reinterpret_cast<char*>(&c), sizeof(uint8_t));

					directoryName += c;
				}

				uint16_t directoryId;
				ifs.read(reinterpret_cast<char*>(&directoryId), sizeof(uint16_t));

				fileNames.get()[directoryId] = directoryName;
			}
			else
			{
				return Cleanup(ifs, NarcError::InvalidFileNameTableEntryId);
			}
		}
	}

	if ((ifs.tellg() % 4) != 0)
	{
		ifs.seekg(4 - (ifs.tellg() % 4), ios::cur);
	}

	FileImages fi;
	ifs.read(reinterpret_cast<char*>(&fi), sizeof(FileImages));

	if (fi.Id != 0x46494D47) { return Cleanup(ifs, NarcError::InvalidFileImagesId); }

	filesystem::create_directory(directory);
	filesystem::current_path(directory);

	if (fnt.ChunkSize == 0x10)
	{
		for (int i = 0; i < fat.FileCount; ++i)
		{
			ifs.seekg(static_cast<uint64_t>(header.ChunkSize) + fat.ChunkSize + fnt.ChunkSize + 8 + fatEntries[i].Start);

			unique_ptr<char[]> buffer = make_unique<char[]>(fatEntries[i].End - fatEntries[i].Start);
			ifs.read(buffer.get(), fatEntries[i].End - fatEntries[i].Start);

			ostringstream oss;
			oss << fileName.stem().string() << "_" << setfill('0') << setw(8) << i << ".bin";

			ofstream ofs(oss.str(), ios::binary);

			if (!ofs.good())
			{
				ofs.close();

				return Cleanup(ifs, NarcError::InvalidOutputFile);
			}

			ofs.write(buffer.get(), fatEntries[i].End - fatEntries[i].Start);
			ofs.close();
		}
	}
	else
	{
		filesystem::path absolutePath = filesystem::absolute(filesystem::current_path());

		for (size_t i = 0; i < fntEntries.size(); ++i)
		{
			filesystem::current_path(absolutePath);
			stack<string> directories;

			for (uint16_t j = fntEntries[i].Utility; j > 0xF000; j = fntEntries[j - 0xF000].Utility)
			{
				directories.push(fileNames.get()[j]);
			}

			for (; !directories.empty(); directories.pop())
			{
				filesystem::create_directory(directories.top());
				filesystem::current_path(directories.top());
			}

			if (fntEntries[i].Utility >= 0xF000)
			{
				filesystem::create_directory(fileNames.get()[0xF000 + i]);
				filesystem::current_path(fileNames.get()[0xF000 + i]);
			}

			ifs.seekg(static_cast<uint64_t>(header.ChunkSize) + fat.ChunkSize + sizeof(FileNameTable) + fntEntries[i].Offset);

			uint16_t fileId = 0x0000;

			for (uint8_t length = 0x80; length != 0x00; ifs.read(reinterpret_cast<char*>(&length), sizeof(uint8_t)))
			{
				if (length <= 0x7F)
				{
					streampos savedPosition = ifs.tellg();

					ifs.seekg(static_cast<uint64_t>(header.ChunkSize) + fat.ChunkSize + fnt.ChunkSize + 8 + fatEntries[fntEntries[i].FirstFileId + fileId].Start);

					unique_ptr<char[]> buffer = make_unique<char[]>(fatEntries[i].End - fatEntries[i].Start);
					ifs.read(buffer.get(), fatEntries[i].End - fatEntries[i].Start);

					ofstream ofs(fileNames.get()[fntEntries[i].FirstFileId + fileId++], ios::binary);

					if (!ofs.good())
					{
						ofs.close();

						return Cleanup(ifs, NarcError::InvalidOutputFile);
					}

					ofs.write(buffer.get(), fatEntries[i].End - fatEntries[i].Start);
					ofs.close();

					ifs.seekg(savedPosition);
					ifs.seekg(length, ios::cur);
				}
				else if (length == 0x80)
				{
					// Reserved
				}
				else if (length <= 0xFF)
				{
					ifs.seekg(static_cast<uint64_t>(length) - 0x80 + 0x2, ios::cur);
				}
				else
				{
					return Cleanup(ifs, NarcError::InvalidFileNameTableEntryId);
				}
			}
		}
	}

	ifs.close();

	return error == NarcError::None ? true : false;
}
