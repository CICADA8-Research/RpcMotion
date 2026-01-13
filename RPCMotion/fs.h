#pragma once
#include <Windows.h>
#include <string>

DWORD ListDirectoryContents(const wchar_t* path, std::wstring& output);
DWORD UploadFileContent(const wchar_t* remotePath, const byte* fileData, DWORD fileSize);
DWORD DownloadFileContent(const wchar_t* remotePath, byte** fileData, DWORD* fileSize);