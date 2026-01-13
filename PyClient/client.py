import argparse
import struct
from io import BytesIO
from impacket.dcerpc.v5 import transport, ndr
from impacket.dcerpc.v5.dtypes import WSTR, DWORD, ULONG, LPWSTR, WCHAR, LPBYTE
from impacket.dcerpc.v5.ndr import NDRCALL, NDRPOINTER, NDRUniConformantArray, NDRSTRUCT
from impacket.dcerpc.v5.rpcrt import RPC_C_AUTHN_LEVEL_NONE, DCERPCException
from impacket.uuid import uuidtup_to_bin

MY_INTERFACE_UUID = uuidtup_to_bin(('12345778-1234-ABCD-EF00-0123456789AC', '1.0'))

OPNUM_PING = 0
OPNUM_EXECUTE = 1
OPNUM_EXECUTE_SILENT = 2
OPNUM_UPLOAD_FILE = 3
OPNUM_DOWNLOAD_FILE = 4
OPNUM_LIST_DIRECTORY = 5
OPNUM_SHUTDOWN = 6


class BYTE_ARRAY(NDRUniConformantArray):
    item = 'c'

class PBYTE_ARRAY(NDRPOINTER):
    referent = (
        ('Data', BYTE_ARRAY),
    )

class NDRPingRequest(NDRCALL):
    opnum = OPNUM_PING
    structure = (
    )

class NDRPingResponse(NDRCALL):
    structure = (
        ('response', LPWSTR),
        ('Return', DWORD),
    )

class NDRExecuteRequest(NDRCALL):
    opnum = OPNUM_EXECUTE
    structure = (
        ('input', WSTR),
    )

class NDRExecuteResponse(NDRCALL):
    structure = (
        ('output', LPWSTR),
        ('Return', DWORD),
    )

class NDRExecuteSilentRequest(NDRCALL):
    opnum = OPNUM_EXECUTE_SILENT
    structure = (
        ('input', WSTR),
    )

class NDRExecuteSilentResponse(NDRCALL):
    structure = (
        ('Return', DWORD),
    )

class NDRUploadFileRequest(NDRCALL):
    opnum = OPNUM_UPLOAD_FILE
    structure = (
        ('remotePath', WSTR),
        ('fileSize', DWORD),
        ('fileData', BYTE_ARRAY),
    )
    
    def __init__(self, data=None, isNDR64=False):
        NDRCALL.__init__(self, data, isNDR64)
        self['fileData'] = BYTE_ARRAY(isNDR64=isNDR64)

class NDRUploadFileRequest2(NDRCALL):
    opnum = OPNUM_UPLOAD_FILE
    structure = (
        ('remotePath', WSTR),
        ('fileSize', DWORD),
        ('fileData', ':'),
    )
    
    def __init__(self, data=None, isNDR64=False):
        NDRCALL.__init__(self, data, isNDR64)
        
    def dump(self, msg=None, indent=0):
        if msg is None:
            msg = self.__class__.__name__
        return "%s" % msg

class NDRUploadFileResponse(NDRCALL):
    structure = (
        ('Return', DWORD),
    )

class NDRDownloadFileRequest(NDRCALL):
    opnum = OPNUM_DOWNLOAD_FILE
    structure = (
        ('remotePath', WSTR),
    )

class NDRDownloadFileResponse(NDRCALL):
    structure = (
        ('fileSize', DWORD),
        ('fileData', LPBYTE),  
        ('Return', DWORD),
    )

class NDRListDirectoryRequest(NDRCALL):
    opnum = OPNUM_LIST_DIRECTORY
    structure = (
        ('path', WSTR),
    )

class NDRListDirectoryResponse(NDRCALL):
    structure = (
        ('output', LPWSTR),
        ('Return', DWORD),
    )

class NDRShutdownRequest(NDRCALL):
    opnum = OPNUM_SHUTDOWN
    structure = (
    )

class NDRShutdownResponse(NDRCALL):
    structure = ()

