# Traymond IPC - TCP Interface Documentation

## Overview

Traymond IPC is a Windows application that allows you to minimize windows to the system tray. It provides both a graphical interface (tray icon and hotkeys) and a TCP-based IPC (Inter-Process Communication) interface for programmatic control.

## TCP Server

The application runs a TCP server on `127.0.0.1:8766` that accepts commands to control window management operations.

### Server Details
- **Host**: 127.0.0.1 (localhost)
- **Port**: 8766
- **Protocol**: TCP
- **Connection Type**: Single-request (connection closes after each command)

### Supported Commands

The TCP server accepts the following commands as plain text:

#### 1. MINIMIZE_CURRENT
Minimizes the currently focused window to the system tray.

**Command**: `MINIMIZE_CURRENT`

**Example**:
```bash
echo "MINIMIZE_CURRENT" | nc 127.0.0.1 8766
```

#### 2. MINIMIZE_BY_HANDLE
Minimizes a specific window by its handle to the system tray.

**Command**: `MINIMIZE_BY_HANDLE:HANDLE:<window_handle>`

**Parameters**:
- `<window_handle>`: The hexadecimal window handle of the target window

**Example**:
```bash
echo "MINIMIZE_BY_HANDLE:HANDLE:12345678" | nc 127.0.0.1 8766
```

#### 3. SHOW_ALL
Restores all hidden windows from the system tray.

**Command**: `SHOW_ALL`

**Example**:
```bash
echo "SHOW_ALL" | nc 127.0.0.1 8766
```

#### 4. EXIT
Terminates the Traymond IPC application.

**Command**: `EXIT`

**Example**:
```bash
echo "EXIT" | nc 127.0.0.1 8766
```

## Command Line Flags

The application supports the following command line flags:

### -debug
Enables debug mode, which shows a console window with debug output.

**Usage**:
```bash
traymond-tcp.exe -debug
```

**Effects**:
- Creates a console window for debug output
- Shows detailed logging of TCP server operations
- Displays connection status and received commands

### -noTray
Disables the main tray icon (only affects the main application icon, not individual window icons).

**Usage**:
```bash
traymond-tcp.exe -noTray
```

**Effects**:
- Hides the main Traymond IPC tray icon
- Individual window icons still appear when windows are minimized
- TCP functionality remains fully operational

### Combining Flags
You can combine multiple flags:

```bash
traymond-tcp.exe -debug -noTray
```

## Usage Examples

### Basic Usage
1. Start the application:
   ```bash
   traymond-tcp.exe
   ```

2. Use the hotkey `Win+Shift+Z` to minimize the current window to tray

3. Use TCP commands for programmatic control:
   ```bash
   # Minimize current window
   echo "MINIMIZE_CURRENT" | nc 127.0.0.1 8766
   
   # Show all hidden windows
   echo "SHOW_ALL" | nc 127.0.0.1 8766
   ```

### Debug Mode
For development or troubleshooting:

```bash
traymond-tcp.exe -debug
```

This will show a console window with messages like:
```
Traymond IPC: Starting up in debug mode...
Traymond IPC: TCP server listening on 127.0.0.1:8766
Traymond IPC: Started successfully, waiting for connections...
Traymond IPC: Client connected successfully
Traymond IPC: Received data: MINIMIZE_CURRENT
Traymond IPC: Client disconnected
```

### Headless Mode
For server environments or when you don't want the main tray icon:

```bash
traymond-tcp.exe -noTray
```

## Client Libraries

### Rust Client
The project includes Rust client libraries:
- `traymond_client.rs` - Basic client implementation
- `traymond_client_lib.rs` - Library version for integration

### Python Example
```python
import socket

def send_command(command):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(('127.0.0.1', 8766))
        sock.send(command.encode())
        sock.close()
    except Exception as e:
        print(f"Error: {e}")

# Minimize current window
send_command("MINIMIZE_CURRENT")

# Show all windows
send_command("SHOW_ALL")
```

### PowerShell Example
```powershell
function Send-TraymondCommand {
    param([string]$Command)
    
    $client = New-Object System.Net.Sockets.TcpClient
    $client.Connect("127.0.0.1", 8766)
    $stream = $client.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $writer.Write($Command)
    $writer.Flush()
    $client.Close()
}

# Minimize current window
Send-TraymondCommand "MINIMIZE_CURRENT"

# Show all windows
Send-TraymondCommand "SHOW_ALL"
```

## Error Handling

- If the TCP server fails to start, the application will show a warning message
- If a client connection fails, the server continues running
- Invalid commands are ignored
- The server automatically handles connection cleanup

## Security Considerations

- The TCP server only listens on localhost (127.0.0.1)
- No authentication is required (intended for local use only)
- Commands are processed as plain text
- Consider firewall rules if running in restricted environments

## Troubleshooting

### Common Issues

1. **Port already in use**: Another instance might be running
2. **Connection refused**: Application not running or wrong port
3. **No debug output**: Make sure to use the `-debug` flag

### Debug Information
When running with `-debug`, you'll see:
- Server startup messages
- Client connection events
- Received commands
- Error messages

## Building

The application can be built using Visual Studio or MSBuild:

```bash
msbuild traymond.sln /p:Configuration=Release
```

The executable will be placed in the `Release/` directory.
