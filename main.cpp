#include <windows.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define WM_UPDATE_STATUS (WM_USER + 2)

#define ID_TRAY_APP_ICON 1001

#define IDM_VERSION 2000
#define IDM_STATUS 2001
#define IDM_START 2002
#define IDM_STOP 2003
#define IDM_CONFIG 2004
#define IDM_DASHBOARD 2005
#define IDM_EXIT 2006

NOTIFYICONDATAW nid;
HWND hWnd;
HMENU hCurrentMenu = NULL;

const wchar_t *HERMES_INSTALL_PATH = L"\\AppData\\Local\\hermes";
const wchar_t *HERMES_PTYHONW_PATH = L"\\hermes-agent\\venv\\Scripts\\pythonw.exe";
const wchar_t *HERMES_GATEWAY_CMD_PATH = L"\\gateway-service\\Hermes_Gateway.cmd";

std::wstring g_hermesHome;
std::wstring g_hermesVersion = L"Version: Fetching...";

std::wstring GetEnvVar(const std::wstring& name) {
    DWORD bufferSize = GetEnvironmentVariableW(name.c_str(), NULL, 0);
    if (bufferSize == 0) return L"";
    std::wstring buffer(bufferSize, L'\0');
    GetEnvironmentVariableW(name.c_str(), &buffer[0], bufferSize);
    buffer.resize(bufferSize - 1); // Remove null terminator
    return buffer;
}

std::wstring ExecCmd(const std::wstring& args) {
    HANDLE hPipeRead, hPipeWrite;
    SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0)) return L"Status: Error (Pipe)";

    SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};
    
    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
    
    std::wstring pythonwPath = g_hermesHome + HERMES_PTYHONW_PATH;
    std::wstring cmdLine = L"\"" + pythonwPath + L"\" -c \"import sys; from hermes_cli.main import main; sys.exit(main())\" " + args;
    
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    if (!CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hPipeRead);
        CloseHandle(hPipeWrite);
        return L"Status: Error (Process)";
    }

    CloseHandle(hPipeWrite);

    std::string resultStr;
    char buffer[256];
    DWORD bytesRead;
    while (ReadFile(hPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        resultStr.append(buffer, bytesRead);
    }

    CloseHandle(hPipeRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (resultStr.empty()) return L"Status: Unknown (No output)";
    
    int wlen = MultiByteToWideChar(CP_UTF8, 0, resultStr.c_str(), (int)resultStr.length(), NULL, 0);
    std::wstring result(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, resultStr.c_str(), (int)resultStr.length(), &result[0], wlen);

    size_t newlinePos = result.find(L'\n');
    if (newlinePos != std::wstring::npos) {
        result = result.substr(0, newlinePos);
    }
    size_t crPos = result.find(L'\r');
    if (crPos != std::wstring::npos) {
        result = result.substr(0, crPos);
    }

    return result;
}

void RunCommandDetached(const std::wstring& args) {
    std::thread([args]() {
        ExecCmd(args);
    }).detach();
}

void StartGatewayCmd() {
    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};
    
    std::wstring cmdPath = g_hermesHome + HERMES_GATEWAY_CMD_PATH;
    std::wstring cmdLine = L"cmd.exe /c \"" + cmdPath + L"\"";
    
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    if (CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void FetchVersionAsync() {
    std::thread([]() {
        std::wstring ver = ExecCmd(L"--version");
        if (!ver.empty() && ver.find(L"Error") == std::wstring::npos) {
            g_hermesVersion = ver;
        } else {
            g_hermesVersion = L"Version: Unknown";
        }
    }).detach();
}

void FetchStatusAsync() {
    std::thread([]() {
        std::wstring statusStr = ExecCmd(L"gateway status");
        if (statusStr.length() > 50) {
            statusStr = statusStr.substr(0, 47) + L"...";
        }
        std::wstring* newStatus = new std::wstring(statusStr);
        PostMessageW(hWnd, WM_UPDATE_STATUS, 0, (LPARAM)newStatus);
    }).detach();
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    hCurrentMenu = CreatePopupMenu();
    
    AppendMenuW(hCurrentMenu, MF_STRING | MF_DISABLED, IDM_VERSION, g_hermesVersion.c_str());
    AppendMenuW(hCurrentMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hCurrentMenu, MF_STRING | MF_DISABLED, IDM_STATUS, L"Status: Fetching...");
    AppendMenuW(hCurrentMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hCurrentMenu, MF_STRING, IDM_START, L"Start Gateway");
    AppendMenuW(hCurrentMenu, MF_STRING, IDM_STOP, L"Stop Gateway");
    AppendMenuW(hCurrentMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hCurrentMenu, MF_STRING, IDM_CONFIG, L"Open Config Directory");
    AppendMenuW(hCurrentMenu, MF_STRING, IDM_DASHBOARD, L"Open Web Dashboard");
    AppendMenuW(hCurrentMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hCurrentMenu, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    
    FetchVersionAsync();

    FetchStatusAsync();
    
    TrackPopupMenu(hCurrentMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL);
    
    DestroyMenu(hCurrentMenu);
    hCurrentMenu = NULL;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_UPDATE_STATUS: {
            std::wstring* statusStr = (std::wstring*)lParam;
            if (hCurrentMenu != NULL) {
                ModifyMenuW(hCurrentMenu, IDM_STATUS, MF_BYCOMMAND | MF_STRING | MF_DISABLED, IDM_STATUS, statusStr->c_str());
            }
            delete statusStr;
            break;
        }

        case WM_TRAYICON:
            if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
                ShowContextMenu(hwnd);
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_START:
                    StartGatewayCmd();
                    break;
                case IDM_STOP:
                    RunCommandDetached(L"gateway stop");
                    break;
                case IDM_CONFIG:
                    ShellExecuteW(NULL, L"explore", g_hermesHome.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    break;
                case IDM_DASHBOARD:
                    RunCommandDetached(L"dashboard");
                    break;
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            break;

        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hermesHome = GetEnvVar(L"HERMES_HOME");
    if (g_hermesHome.empty()) {
        std::wstring userProfile = GetEnvVar(L"USERPROFILE");
        g_hermesHome = userProfile + HERMES_INSTALL_PATH;
    }

    const wchar_t* CLASS_NAME = L"HermesTrayClass";

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    hWnd = CreateWindowExW(
        0, CLASS_NAME, L"Hermes Tray", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL
    );

    if (hWnd == NULL) {
        return 0;
    }

    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hWnd;
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    
    HICON hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(101));
    if (!hIcon) {
        hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    nid.hIcon = hIcon;
    lstrcpyW(nid.szTip, L"Hermes Gateway Manager");

    Shell_NotifyIconW(NIM_ADD, &nid);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