class MyRpcInterfaceClient:
    def __init__(self, binding):
        self.binding = binding
        self.dce = None
        self.connected = False
        
    def connect(self, host, port):
        string_binding = f"ncacn_ip_tcp:{host}[{port}]"
        trans = transport.DCERPCTransportFactory(string_binding)
        self.dce = trans.get_dce_rpc()
        self.dce.set_auth_level(RPC_C_AUTHN_LEVEL_NONE)
        self.dce.connect()
        self.dce.bind(MY_INTERFACE_UUID)
        self.connected = True
        return True
        
    def disconnect(self):
        if self.dce:
            self.dce.disconnect()
            self.dce = None
        self.connected = False
        
    def ping(self):
        if not self.connected:
            raise Exception("Not connected")
            
        request = NDRPingRequest()
        try:
            self.dce.call(OPNUM_PING, request)
            response = self.dce.recv()
            resp = NDRPingResponse(response)
            output = resp['response'] if resp['response'] else None
            return resp['Return'], output
        except DCERPCException as e:
            raise
            
    def execute(self, command):
        if not self.connected:
            raise Exception("Not connected")
            
        request = NDRExecuteRequest()
        request['input'] = command + '\x00'
        
        try:
            self.dce.call(OPNUM_EXECUTE, request)
            
            response = self.dce.recv()
            resp = NDRExecuteResponse(response)
            output = resp['output'] if resp['output'] else ""
            return resp['Return'], output
        except DCERPCException as e:
            raise
            
    def execute_silent(self, command):
        if not self.connected:
            raise Exception("Not connected")
            
        request = NDRExecuteSilentRequest()
        request['input'] = command + '\x00'
        
        try:
            self.dce.call(OPNUM_EXECUTE_SILENT, request)
            response = self.dce.recv()
            resp = NDRExecuteSilentResponse(response)
            return resp['Return']
        except DCERPCException as e:
            raise
            
    def upload_file(self, local_path, remote_path):
        if not self.connected:
            raise Exception("Not connected")
            
        with open(local_path, 'rb') as f:
            file_data = f.read()
        
        request = NDRUploadFileRequest()
        request['remotePath'] = remote_path + '\x00'
        file_size = len(file_data)
        request['fileSize'] = file_size

        byte_array = BYTE_ARRAY()

        byte_list = []
        for i in range(file_size):
            byte_list.append(bytes([file_data[i]]))
        
        byte_array['Data'] = byte_list
        
        request['fileData'] = byte_array
        
        try:
            self.dce.call(OPNUM_UPLOAD_FILE, request)
            response = self.dce.recv()
            resp = NDRUploadFileResponse(response)
            return resp['Return']
        except DCERPCException as e:
            raise Exception(f"Upload failed: {e}")
            
    def download_file(self, remote_path, local_path):
        if not self.connected:
            raise Exception("Not connected")
            
        request = NDRDownloadFileRequest()
        request['remotePath'] = remote_path + '\x00'
        
        try:
            self.dce.call(OPNUM_DOWNLOAD_FILE, request)
            response = self.dce.recv()
            resp = NDRDownloadFileResponse(response)
            
            if resp['Return'] == 0:
                
                file_data_ptr = resp['fileData']
                if file_data_ptr:
                    file_size = resp['fileSize']
                    file_data = b''.join(file_data_ptr[:file_size])
                    
                    with open(local_path, 'wb') as f:
                        f.write(file_data)
                else:
                    raise Exception("No file data received. Is file empty?")
                    
            return resp['Return']
        except DCERPCException as e:
            raise
            
    def list_directory(self, path=""):
        if not self.connected:
            raise Exception("Not connected")
            
        request = NDRListDirectoryRequest()
        request['path'] = path + '\x00'
        
        try:
            self.dce.call(OPNUM_LIST_DIRECTORY, request)
            response = self.dce.recv()
            resp = NDRListDirectoryResponse(response)
            output = resp['output'] if resp['output'] else ""
            return resp['Return'], output
        except DCERPCException as e:
            raise
            
    def shutdown(self):
        if not self.connected:
            raise Exception("Not connected")
            
        request = NDRShutdownRequest()
        try:
            self.dce.call(OPNUM_SHUTDOWN, request)
            response = self.dce.recv()
            self.connected = False
            return 0
        except DCERPCException as e:
            raise

