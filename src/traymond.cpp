#include <Windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <thread>

#define VK_Z_KEY 0x5A
// These keys are used to send windows to tray
#define TRAY_KEY VK_Z_KEY
#define MOD_KEY MOD_WIN + MOD_SHIFT

#define WM_ICON 0x1C0A
#define WM_OURICON 0x1C0B
#define WM_IPC_COMMAND 0x1C0C
#define WM_SAVE_TIMER 0x1C0D
#define EXIT_ID 0x99
#define SHOW_ALL_ID 0x98
#define MAXIMUM_WINDOWS 100

// IPC Commands
#define IPC_MINIMIZE_CURRENT 1
#define IPC_MINIMIZE_BY_HANDLE 2
#define IPC_SHOW_ALL 3
#define IPC_EXIT 4

// IPC Message structure
typedef struct IPC_MESSAGE {
    int command;
    HWND windowHandle;
    char windowTitle[256];
} IPC_MESSAGE;

// Stores hidden window record.
typedef struct HIDDEN_WINDOW {
  NOTIFYICONDATA icon;
  HWND window;
} HIDDEN_WINDOW;

// Current execution context
typedef struct TRCONTEXT {
  HWND mainWindow;
  HIDDEN_WINDOW icons[MAXIMUM_WINDOWS];
  HMENU trayMenu;
  int iconIndex; // How many windows are currently hidden
  bool showTrayIcon; // Whether to show the main tray icon
} TRCONTEXT;

HANDLE saveFile;
HANDLE ipcPipe = INVALID_HANDLE_VALUE;
HANDLE ipcThread = NULL;
HANDLE ipcStopEvent = NULL;
bool ipcRunning = false;
bool savePending = false;

// Function to parse command line arguments
bool parseCommandLine(LPSTR lpCmdLine, bool* showTrayIcon) {
    *showTrayIcon = true; // Default to showing tray icon
    
    if (lpCmdLine && strlen(lpCmdLine) > 0) {
        // Check for -noTray flag
        if (strstr(lpCmdLine, "-noTray") != NULL) {
            *showTrayIcon = false;
        }
    }
    
    return true;
}

