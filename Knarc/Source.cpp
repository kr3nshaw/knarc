#include <cstring>
#include <iostream>
#include <string>

#include "Narc.h"

using namespace std;

void PrintError(NarcError error)
{
	switch (error)
	{
		case NarcError::None:								cout << "ERROR: No error???" << endl;										break;
		case NarcError::InvalidInputFile:					cout << "ERROR: Invalid input file" << endl;								break;
		case NarcError::InvalidHeaderId:					cout << "ERROR: Invalid header ID" << endl;									break;
		case NarcError::InvalidByteOrderMark:				cout << "ERROR: Invalid byte order mark" << endl;							break;
		case NarcError::InvalidVersion:						cout << "ERROR: Invalid NARC version" << endl;								break;
		case NarcError::InvalidHeaderSize:					cout << "ERROR: Invalid header size" << endl;								break;
		case NarcError::InvalidChunkCount:					cout << "ERROR: Invalid chunk count" << endl;								break;
		case NarcError::InvalidFileAllocationTableId:		cout << "ERROR: Invalid file allocation table ID" << endl;					break;
		case NarcError::InvalidFileAllocationTableReserved:	cout << "ERROR: Invalid file allocation table reserved section" << endl;	break;
		case NarcError::InvalidFileNameTableId:				cout << "ERROR: Invalid file name table ID" << endl;						break;
		case NarcError::InvalidFileImagesId:				cout << "ERROR: Invalid file images ID" << endl;							break;
		case NarcError::InvalidOutputFile:					cout << "ERROR: Invalid output file" << endl;								break;
		default:											cout << "ERROR: Unknown error???" << endl;									break;
	}
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		cout << "OVERVIEW: Knarc" << endl << endl;
		cout << "USAGE: knarc [options] <inputs>" << endl << endl;
		cout << "OPTIONS:" << endl;
		cout << "\t-d\tDirectory to pack from/unpack to" << endl;
		cout << "\t-f\tFile name of archive to pack to/unpack from" << endl;
		cout << "\t-p\tPack" << endl;
		cout << "\t-u\tUnpack" << endl;

		return 1;
	}

	string directory = "";
	string fileName = "";

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-d"))
		{
			if (i == (argc - 1))
			{
				cerr << "ERROR: No directory specified" << endl;

				return 1;
			}

			directory = argv[++i];
		}
		else if (!strcmp(argv[i], "-f"))
		{
			if (i == (argc - 1))
			{
				cerr << "ERROR: No file name specified" << endl;

				return 1;
			}

			fileName = argv[++i];
		}
		else if (!strcmp(argv[i], "-p"))
		{
			if (fileName == "")
			{
				cerr << "ERROR: No NARC specified to pack to" << endl;

				return 1;
			}

			Narc narc;

			if (!narc.Pack(fileName, directory))
			{
				PrintError(narc.GetError());

				return 1;
			}
		}
		else if (!strcmp(argv[i], "-u"))
		{
			if (fileName == "")
			{
				cerr << "ERROR: No NARC specified to unpack from" << endl;

				return 1;
			}

			Narc narc;

			if (!narc.Unpack(fileName, directory))
			{
				PrintError(narc.GetError());

				return 1;
			}
		}
	}

	return 0;
}