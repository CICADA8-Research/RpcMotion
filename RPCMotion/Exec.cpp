#include "Exec.h"

DWORD ExecuteCommand(const wchar_t* command, std::wstring& output, bool silent) {
    if (command == NULL || wcslen(command) == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    DWORD dwExitCode = 0;
    DWORD dwBytesRead = 0;
    const DWORD BUFFER_SIZE = 8192;  
    CHAR chBuffer[BUFFER_SIZE] = { 0 };
    std::string resultBytes;

    if (!silent) {
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            return GetLastError();
        }
    }

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;

    if (!silent) {
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
    }
    else {
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    wchar_t* cmdCopy = _wcsdup(command);
    if (!CreateProcessW(
        NULL,
        cmdCopy,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        NULL,
        NULL,
        &si,
        &pi)) {
        DWORD error = GetLastError();
        free(cmdCopy);
        if (!silent) {
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
        }
        return error;
    }
    free(cmdCopy);

    if (!silent) {
        CloseHandle(hWritePipe);

        while (ReadFile(hReadPipe, chBuffer, BUFFER_SIZE - 1, &dwBytesRead, NULL) && dwBytesRead > 0) {
            resultBytes.append(chBuffer, dwBytesRead);
        }

        CloseHandle(hReadPipe);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &dwExitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (!silent && !resultBytes.empty()) {
        int wideSize = 0;

        wideSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            resultBytes.c_str(), (int)resultBytes.size(),
            NULL, 0);

        if (wideSize > 0) {
            // UTF-8
            output.resize(wideSize);
            MultiByteToWideChar(CP_UTF8, 0,
                resultBytes.c_str(), (int)resultBytes.size(),
                &output[0], wideSize);
        }
        else {
            // OEM
            wideSize = MultiByteToWideChar(CP_OEMCP, 0,
                resultBytes.c_str(), (int)resultBytes.size(),
                NULL, 0);
            if (wideSize > 0) {
                output.resize(wideSize);
                MultiByteToWideChar(CP_OEMCP, 0,
                    resultBytes.c_str(), (int)resultBytes.size(),
                    &output[0], wideSize);
            }
            else {
                //ANSI
                wideSize = MultiByteToWideChar(CP_ACP, 0,
                    resultBytes.c_str(), (int)resultBytes.size(),
                    NULL, 0);
                if (wideSize > 0) {
                    output.resize(wideSize);
                    MultiByteToWideChar(CP_ACP, 0,
                        resultBytes.c_str(), (int)resultBytes.size(),
                        &output[0], wideSize);
                }
                else {
                    output = L"[Error: Could not decode command output]";
                }
            }
        }
    }

    return dwExitCode;
}