def interactive_shell(client):
    print("Interactive RPC Shell (type 'help' for commands, 'exit' to quit)")
    
    while True:
        try:
            cmd = input("\nRPC> ").strip()
            if not cmd:
                continue
                
            if cmd.lower() in ['exit', 'quit']:
                break
                
            elif cmd.lower() == 'help':
                print("Available commands:")
                print("  help                    Show this help")
                print("  exit, quit             Exit shell")
                print("  connect <host> <port>  Connect to server")
                print("  disconnect             Disconnect from server")
                print("  exec <command>         Execute command with output")
                print("  silent <command>       Execute command without output")
                print("  upload <local> <remote> Upload file to server")
                print("  download <remote> <local> Download file from server")
                print("  ls [path]              List directory")
                print("  shutdown               Shutdown server")
                print("  ping                   Ping server")
                print("  status                 Show connection status")
                
            elif cmd.lower().startswith('connect '):
                parts = cmd.split()
                if len(parts) >= 3:
                    host = parts[1]
                    port = parts[2]
                    try:
                        client.connect(host, port)
                        print(f"[+] Connected to {host}:{port}")
                    except Exception as e:
                        print(f"[-] Connection failed: {e}")
                else:
                    print("Usage: connect <host> <port>")
                    
            elif cmd.lower() == 'disconnect':
                client.disconnect()
                print("[+] Disconnected")
                
            elif cmd.lower().startswith('exec '):
                if not client.connected:
                    print("[-] Not connected")
                    continue
                    
                command = cmd[5:]
                try:
                    exit_code, output = client.execute(command)
                    if exit_code == 0:
                        print(f"[+] Command output:\n{output}")
                    else:
                        print(f"[-] Command failed with code: {exit_code}")
                except Exception as e:
                    print(f"[-] Execute failed: {e}")
                    
            elif cmd.lower().startswith('silent '):
                if not client.connected:
                    print("[-] Not connected")
                    continue
                    
                command = cmd[7:]
                try:
                    exit_code = client.execute_silent(command)
                    if exit_code == 0:
                        print("[+] Command executed")
                    else:
                        print(f"[-] Command failed with code: {exit_code}")
                except Exception as e:
                    print(f"[-] Execute silent failed: {e}")
                    
            elif cmd.lower().startswith('upload '):
                if not client.connected:
                    print("[-] Not connected")
                    continue
                    
                parts = cmd.split()
                if len(parts) == 3:
                    local_path = parts[1]
                    remote_path = parts[2]
                    try:
                        result = client.upload_file(local_path, remote_path)
                        if result == 0:
                            print("[+] File uploaded")
                        else:
                            print(f"[-] Upload failed with code: {result}")
                    except Exception as e:
                        print(f"[-] Upload failed: {e}")
                else:
                    print("Usage: upload <local_path> <remote_path>")
                    
            elif cmd.lower().startswith('download '):
                if not client.connected:
                    print("[-] Not connected")
                    continue
                    
                parts = cmd.split()
                if len(parts) == 3:
                    remote_path = parts[1]
                    local_path = parts[2]
                    try:
                        result = client.download_file(remote_path, local_path)
                        if result == 0:
                            print("[+] File downloaded")
                        else:
                            print(f"[-] Download failed with code: {result}")
                    except Exception as e:
                        print(f"[-] Download failed: {e}")
                else:
                    print("Usage: download <remote_path> <local_path>")
                    
            elif cmd.lower().startswith('ls'):
                if not client.connected:
                    print("[-] Not connected")
                    continue
                    
                parts = cmd.split()
                path = parts[1] if len(parts) > 1 else ""
                try:
                    result, output = client.list_directory(path)
                    if result == 0:
                        print(f"[+] Directory listing:\n{output}")
                    else:
                        print(f"[-] List failed with code: {result}")
                except Exception as e:
                    print(f"[-] List failed: {e}")
                    
            elif cmd.lower() == 'shutdown':
                if not client.connected:
                    print("[-] Not connected")
                    continue
                    
                try:
                    result = client.shutdown()
                    print("[+] Server shutdown command sent")
                except Exception as e:
                    print(f"[-] Shutdown failed: {e}")
                    
            elif cmd.lower() in ['ping', 'status']:
                if not client.connected:
                    print("[-] Not connected")
                    continue
                    
                try:
                    result, output = client.ping()
                    if result == 0:
                        print(f"[+] Server response: {output}")
                        print("[+] Connected")
                    else:
                        print(f"[-] Ping failed with code: {result}")
                except Exception as e:
                    print(f"[-] Ping failed: {e}")
                    print("[-] Not connected")
                    
            else:
                print("[-] Unknown command. Type 'help' for available commands.")
                
        except KeyboardInterrupt:
            print("\n[!] Interrupted")
            break
        except EOFError:
            break

