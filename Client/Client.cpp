#include <Windows.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <locale>
#include <fstream>
#include <vector>
#include "..\RPCMotion\MyInterface_h.h"
#pragma comment(lib, "rpcrt4.lib")

wchar_t* getCmdOption(wchar_t** begin, wchar_t** end, const std::wstring& option) {
    wchar_t** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end) {
        return *itr;
    }
    return nullptr;
}

bool cmdOptionExists(wchar_t** begin, wchar_t** end, const std::wstring& option) {
    return std::find(begin, end, option) != end;
}

void PrintHelp() {
    std::wcout << L"RPC Client Usage:\n"
        << L"  --host <ip>        Server IP address (default: 127.0.0.1)\n"
        << L"  --port <port>      Server port (default: 12345)\n"
        << L"  --command <cmd>    Command to execute on server\n"
        << L"  --shutdown         Shutdown the server\n"
        << L"  --interactive      Start interactive shell\n"
        << L"  --help             Show this help message\n";
}

class RpcClient {
private:
    handle_t hBinding;
    std::wstring host;
    std::wstring port;
    bool connected;

public:
    RpcClient() : hBinding(NULL), connected(false) {}

    ~RpcClient() {
        Disconnect();
    }

    RPC_STATUS Connect(const std::wstring& serverHost, const std::wstring& serverPort) {
        if (connected) Disconnect();

        host = serverHost;
        port = serverPort;

        RPC_STATUS rpcStatus;
        RPC_WSTR szStringBinding = NULL;

        rpcStatus = RpcStringBindingCompose(
            NULL,
            (RPC_WSTR)L"ncacn_ip_tcp",
            (RPC_WSTR)host.c_str(),
            (RPC_WSTR)port.c_str(),
            NULL,
            &szStringBinding);

        if (rpcStatus != RPC_S_OK) {
            return rpcStatus;
        }

        rpcStatus = RpcBindingFromStringBinding(szStringBinding, &hBinding);
        RpcStringFree(&szStringBinding);

        if (rpcStatus != RPC_S_OK) {
            return rpcStatus;
        }

        rpcStatus = RpcBindingSetAuthInfo(
            hBinding,
            NULL,
            RPC_C_AUTHN_LEVEL_NONE,
            RPC_C_AUTHN_NONE,
            NULL,
            RPC_C_AUTHZ_NONE);

        if (rpcStatus == RPC_S_OK) {
            wchar_t* pingResponse = NULL;
            DWORD pingResult = 0;
            RPC_STATUS pingRpcStatus = RPC_S_OK;

            RpcTryExcept
            {
                pingResult = ::Ping(hBinding, &pingResponse);

                if (pingResponse) {
                    if (pingResult == ERROR_SUCCESS && wcscmp(pingResponse, L"PONG_OK") == 0) {
                        connected = true;
                        std::wcout << L"[+] Server responded to ping: " << pingResponse << std::endl;
                    }
                    else {
                        std::wcout << L"[-] Invalid ping response. Code: " << pingResult
                                   << L", Response: " << pingResponse << std::endl;
                        rpcStatus = RPC_S_SERVER_UNAVAILABLE;
                    }

                }
                else {
                    std::wcout << L"[-] No response from server. Error: " << pingResult << std::endl;
                    rpcStatus = RPC_S_SERVER_UNAVAILABLE;
                }
            }
                RpcExcept(1)
            {
                pingRpcStatus = RpcExceptionCode();
                std::wcout << L"[-] RPC ping failed: " << pingRpcStatus << std::endl;
                rpcStatus = pingRpcStatus;
            }
            RpcEndExcept
        }

        if (!connected && rpcStatus == RPC_S_OK) {
            RpcBindingFree(&hBinding);
            hBinding = NULL;
        }

        return rpcStatus;
    }

    void Disconnect() {
        if (hBinding) {
            RpcBindingFree(&hBinding);
            hBinding = NULL;
        }
        connected = false;
    }

    bool IsConnected() const {
        return connected;
    }

    RPC_STATUS Reconnect() {
        Disconnect();
        return Connect(host, port);
    }

    DWORD Execute(const std::wstring& command, std::wstring& output) {
        if (!connected) return RPC_S_INVALID_BINDING;

        DWORD exitCode = 0;
        wchar_t* result = NULL;

        RpcTryExcept{
            exitCode = ::Execute(hBinding, const_cast<wchar_t*>(command.c_str()), &result);
            if (result) {
                output = result;
            }
        }
        RpcExcept(1) {
            return RpcExceptionCode();
        }
        RpcEndExcept

        return exitCode;
    }