// IPC Server thread function
DWORD WINAPI IPCServerThread(LPVOID lpParam) {
    TRCONTEXT* context = (TRCONTEXT*)lpParam;
    
    // Create named pipe once
    ipcPipe = CreateNamedPipeA(
        "\\\\.\\pipe\\traymond_ipc",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        sizeof(IPC_MESSAGE),
        sizeof(IPC_MESSAGE),
        0,
        NULL
    );
    
    if (ipcPipe == INVALID_HANDLE_VALUE) {
        return 1; // Exit if we can't create the pipe
    }
    
    while (ipcRunning) {
        // Wait for client connection with timeout using event
        DWORD waitResult = WaitForSingleObject(ipcStopEvent, 50); // Check every 50ms for better responsiveness
        if (waitResult == WAIT_OBJECT_0) {
            // Stop event was signaled
            break;
        }
        
        // Try to connect
        if (ConnectNamedPipe(ipcPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            IPC_MESSAGE msg;
            DWORD bytesRead;
            
            // Read message from client
            if (ReadFile(ipcPipe, &msg, sizeof(IPC_MESSAGE), &bytesRead, NULL)) {
                // Post message to main thread to handle the command
                PostMessage(context->mainWindow, WM_IPC_COMMAND, 0, (LPARAM)&msg);
            }
            
            DisconnectNamedPipe(ipcPipe);
        }
    }
    
    return 0;
}

// Saves our hidden windows so they can be restored in case
// of crashing.
void save(const TRCONTEXT *context) {
  DWORD numbytes;
  // Truncate file
  SetFilePointer(saveFile, 0, NULL, FILE_BEGIN);
  SetEndOfFile(saveFile);
  if (!context->iconIndex) {
    return;
  }
  
  char buffer[32];
  for (int i = 0; i < context->iconIndex; i++)
  {
    if (context->icons[i].window) {
      int len = sprintf_s(buffer, sizeof(buffer), "%ld,", (long)context->icons[i].window);
      if (len > 0) {
        WriteFile(saveFile, buffer, len, &numbytes, NULL);
      }
    }
  }
}

// Restores a window
void showWindow(TRCONTEXT *context, LPARAM lParam) {
  for (int i = 0; i < context->iconIndex; i++)
  {
    if (context->icons[i].icon.uID == HIWORD(lParam)) {
      ShowWindow(context->icons[i].window, SW_SHOW);
      Shell_NotifyIcon(NIM_DELETE, &context->icons[i].icon);
      SetForegroundWindow(context->icons[i].window);
      
      // Move the last element to this position to avoid holes
      if (i < context->iconIndex - 1) {
        context->icons[i] = context->icons[context->iconIndex - 1];
      }
      context->icons[context->iconIndex - 1] = {};
      context->iconIndex--;
      
      savePending = true;
      break;
    }
  }
}

// Minimizes the current window to tray.
// Uses currently focused window unless supplied a handle as the argument.
void minimizeToTray(TRCONTEXT *context, long restoreWindow) {
  // Taskbar and desktop windows are restricted from hiding.
  const char restrictWins[][14] = { {"WorkerW"}, {"Shell_TrayWnd"} };

  HWND currWin = 0;
  if (!restoreWindow) {
    currWin = GetForegroundWindow();
  }
  else {
    currWin = reinterpret_cast<HWND>(restoreWindow);
  }

  if (!currWin) {
    return;
  }

  char className[256];
  if (!GetClassName(currWin, className, 256)) {
    return;
  }
  else {
    for (int i = 0; i < sizeof(restrictWins) / sizeof(*restrictWins); i++)
    {
      if (strcmp(restrictWins[i], className) == 0) {
        return;
      }
    }
  }
  if (context->iconIndex == MAXIMUM_WINDOWS) {
    MessageBoxA(NULL, "Error! Too many hidden windows. Please unhide some.", "Traymond IPC", MB_OK | MB_ICONERROR);
    return;
  }
  ULONG_PTR icon = GetClassLongPtr(currWin, GCLP_HICONSM);
  if (!icon) {
    icon = SendMessage(currWin, WM_GETICON, 2, NULL);
    if (!icon) {
      return;
    }
  }

  NOTIFYICONDATA nid;
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = context->mainWindow;
  nid.hIcon = (HICON)icon;
  nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  nid.uVersion = NOTIFYICON_VERSION_4;
  nid.uID = LOWORD(reinterpret_cast<UINT>(currWin));
  nid.uCallbackMessage = WM_ICON;
  GetWindowText(currWin, nid.szTip, 128);
  context->icons[context->iconIndex].icon = nid;
  context->icons[context->iconIndex].window = currWin;
  context->iconIndex++;
  Shell_NotifyIcon(NIM_ADD, &nid);
  Shell_NotifyIcon(NIM_SETVERSION, &nid);
  ShowWindow(currWin, SW_HIDE);
  if (!restoreWindow) {
    savePending = true;
  }

}

// Adds our own icon to tray
void createTrayIcon(HWND mainWindow, HINSTANCE hInstance, NOTIFYICONDATA* icon) {
  icon->cbSize = sizeof(NOTIFYICONDATA);
  icon->hWnd = mainWindow;
  icon->hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
  icon->uFlags = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE;
  icon->uVersion = NOTIFYICON_VERSION_4;
  icon->uID = reinterpret_cast<UINT>(mainWindow);
  icon->uCallbackMessage = WM_OURICON;
  strcpy_s(icon->szTip, sizeof(icon->szTip), "Traymond IPC");
  Shell_NotifyIcon(NIM_ADD, icon);
  Shell_NotifyIcon(NIM_SETVERSION, icon);
}

// Creates our tray icon menu
void createTrayMenu(HMENU* trayMenu) {
  *trayMenu = CreatePopupMenu();

  MENUITEMINFO showAllMenuItem;
  MENUITEMINFO exitMenuItem;

  exitMenuItem.cbSize = sizeof(MENUITEMINFO);
  exitMenuItem.fMask = MIIM_STRING | MIIM_ID;
  exitMenuItem.fType = MFT_STRING;
  exitMenuItem.dwTypeData = "Exit";
  exitMenuItem.cch = 5;
  exitMenuItem.wID = EXIT_ID;

  showAllMenuItem.cbSize = sizeof(MENUITEMINFO);
  showAllMenuItem.fMask = MIIM_STRING | MIIM_ID;
  showAllMenuItem.fType = MFT_STRING;
  showAllMenuItem.dwTypeData = "Restore all windows";
  showAllMenuItem.cch = 20;
  showAllMenuItem.wID = SHOW_ALL_ID;

  InsertMenuItem(*trayMenu, 0, FALSE, &showAllMenuItem);
  InsertMenuItem(*trayMenu, 0, FALSE, &exitMenuItem);
}
// Shows all hidden windows;
void showAllWindows(TRCONTEXT *context) {
  for (int i = 0; i < context->iconIndex; i++)
  {
    ShowWindow(context->icons[i].window, SW_SHOW);
    Shell_NotifyIcon(NIM_DELETE, &context->icons[i].icon);
    context->icons[i] = {};
  }
  savePending = true;
  context->iconIndex = 0;
}

void exitApp() {
  // Stop IPC server thread first
  if (ipcThread) {
    ipcRunning = false;
    // Signal the thread to stop using the event
    if (ipcStopEvent) {
      SetEvent(ipcStopEvent);
    }
    // Wait a short time for the thread to exit gracefully
    if (WaitForSingleObject(ipcThread, 2000) == WAIT_TIMEOUT) {
      // If thread doesn't exit gracefully, terminate it
      TerminateThread(ipcThread, 0);
    }
    CloseHandle(ipcThread);
    ipcThread = NULL;
  }
  
  // Clean up the stop event
  if (ipcStopEvent) {
    CloseHandle(ipcStopEvent);
    ipcStopEvent = NULL;
  }
  
  PostQuitMessage(0);
}

// Creates and reads the save file to restore hidden windows in case of unexpected termination
void startup(TRCONTEXT *context) {
  if ((saveFile = CreateFile("traymond-ipc.dat", GENERIC_READ | GENERIC_WRITE, \
    0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
    MessageBoxA(NULL, "Error! Traymond IPC could not create a save file.", "Traymond IPC", MB_OK | MB_ICONERROR);
    exitApp();
  }
  // Check if we've crashed (i. e. there is a save file) during current uptime and
  // if there are windows to restore, in which case restore them and
  // display a reassuring message.
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    DWORD numbytes;
    DWORD fileSize = GetFileSize(saveFile, NULL);

    if (!fileSize) {
      return;
    };

    FILETIME saveFileWriteTime;
    GetFileTime(saveFile, NULL, NULL, &saveFileWriteTime);
    uint64_t writeTime = ((uint64_t)saveFileWriteTime.dwHighDateTime << 32 | (uint64_t)saveFileWriteTime.dwLowDateTime) / 10000;
    GetSystemTimeAsFileTime(&saveFileWriteTime);
    writeTime = (((uint64_t)saveFileWriteTime.dwHighDateTime << 32 | (uint64_t)saveFileWriteTime.dwLowDateTime) / 10000) - writeTime;

    if (GetTickCount64() < writeTime) {
      return;
    }

    char* contents = (char*)malloc(fileSize + 1);
    if (contents) {
      ReadFile(saveFile, contents, fileSize, &numbytes, NULL);
      contents[fileSize] = '\0';
      
      char handle[16];
      int index = 0;
      for (size_t i = 0; i < fileSize; i++)
      {
        if (contents[i] != ',') {
          handle[index] = contents[i];
          index++;
        }
        else {
          handle[index] = '\0';
          index = 0;
          minimizeToTray(context, atol(handle));
        }
      }
      free(contents);
      
      char restore_message[256];
      sprintf_s(restore_message, sizeof(restore_message), 
        "Traymond IPC had previously been terminated unexpectedly.\n\nRestored %d %s.", 
        context->iconIndex, context->iconIndex > 1 ? "icons" : "icon");
      MessageBoxA(NULL, restore_message, "Traymond IPC", MB_OK);
    }
    }
  }

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

  TRCONTEXT* context = reinterpret_cast<TRCONTEXT*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  POINT pt;
  switch (uMsg)
  {
  case WM_ICON:
    if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
      showWindow(context, lParam);
    }
    break;
  case WM_OURICON:
    if (LOWORD(lParam) == WM_RBUTTONUP && context->showTrayIcon) {
      SetForegroundWindow(hwnd);
      GetCursorPos(&pt);
      TrackPopupMenuEx(context->trayMenu, \
      (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN) | TPM_BOTTOMALIGN, \
        pt.x, pt.y, hwnd, NULL);
    }
    break;
  case WM_COMMAND:
    if (HIWORD(wParam) == 0) {
      switch LOWORD(wParam) {
      case SHOW_ALL_ID:
        showAllWindows(context);
        break;
      case EXIT_ID:
        exitApp();
        break;
      }
    }
    break;
  case WM_HOTKEY: // We only have one hotkey, so no need to check the message
    minimizeToTray(context, NULL);
    break;
  case WM_IPC_COMMAND:
    {
      IPC_MESSAGE* msg = (IPC_MESSAGE*)lParam;
      switch (msg->command) {
        case IPC_MINIMIZE_CURRENT:
          minimizeToTray(context, NULL);
          break;
        case IPC_MINIMIZE_BY_HANDLE:
          minimizeToTray(context, (long)msg->windowHandle);
          break;
        case IPC_SHOW_ALL:
          showAllWindows(context);
          break;
        case IPC_EXIT:
          exitApp();
          break;
      }
    }
    break;
  case WM_SAVE_TIMER:
    if (savePending) {
      save(context);
      savePending = false;
    }
    break;
  default:
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return 0;
}

#pragma warning( push )
#pragma warning( disable : 4100 )
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
#pragma warning( pop )

  TRCONTEXT context = {};

  NOTIFYICONDATA icon = {};

  // Parse command line arguments first
  bool showTrayIcon = true;
  parseCommandLine(lpCmdLine, &showTrayIcon);
  context.showTrayIcon = showTrayIcon;

  // Mutex to allow only one instance
  const char szUniqueNamedMutex[] = "traymond_ipc_mutex";
  HANDLE mutex = CreateMutex(NULL, TRUE, szUniqueNamedMutex);
  if (GetLastError() == ERROR_ALREADY_EXISTS)
  {
    MessageBoxA(NULL, "Error! Another instance of Traymond IPC is already running.", "Traymond IPC", MB_OK | MB_ICONERROR);
    return 1;
  }

  BOOL bRet;
  MSG msg;

  const char CLASS_NAME[] = "TraymondIPC";

  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;

  if (!RegisterClass(&wc)) {
    return 1;
  }

  context.mainWindow = CreateWindow(CLASS_NAME, NULL, NULL, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

  if (!context.mainWindow) {
    return 1;
  }

  // Store our context in main window for retrieval by WindowProc
  SetWindowLongPtr(context.mainWindow, GWLP_USERDATA, reinterpret_cast<LONG>(&context));

  if (!RegisterHotKey(context.mainWindow, 0, MOD_KEY | MOD_NOREPEAT, TRAY_KEY)) {
    MessageBoxA(NULL, "Error! Could not register the hotkey.", "Traymond IPC", MB_OK | MB_ICONERROR);
    return 1;
  }

  // Create tray icon only if showTrayIcon is true
  if (context.showTrayIcon) {
    createTrayIcon(context.mainWindow, hInstance, &icon);
  }
  createTrayMenu(&context.trayMenu);
  startup(&context);
  
  // Set up save timer (every 2 seconds)
  SetTimer(context.mainWindow, WM_SAVE_TIMER, 2000, NULL);

  // Start IPC server thread
  ipcRunning = true;
  ipcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!ipcStopEvent) {
    MessageBoxA(NULL, "Warning! Could not create IPC stop event.", "Traymond IPC", MB_OK | MB_ICONWARNING);
  }
  
  ipcThread = CreateThread(NULL, 0, IPCServerThread, &context, 0, NULL);
  if (!ipcThread) {
    MessageBoxA(NULL, "Warning! Could not start IPC server thread.", "Traymond IPC", MB_OK | MB_ICONWARNING);
  }

  while ((bRet = GetMessage(&msg, 0, 0, 0)) != 0)
  {
    if (bRet != -1) {
      DispatchMessage(&msg);
    }
  }
  // Clean up on exit;
  KillTimer(context.mainWindow, WM_SAVE_TIMER);
  if (savePending) {
    save(&context);
  }
  showAllWindows(&context);
  // Only remove tray icon if it was created
  if (context.showTrayIcon) {
    Shell_NotifyIcon(NIM_DELETE, &icon);
  }
  
  // Stop IPC server thread
  if (ipcThread) {
    ipcRunning = false;
    // Signal the thread to stop using the event
    if (ipcStopEvent) {
      SetEvent(ipcStopEvent);
    }
    // Wait a short time for the thread to exit gracefully
    if (WaitForSingleObject(ipcThread, 2000) == WAIT_TIMEOUT) {
      // If thread doesn't exit gracefully, terminate it
      TerminateThread(ipcThread, 0);
    }
    CloseHandle(ipcThread);
  }
  
  // Clean up the stop event
  if (ipcStopEvent) {
    CloseHandle(ipcStopEvent);
  }
  
  if (ipcPipe != INVALID_HANDLE_VALUE) {
    CloseHandle(ipcPipe);
  }
  
  ReleaseMutex(mutex);
  CloseHandle(mutex);
  CloseHandle(saveFile);
  DestroyMenu(context.trayMenu);
  DestroyWindow(context.mainWindow);
  DeleteFile("traymond-ipc.dat"); // No save file means we have exited gracefully
  UnregisterHotKey(context.mainWindow, 0);
  return msg.wParam;
}
