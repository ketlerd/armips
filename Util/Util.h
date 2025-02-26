#pragma once
#include <string>
#include <stdio.h>

#include "ByteArray.h"

typedef std::vector<std::wstring> StringList;

std::wstring convertUtf8ToWString(const char* source);
std::string convertWCharToUtf8(wchar_t character);
;std::string convertWStringToUtf8(const std::wstring& source);

std::wstring intToHexString(unsigned int value, int digits, bool prefix = false);
std::wstring intToString(unsigned int value, int digits);

StringList getStringListFromArray(wchar_t** source, int count);

size_t fileSize(const std::wstring& fileName);
bool fileExists(const std::wstring& strFilename);
bool copyFile(const std::wstring& existingFile, const std::wstring& newFile);
bool deleteFile(const std::wstring& fileName);;

std::wstring toWLowercase(const std::string& str);
std::wstring getFileNameFromPath(const std::wstring& path);
size_t replaceAll(std::wstring& str, const wchar_t* oldValue,const std::wstring& newValue);
bool startsWith(const std::wstring& str, wchar_t* value, size_t stringPos = 0);

inline unsigned int swapEndianness32(unsigned int value)
{
	return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
}

inline unsigned short swapEndianness16(unsigned short value)
{
	return ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
}

enum class OpenFileMode { ReadBinary, WriteBinary, ReadWriteBinary };
FILE* openFile(const std::wstring& fileName, OpenFileMode mode);
std::wstring getCurrentDirectory();
void changeDirectory(const std::wstring& dir);


#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