    DWORD ExecuteSilent(const std::wstring& command) {
        if (!connected) return RPC_S_INVALID_BINDING;

        DWORD exitCode = 0;

        RpcTryExcept{
            exitCode = ::ExecuteSilent(hBinding, const_cast<wchar_t*>(command.c_str()));
        }
        RpcExcept(1) {
            return RpcExceptionCode();
        }
        RpcEndExcept

        return exitCode;
    }

    DWORD UploadFile(const std::wstring& localPath, const std::wstring& remotePath) {
        if (!connected) return RPC_S_INVALID_BINDING;

        HANDLE hFile = CreateFileW(
            localPath.c_str(),
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
            DWORD error = GetLastError();
            CloseHandle(hFile);
            return error;
        }

        if (fileSizeLarge.HighPart > 0 || fileSizeLarge.LowPart > 1024 * 1024 * 1024 * 10) {
            CloseHandle(hFile);
            return ERROR_FILE_TOO_LARGE;
        }

        DWORD fileSize = fileSizeLarge.LowPart;
        byte* buffer = (byte*)malloc(fileSize);
        if (!buffer) {
            CloseHandle(hFile);
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        DWORD bytesRead = 0;
        BOOL readResult = ReadFile(hFile, buffer, fileSize, &bytesRead, NULL);
        CloseHandle(hFile);

        if (!readResult) {
            free(buffer);
            return GetLastError();
        }

        DWORD result = 0;
        RPC_STATUS rpcStatus = RPC_S_OK;

        RpcTryExcept
        {
            result = ::UploadFile(hBinding,
                const_cast<wchar_t*>(remotePath.c_str()),
                fileSize,
                buffer);
        }
        RpcExcept(1)
        {
            rpcStatus = RpcExceptionCode();
        }
        RpcEndExcept

        free(buffer);

        if (rpcStatus != RPC_S_OK) {
            return rpcStatus;
        }

        return result;
    }

    DWORD DownloadFile(const std::wstring& remotePath, const std::wstring& localPath) {
        if (!connected) return RPC_S_INVALID_BINDING;

        byte* fileData = NULL;
        DWORD fileSize = 0;
        DWORD result = 0;
        RPC_STATUS rpcStatus = RPC_S_OK;

        RpcTryExcept
        {
            result = ::DownloadFile(hBinding,
                const_cast<wchar_t*>(remotePath.c_str()),
                &fileSize,
                &fileData);
        }
        RpcExcept(1)
        {
            rpcStatus = RpcExceptionCode();
        }
        RpcEndExcept

            if (rpcStatus != RPC_S_OK) {
                return rpcStatus;
            }

        if (result != ERROR_SUCCESS) {
            return result;
        }

        HANDLE hFile = CreateFileW(
            localPath.c_str(),
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (fileData) {
                midl_user_free(fileData);
            }
            return error;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, fileData, fileSize, &bytesWritten, NULL)) {
            DWORD error = GetLastError();
            CloseHandle(hFile);
            if (fileData) {
                midl_user_free(fileData);
            }
            return error;
        }

        CloseHandle(hFile);
        if (fileData) {
            midl_user_free(fileData);
        }

        return ERROR_SUCCESS;
    }

    DWORD ListDirectory(const std::wstring& path, std::wstring& output) {
        if (!connected) return RPC_S_INVALID_BINDING;

        wchar_t* result = NULL;
        DWORD status = 0;

        RpcTryExcept{
            status = ::ListDirectory(hBinding,
                path.empty() ? NULL : const_cast<wchar_t*>(path.c_str()),
                &result);

            if (result) {
                output = result;
            }
        }
        RpcExcept(1) {
            return RpcExceptionCode();
        }
        RpcEndExcept

        return status;
    }

    DWORD ShutdownServer() {
        if (!connected) return RPC_S_INVALID_BINDING;

        RPC_STATUS rpcStatus = RPC_S_OK;

        RpcTryExcept{
            ::ShutDown(hBinding);
        }
        RpcExcept(1) {
            rpcStatus = RpcExceptionCode();
        }
        RpcEndExcept

            connected = false;
        hBinding = NULL;

        return rpcStatus;
    }

    DWORD PingServer(std::wstring& response) {
        if (!connected) return RPC_S_INVALID_BINDING;

        wchar_t* pingResponse = NULL;
        DWORD result = 0;
        RPC_STATUS rpcStatus = RPC_S_OK;

        RpcTryExcept
        { 
            result = ::Ping(hBinding, &pingResponse);

            if (pingResponse) {
                response = pingResponse;
            }
        }
        RpcExcept(1)
        {
            rpcStatus = RpcExceptionCode();
            if (rpcStatus == RPC_S_SERVER_UNAVAILABLE ||
                rpcStatus == RPC_S_SERVER_TOO_BUSY) {
                connected = false;
                hBinding = NULL;
            }
        }
        RpcEndExcept

            return (rpcStatus == RPC_S_OK) ? result : rpcStatus;
    }
};

