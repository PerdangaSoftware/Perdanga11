#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ole2.h>
#include <gdiplus.h>
#include <shellapi.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "uiautomationcore.lib")

#include "resource.h"
#include "Config.hpp"
#include "AppIndexer.hpp"
#include "Hooks.hpp"
#include "MenuRenderer.hpp"
#include "MenuInteraction.hpp"
#include "MenuWindow.hpp"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 2001
#define ID_TRAY_AUTOSTART 2002
#define ID_TRAY_OPEN_CONFIG 2003

#ifndef MSGFLT_ADD
#define MSGFLT_ADD 1
#endif

NOTIFYICONDATAW g_nid = { 0 };

LRESULT CALLBACK SubclassedTrayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            POINT curPoint;
            GetCursorPos(&curPoint);

            MenuInteraction::ApplyMenuTheme();
            HMENU hMenu = CreatePopupMenu();
            bool autostart = Config::IsAutostartEnabled();

            const wchar_t* autostartText = Config::IsRussian() ? L"\x0410\x0432\x0442\x043E\x0437\x0430\x043F\x0443\x0441\x043A \x043F\x0440\x0438 \x0441\x0442\x0430\x0440\x0442\x0435" : L"Run at Startup";
            const wchar_t* configText = Config::IsRussian() ? L"\x041E\x0442\x043A\x0440\x044B\x0442\x044C config.ini" : L"Edit config.ini";
            const wchar_t* langTitle = Config::IsRussian() ? L"\x042F\x0437\x044B\x043A" : L"Language";
            const wchar_t* exitText = Config::IsRussian() ? L"\x0412\x044B\x0445\x043E\x0434" : L"Exit";

            AppendMenuW(hMenu, MF_STRING | (autostart ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_AUTOSTART, autostartText);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_CONFIG, configText);

            HMENU hLangSub = CreatePopupMenu();
            AppendMenuW(hLangSub, MF_STRING | (Config::g_configuredLanguage == AppLanguage::Auto ? MF_CHECKED : 0), ID_LANG_AUTO, Config::IsRussian() ? L"\x0410\x0432\x0442\x043E (\x0441\x0438\x0441\x0442\x0435\x043C\x043D\x044B\x0439)" : L"Auto (System)");
            AppendMenuW(hLangSub, MF_STRING | (Config::g_configuredLanguage == AppLanguage::English ? MF_CHECKED : 0), ID_LANG_EN, L"English");
            AppendMenuW(hLangSub, MF_STRING | (Config::g_configuredLanguage == AppLanguage::Russian ? MF_CHECKED : 0), ID_LANG_RU, L"\x0420\x0443\x0441\x0441\x043A\x0438\x0439");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hLangSub, langTitle);

            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, exitText);

            SetForegroundWindow(hwnd);
            int clicked = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, curPoint.x, curPoint.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);

            if (clicked == ID_TRAY_AUTOSTART) {
                Config::SetAutostart(!autostart);
            } else if (clicked == ID_TRAY_OPEN_CONFIG) {
                ShellExecuteW(nullptr, L"open", Config::GetConfigPath().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            } else if (clicked == ID_LANG_AUTO) {
                Config::SetLanguage(AppLanguage::Auto);
                MenuWindow::ReloadPinnedData();
            } else if (clicked == ID_LANG_EN) {
                Config::SetLanguage(AppLanguage::English);
                MenuWindow::ReloadPinnedData();
            } else if (clicked == ID_LANG_RU) {
                Config::SetLanguage(AppLanguage::Russian);
                MenuWindow::ReloadPinnedData();
            } else if (clicked == ID_TRAY_EXIT) {
                PostQuitMessage(0);
            }
        } else if (lParam == WM_LBUTTONUP) {
            MenuWindow::Toggle(!MenuWindow::g_isVisible);
        }
    }
    return CallWindowProcW((WNDPROC)GetWindowLongPtrW(hwnd, GWLP_USERDATA), hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Allow non-elevated Explorer context menu invocations to post messages through UIPI
    typedef BOOL(WINAPI* PFN_ChangeWindowMessageFilter)(UINT, DWORD);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        auto pChangeFilter = (PFN_ChangeWindowMessageFilter)GetProcAddress(hUser32, "ChangeWindowMessageFilter");
        if (pChangeFilter) {
            pChangeFilter(WM_APP_INDEX_READY, MSGFLT_ADD);
            pChangeFilter(WM_APP_TOGGLE_MENU, MSGFLT_ADD);
            pChangeFilter(WM_PAINT, MSGFLT_ADD);
        }
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc >= 3 && _wcsicmp(argv[1], L"--pin") == 0) {
        std::wstring targetPath = argv[2];
        LocalFree(argv);

        // Strip quotes if present
        if (targetPath.length() >= 2 && targetPath.front() == L'"' && targetPath.back() == L'"') {
            targetPath = targetPath.substr(1, targetPath.length() - 2);
        }

        // Clean trailing slashes
        while (targetPath.length() > 3 && (targetPath.back() == L'\\' || targetPath.back() == L'/')) {
            targetPath.pop_back();
        }

        // Silently remove Zone.Identifier alternate stream so the file never triggers execution warnings
        std::wstring zoneStream = targetPath + L":Zone.Identifier";
        DeleteFileW(zoneStream.c_str());

        size_t lastSlash = targetPath.find_last_of(L"\\/");
        std::wstring name = (lastSlash != std::wstring::npos) ? targetPath.substr(lastSlash + 1) : targetPath;

        DWORD attr = GetFileAttributesW(targetPath.c_str());
        bool isDirectory = (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));

        if (!isDirectory) {
            size_t dotPos = name.find_last_of(L'.');
            if (dotPos != std::wstring::npos) {
                name = name.substr(0, dotPos);
            }
        }

        if (name.empty()) name = targetPath;

        Config::SaveApp(name, targetPath);

        HWND existingWnd = FindWindowW(L"Perdanga11WindowClass", nullptr);
        if (existingWnd) {
            PostMessageW(existingWnd, WM_APP_INDEX_READY, 0, 0);
            PostMessageW(existingWnd, WM_PAINT, 0, 0);
        }
        return 0;
    }
    if (argv) LocalFree(argv);

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Perdanga11_SingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existingWnd = FindWindowW(L"Perdanga11WindowClass", nullptr);
        if (existingWnd) {
            PostMessageW(existingWnd, WM_APP_TOGGLE_MENU, 0, 0);
        }
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Register correct shell context menu entries
    Config::RegisterShellContextMenu();

    HWND hMainWnd = MenuWindow::Create(hInstance);

    Config::StartAsyncIndexing(hMainWnd);

    // Initialize notification tray icon with embedded application icon
    ZeroMemory(&g_nid, sizeof(NOTIFYICONDATAW));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hMainWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(g_nid.szTip, L"Perdanga11");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    LONG_PTR prevProc = SetWindowLongPtrW(hMainWnd, GWLP_WNDPROC, (LONG_PTR)SubclassedTrayProc);
    SetWindowLongPtrW(hMainWnd, GWLP_USERDATA, prevProc);

    Hooks::Install(hMainWnd, hInstance);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Hooks::Uninstall();
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (hMutex) CloseHandle(hMutex);
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();

    return 0;
}