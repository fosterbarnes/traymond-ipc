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
- Outputs startup messages and error information

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
- Right-click menu functionality is disabled

### -noHotkey
Disables the hotkey functionality (Win+Shift+Z).

**Usage**:
```bash
traymond-tcp.exe -noHotkey
```

**Effects**:
- Disables the Win+Shift+Z hotkey for minimizing windows
- TCP functionality remains fully operational
- Tray icon functionality remains available

### Combining Flags
You can combine multiple flags in any order:

```bash
traymond-tcp.exe -debug -noTray
traymond-tcp.exe -debug -noHotkey
traymond-tcp.exe -noTray -noHotkey
traymond-tcp.exe -debug -noTray -noHotkey
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

### Server Mode
For pure server environments where you only want TCP functionality:

```bash
traymond-tcp.exe -noTray -noHotkey
```

## Client Libraries

### Rust Client
The project includes Rust client libraries:

#### Basic Client (`traymond_client.rs`)
A simple command-line client for testing and basic usage.

**Usage**:
```bash
cargo run --bin traymond_client
```

#### Library Client (`traymond_client_lib.rs`)
A library version for integration into other Rust applications.

**Example Usage**:
```rust
use traymond_client_lib::{TraymondClient, TraymondCommand};

// Minimize current window
TraymondClient::minimize_current()?;

// Minimize window by handle
TraymondClient::minimize_by_handle(0x12345678)?;

// Show all windows
TraymondClient::show_all()?;

// Exit Traymond
TraymondClient::exit()?;

// Check if Traymond is running
if TraymondClient::is_running() {
    println!("Traymond is running");
}
```

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

## Features

### Window Management
- **Minimize to Tray**: Hide windows to system tray with individual icons
- **Restore Windows**: Double-click tray icons or use SHOW_ALL command
- **Window Persistence**: Automatically restores windows after unexpected termination
- **Restricted Windows**: Prevents hiding critical system windows (Taskbar, Desktop)

### Hotkey Support
- **Default Hotkey**: Win+Shift+Z to minimize current window
- **Configurable**: Can be disabled with `-noHotkey` flag
- **Non-repeating**: Prevents accidental multiple triggers

### TCP Server
- **Local Only**: Binds to 127.0.0.1 for security
- **Single Instance**: Mutex prevents multiple instances
- **Graceful Shutdown**: Proper cleanup on exit
- **Error Handling**: Continues running even if client connections fail

### Debug Features
- **Console Output**: Detailed logging with `-debug` flag
- **Connection Logging**: Shows client connections and commands
- **Error Reporting**: Displays startup and runtime errors

## Error Handling

- If the TCP server fails to start, the application will show a warning message
- If a client connection fails, the server continues running
- Invalid commands are ignored
- The server automatically handles connection cleanup
- Mutex prevents multiple instances from running simultaneously

## Security Considerations

- The TCP server only listens on localhost (127.0.0.1)
- No authentication is required (intended for local use only)
- Commands are processed as plain text
- Consider firewall rules if running in restricted environments
- Single instance enforcement prevents conflicts

## Troubleshooting

### Common Issues

1. **Port already in use**: Another instance might be running
2. **Connection refused**: Application not running or wrong port
3. **No debug output**: Make sure to use the `-debug` flag
4. **Hotkey not working**: Check if another application is using Win+Shift+Z

### Debug Information
When running with `-debug`, you'll see:
- Server startup messages
- Client connection events
- Received commands
- Error messages
- Window management operations

### Instance Management
- Only one instance can run at a time
- Previous instances are automatically detected and prevented
- Use Task Manager to kill stuck instances if needed

## Building

The application can be built using Visual Studio or MSBuild:

```bash
msbuild traymond.sln /p:Configuration=Release
```

The executable will be placed in the `Release/` directory.

### Build Requirements
- Visual Studio 2019 or later
- Windows SDK
- C++17 or later

## File Structure

- `traymond-tcp.exe` - Main executable
- `traymond-tcp.dat` - Window persistence file (auto-created)
- `traymond_client.rs` - Basic Rust client
- `traymond_client_lib.rs` - Rust library for integration