void InteractiveShell(RpcClient& client) {
    std::wcout << L"Interactive RPC Shell (type 'help' for commands, 'exit' to quit)\n";

    while (true) {
        std::wcout << L"\nRPC> ";

        std::wstring input;
        std::getline(std::wcin, input);

        if (input.empty()) continue;

        if (input == L"exit" || input == L"quit") {
            break;
        }
        else if (input == L"help") {
            std::wcout << L"Available commands:\n"
                << L"  help                    Show this help\n"
                << L"  exit, quit             Exit shell\n"
                << L"  connect <host> <port>  Connect to server\n"
                << L"  disconnect             Disconnect from server\n"
                << L"  reconnect              Reconnect to last server\n"
                << L"  exec <command>         Execute command with output\n"
                << L"  silent <command>       Execute command without output\n"
                << L"  upload <local> <remote> Upload file to server\n"
                << L"  download <remote> <local> Download file from server\n"
                << L"  ls [path]              List directory\n"
                << L"  shutdown               Shutdown server\n"
                << L"  status                 Show connection status\n"
                << L"  ping                   Ping server\n";
        }
        else if (input.substr(0, 7) == L"connect") {
            std::wstring host = L"127.0.0.1";
            std::wstring port = L"12345";

            size_t pos = input.find(L' ', 7);
            if (pos != std::wstring::npos) {
                size_t portPos = input.find(L' ', pos + 1);
                if (portPos != std::wstring::npos) {
                    host = input.substr(pos + 1, portPos - pos - 1);
                    port = input.substr(portPos + 1);
                }
                else {
                    host = input.substr(pos + 1);
                }
            }

            RPC_STATUS status = client.Connect(host, port);
            if (status == RPC_S_OK) {
                std::wcout << L"[+] Connected to " << host << L":" << port << std::endl;
            }
            else {
                std::wcout << L"[-] Connection failed: " << status << std::endl;
            }
        }
        else if (input == L"disconnect") {
            client.Disconnect();
            std::wcout << L"[+] Disconnected" << std::endl;
        }
        else if (input == L"reconnect") {
            RPC_STATUS status = client.Reconnect();
            if (status == RPC_S_OK) {
                std::wcout << L"[+] Reconnected" << std::endl;
            }
            else {
                std::wcout << L"[-] Reconnection failed: " << status << std::endl;
            }
        }
        else if (input.substr(0, 4) == L"exec") {
            if (!client.IsConnected()) {
                std::wcout << L"[-] Not connected" << std::endl;
                continue;
            }

            std::wstring command = input.substr(5);
            std::wstring output;
            DWORD result = client.Execute(command, output);

            if (result == ERROR_SUCCESS) {
                std::wcout << output.c_str() << std::endl;
            }
            else {
                std::wcout << L"[-] Command failed: " << result << std::endl;
            }
        }
        else if (input.substr(0, 6) == L"silent") {
            if (!client.IsConnected()) {
                std::wcout << L"[-] Not connected" << std::endl;
                continue;
            }

            std::wstring command = input.substr(7);
            DWORD result = client.ExecuteSilent(command);

            if (result == ERROR_SUCCESS) {
                std::wcout << L"[+] Command executed" << std::endl;
            }
            else {
                std::wcout << L"[-] Command failed: " << result << std::endl;
            }
        }
        else if (input.substr(0, 6) == L"upload") {
            if (!client.IsConnected()) {
                std::wcout << L"[-] Not connected" << std::endl;
                continue;
            }

            size_t pos1 = input.find(L' ', 6);
            size_t pos2 = input.find(L' ', pos1 + 1);

            if (pos1 == std::wstring::npos || pos2 == std::wstring::npos) {
                std::wcout << L"Usage: upload <local_path> <remote_path>" << std::endl;
                continue;
            }

            std::wstring localPath = input.substr(pos1 + 1, pos2 - pos1 - 1);
            std::wstring remotePath = input.substr(pos2 + 1);

            DWORD result = client.UploadFile(localPath, remotePath);

            if (result == ERROR_SUCCESS) {
                std::wcout << L"[+] File uploaded" << std::endl;
            }
            else {
                std::wcout << L"[-] Upload failed: " << result << std::endl;
            }
        }
        else if (input.substr(0, 8) == L"download") {
            if (!client.IsConnected()) {
                std::wcout << L"[-] Not connected" << std::endl;
                continue;
            }

            size_t pos1 = input.find(L' ', 8);
            size_t pos2 = input.find(L' ', pos1 + 1);

            if (pos1 == std::wstring::npos || pos2 == std::wstring::npos) {
                std::wcout << L"Usage: download <remote_path> <local_path>" << std::endl;
                continue;
            }

            std::wstring remotePath = input.substr(pos1 + 1, pos2 - pos1 - 1);
            std::wstring localPath = input.substr(pos2 + 1);

            DWORD result = client.DownloadFile(remotePath, localPath);

            if (result == ERROR_SUCCESS) {
                std::wcout << L"[+] File downloaded" << std::endl;
            }
            else {
                std::wcout << L"[-] Download failed: " << result << std::endl;
            }
        }
        else if (input.substr(0, 2) == L"ls") {
            if (!client.IsConnected()) {
                std::wcout << L"[-] Not connected" << std::endl;
                continue;
            }

            std::wstring path;
            if (input.length() > 3) {
                path = input.substr(3);
            }

            std::wstring output;
            DWORD result = client.ListDirectory(path, output);

            if (result == ERROR_SUCCESS) {
                std::wcout << output << std::endl;
            }
            else {
                std::wcout << L"[-] List failed: " << result << std::endl;
            }
        }
        else if (input == L"shutdown") {
            if (!client.IsConnected()) {
                std::wcout << L"[-] Not connected" << std::endl;
                continue;
            }

            DWORD result = client.ShutdownServer();
            if (result == RPC_S_OK) {
                std::wcout << L"[+] Server shutdown command sent" << std::endl;
            }
            else {
                std::wcout << L"[-] Shutdown failed: " << result << std::endl;
            }
        }
        else if ((input == L"status") or (input == L"ping")) {
            if (client.IsConnected()) {
                std::wstring response;
                DWORD result = client.PingServer(response);

                if (result == ERROR_SUCCESS) {
                    std::wcout << L"[+] Server response: " << response << std::endl;
                }
                else {
                    std::wcout << L"[-] Ping failed: " << result << std::endl;
                }

                std::wcout << L"[+] Connected" << std::endl;
            }
            else {
                std::wcout << L"[-] Not connected" << std::endl;
            }
        }
        else {
            std::wcout << L"[-] Unknown command. Type 'help' for available commands." << std::endl;
        }
    }
}