def main():
    parser = argparse.ArgumentParser(description="RPC Client")
    parser.add_argument("--host", default="127.0.0.1", help="Server IP address")
    parser.add_argument("--port", default="12345", help="Server port")
    parser.add_argument("--command", help="Command to execute on server")
    parser.add_argument("--silent", action="store_true", help="Execute command without output")
    parser.add_argument("--upload", nargs=2, metavar=("LOCAL", "REMOTE"), help="Upload file to server")
    parser.add_argument("--download", nargs=2, metavar=("REMOTE", "LOCAL"), help="Download file from server")
    parser.add_argument("--list", nargs='?', const="", help="List directory contents")
    parser.add_argument("--shutdown", action="store_true", help="Shutdown the server")
    parser.add_argument("--ping", action="store_true", help="Ping server")
    parser.add_argument("--interactive", action="store_true", help="Start interactive shell")
    args = parser.parse_args()

    client = MyRpcInterfaceClient(None)
        
    commands = [args.command, args.silent, args.upload, args.download, 
                args.list is not None, args.interactive, args.shutdown, args.ping]
    if not any(commands):
        parser.error("You must specify at least one action")
        return 1
        
    try:
        client.connect(args.host, args.port)
        
        if args.interactive:
            interactive_shell(client)
            return 0
    
        elif args.ping:
            print("[*] Pinging server...")
            result, output = client.ping()
            if result == 0:
                print(f"[+] Server response: {output}")
            else:
                print(f"[-] Ping failed with code: {result}")
                
        elif args.command:
            print(f"[*] Executing command: {args.command}")
            if args.silent:
                result = client.execute_silent(args.command)
                if result == 0:
                    print("[+] Command executed")
                else:
                    print(f"[-] Command failed with code: {result}")
            else:
                result, output = client.execute(args.command)
                if result == 0:
                    print(f"[+] Command output:\n{output}")
                else:
                    print(f"[-] Command failed with code: {result}")
                    
        elif args.upload:
            local_path, remote_path = args.upload
            print(f"[*] Uploading {local_path} to {remote_path}")
            result = client.upload_file(local_path, remote_path)
            if result == 0:
                print("[+] File uploaded")
            else:
                print(f"[-] Upload failed with code: {result}")
                
        elif args.download:
            remote_path, local_path = args.download
            print(f"[*] Downloading {remote_path} to {local_path}")
            result = client.download_file(remote_path, local_path)
            if result == 0:
                print("[+] File downloaded")
            else:
                print(f"[-] Download failed with code: {result}")
                
        elif args.list is not None:
            path = args.list
            print(f"[*] Listing directory: {path if path else 'current'}")
            result, output = client.list_directory(path)
            if result == 0:
                print(f"[+] Directory listing:\n{output}")
            else:
                print(f"[-] List failed with code: {result}")
                
        elif args.shutdown:
            print("[*] Sending shutdown command to server...")
            result = client.shutdown()
            print("[+] Server shutdown command sent")
            
    except Exception as e:
        print(f"[-] Error: {e}")
        return 1
        
    finally:
        if client.connected:
            client.disconnect()
            
    return 0

if __name__ == "__main__":
    main()