#pragma once
#include <Windows.h>
#include <string>
#include <iostream>

DWORD ExecuteCommand(const wchar_t* command, std::wstring& output, bool silent = false);