int wmain(int argc, wchar_t* argv[]) {
    setlocale(LC_ALL, "");

    if (cmdOptionExists(argv, argv + argc, L"--help") || argc == 1) {
        PrintHelp();
        return 0;
    }

    if (cmdOptionExists(argv, argv + argc, L"--interactive")) {
        wchar_t* host = getCmdOption(argv, argv + argc, L"--host");
        wchar_t* port = getCmdOption(argv, argv + argc, L"--port");

        RpcClient client;

        if (host || port) {
            std::wstring serverHost = host ? host : L"127.0.0.1";
            std::wstring serverPort = port ? port : L"12345";

            RPC_STATUS status = client.Connect(serverHost, serverPort);
            if (status != RPC_S_OK) {
                std::wcout << L"[-] Initial connection failed: " << status << std::endl;
            }
        }

        InteractiveShell(client);
        return 0;
    }

    wchar_t* host = getCmdOption(argv, argv + argc, L"--host");
    wchar_t* port = getCmdOption(argv, argv + argc, L"--port");
    wchar_t* command = getCmdOption(argv, argv + argc, L"--command");
    bool shutdown = cmdOptionExists(argv, argv + argc, L"--shutdown");

    if (!command && !shutdown) {
        std::wcerr << L"Error: You must specify either --command or --shutdown\n";
        PrintHelp();
        return 1;
    }

    if (!host) host = (wchar_t*)L"127.0.0.1";
    if (!port) port = (wchar_t*)L"12345";

    RpcClient client;
    RPC_STATUS status = client.Connect(host, port);

    if (status != RPC_S_OK) {
        std::wcerr << L"[-] Connection failed: " << status << std::endl;
        return status;
    }

    if (shutdown) {
        status = client.ShutdownServer();
        if (status == RPC_S_OK) {
            std::wcout << L"[+] Server shutdown command sent" << std::endl;
        }
        else {
            std::wcerr << L"[-] Shutdown failed: " << status << std::endl;
        }
    }
    else if (command) {
        std::wstring output;
        DWORD result = client.Execute(command, output);

        if (result == ERROR_SUCCESS) {
            std::wcout << output << std::endl;
        }
        else {
            std::wcerr << L"[-] Command failed: " << result << std::endl;
        }
    }

    return 0;
}

void* midl_user_allocate(size_t len) {
    return malloc(len);
}

void midl_user_free(void* ptr) {
    free(ptr);
}