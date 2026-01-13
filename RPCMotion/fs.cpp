#include "fs.h"

DWORD ListDirectoryContents(const wchar_t* path, std::wstring& output) {
    std::wstring searchPath;
    if (path == NULL || wcslen(path) == 0) {
        wchar_t currentDir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, currentDir);
        searchPath = std::wstring(currentDir) + L"\\*";
    }
    else {
        searchPath = std::wstring(path);
        if (searchPath.back() != L'\\' && searchPath.back() != L'/') {
            searchPath += L"\\";
        }
        searchPath += L"*";
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }

    output = L"Directory listing:\r\n";

    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            output += L"[DIR]  ";
        }
        else {
            output += L"[FILE] ";
        }

        output += findData.cFileName;
        output += L"\r\n";
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return ERROR_SUCCESS;
}

DWORD UploadFileContent(const wchar_t* remotePath, const byte* fileData, DWORD fileSize) {
    if (remotePath == NULL || fileData == NULL || fileSize == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    HANDLE hFile = CreateFileW(
        remotePath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(hFile, fileData, fileSize, &bytesWritten, NULL)) {
        DWORD error = GetLastError();
        CloseHandle(hFile);
        return error;
    }

    CloseHandle(hFile);
    return ERROR_SUCCESS;
}

DWORD DownloadFileContent(const wchar_t* remotePath, byte** fileData, DWORD* fileSize) {
    if (remotePath == NULL || fileData == NULL || fileSize == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    HANDLE hFile = CreateFileW(
        remotePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }

    LARGE_INTEGER fileSizeLarge;
    if (!GetFileSizeEx(hFile, &fileSizeLarge)) {
        CloseHandle(hFile);
        return GetLastError();
    }

    if (fileSizeLarge.HighPart > 0 || fileSizeLarge.LowPart > 1024 * 1024 * 1024 * 10) {
        CloseHandle(hFile);
        return ERROR_FILE_TOO_LARGE;
    }

    *fileSize = fileSizeLarge.LowPart;
    *fileData = (byte*)midl_user_allocate(*fileSize);

    if (*fileData == NULL) {
        CloseHandle(hFile);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, *fileData, *fileSize, &bytesRead, NULL)) {
        midl_user_free(*fileData);
        CloseHandle(hFile);
        return GetLastError();
    }

    CloseHandle(hFile);
    return ERROR_SUCCESS;
}