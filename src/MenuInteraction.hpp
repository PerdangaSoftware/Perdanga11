#pragma once
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <shlobj.h>
#include <powrprof.h>
#include <dwmapi.h>
#include <vector>
#include <string>
#include "Config.hpp"
#include "AppIndexer.hpp"

#define ID_MENU_PIN_ACTION 3000
#define ID_MENU_RUN_ADMIN 3001
#define ID_MENU_OPEN_LOCATION 3002
#define ID_MENU_PIN_CUSTOM_FILE 3004
#define ID_MENU_PIN_CUSTOM_DIR 3005

#define ID_PIN_TO_TAB_BASE 3100
#define ID_SETTINGS_ADD_TAB 6500
#define ID_SETTINGS_RENAME_CURRENT 6501
#define ID_SETTINGS_DELETE_CURRENT 6502

#define ID_LANG_AUTO 6601
#define ID_LANG_EN   6602
#define ID_LANG_RU   6603

class MenuInteraction {
public:
    static inline bool g_isPowerMenuOpen = false;
    static inline int g_powerMenuHoverIndex = -1;
    static inline bool g_isModalDialogOpen = false;

    static void ApplyMenuTheme() {
        typedef enum PreferredAppMode {
            Default,
            AllowDark,
            ForceDark,
            ForceLight,
            Max
        } PreferredAppMode;

        using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode);
        HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hUxtheme) {
            auto SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
            if (SetPreferredAppMode) {
                SetPreferredAppMode(Config::IsDarkMode() ? ForceDark : ForceLight);
            }
            FreeLibrary(hUxtheme);
        }
    }

    static bool EnableShutdownPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tkp;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
            tkp.PrivilegeCount = 1;
            tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, 0);
            CloseHandle(hToken);
            return true;
        }
        return false;
    }

    static void DoLock() {
        LockWorkStation();
    }

    static void DoSleep() {
        if (!SetSuspendState(FALSE, TRUE, FALSE)) {
            ShellExecuteW(nullptr, L"open", L"rundll32.exe", L"powrprof.dll,SetSuspendState 0,1,0", nullptr, SW_HIDE);
        }
    }

    static void DoShutdown() {
        EnableShutdownPrivilege();
        if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_POWEROFF | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE)) {
            ShellExecuteW(nullptr, L"open", L"shutdown.exe", L"/s /t 0", nullptr, SW_HIDE);
        }
    }

    static void DoRestart() {
        EnableShutdownPrivilege();
        if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE)) {
            ShellExecuteW(nullptr, L"open", L"shutdown.exe", L"/r /t 0", nullptr, SW_HIDE);
        }
    }

    static void ExecuteApp(const AppItem& item, bool asAdmin) {
        if (item.isDirectory) {
            if (!Config::DoesFolderExist(item.target)) {
                const wchar_t* msg = Config::IsRussian()
                    ? L"\x0412\x044B\x0431\x0440\x0430\x043D\x043D\x0430\x044F \x043F\x0430\x043F\x043A\x0430 \x043D\x0435 \x0441\x0443\x0449\x0435\x0441\x0442\x0432\x0443\x0435\x0442 \x0438\x043B\x0438 \x043D\x0435\x0434\x043E\x0441\x0442\x0443\x043F\x043D\x0430."
                    : L"The selected folder does not exist or is unavailable.";
                MessageBoxW(nullptr, msg, L"Perdanga11", MB_OK | MB_ICONWARNING);
                return;
            }
        }
        ShellExecuteW(nullptr, asAdmin ? L"runas" : L"open", item.target.c_str(), item.arguments.c_str(), nullptr, SW_SHOWNORMAL);
    }

    // Opens the actual directory where the real binary is installed, resolving .lnk shortcuts
    static void OpenItemLocation(const AppItem& item) {
        std::wstring target = item.target;
        if (target.length() >= 2 && target.front() == L'"' && target.back() == L'"') {
            target = target.substr(1, target.length() - 2);
        }

        // If it is a shortcut (.lnk), resolve to the actual target file/binary
        size_t dotPos = target.find_last_of(L'.');
        if (dotPos != std::wstring::npos && _wcsicmp(target.c_str() + dotPos, L".lnk") == 0) {
            std::wstring iconPath;
            int iconIdx = 0;
            std::wstring resolved = AppIndexer::ResolveLnkTarget(target, iconPath, iconIdx);
            if (!resolved.empty() && resolved != target) {
                target = resolved;
            }
        }

        target = Config::ResolveAppPath(target);

        // Open Explorer and select the real installed program file
        std::wstring param = L"/select,\"" + target + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
    }

    static bool PromptAddFile(HWND hWnd, std::wstring& outName, std::wstring& outPath) {
        g_isModalDialogOpen = true;

        wchar_t filePath[MAX_PATH] = { 0 };
        OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = Config::IsRussian() ? L"\x041F\x043E\x0438\x0441\x043A \x043F\x0440\x043E\x0433\x0440\x0430\x043C\x043C \x0438 \x0444\x0430\x0439\x043B\x043E\x0432 (*.*)\0*.*\0" : L"Applications & Files (*.*)\0*.*\0";
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

        bool ok = false;
        if (GetOpenFileNameW(&ofn)) {
            std::wstring fullPath(filePath);
            size_t lastSlash = fullPath.find_last_of(L"\\/");
            std::wstring fileName = (lastSlash != std::wstring::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
            size_t dotPos = fileName.find_last_of(L'.');
            outName = (dotPos != std::wstring::npos) ? fileName.substr(0, dotPos) : fileName;
            outPath = fullPath;
            ok = true;
        }

        g_isModalDialogOpen = false;
        SetForegroundWindow(hWnd);
        return ok;
    }

    static bool PromptAddFolder(HWND hWnd, std::wstring& outName, std::wstring& outPath) {
        g_isModalDialogOpen = true;
        bool ok = false;

        IFileDialog* pfd = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
            DWORD dwOptions;
            if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
            }
            if (SUCCEEDED(pfd->Show(hWnd))) {
                IShellItem* psi = nullptr;
                if (SUCCEEDED(pfd->GetResult(&psi)) && psi) {
                    PWSTR pszPath = nullptr;
                    if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                        outPath = pszPath;
                        size_t lastSlash = outPath.find_last_of(L"\\/");
                        outName = (lastSlash != std::wstring::npos) ? outPath.substr(lastSlash + 1) : outPath;
                        if (outName.empty()) outName = outPath;
                        CoTaskMemFree(pszPath);
                        ok = true;
                    }
                    psi->Release();
                }
            }
            pfd->Release();
        }

        g_isModalDialogOpen = false;
        SetForegroundWindow(hWnd);
        return ok;
    }

    static bool PromptTabNameDialog(HWND hParent, const wchar_t* title, std::wstring& inOutName) {
        g_isModalDialogOpen = true;

        struct DlgState {
            std::wstring name;
            bool accepted = false;
            HWND hEdit = nullptr;
            HWND hBtnSave = nullptr;
            HWND hBtnCancel = nullptr;
            HBRUSH hbrBg = nullptr;
            HBRUSH hbrEdit = nullptr;
            COLORREF clrBg = 0;
            COLORREF clrText = 0;
            COLORREF clrEdit = 0;
            COLORREF clrBorder = 0;
            HFONT hFontMain = nullptr;
            HFONT hFontBold = nullptr;
        };

        DlgState state;
        state.name = inOutName;

        bool isDark = Config::IsDarkMode();
        state.clrBg = isDark ? RGB(32, 32, 32) : RGB(243, 243, 246);
        state.clrText = isDark ? RGB(255, 255, 255) : RGB(25, 25, 25);
        state.clrEdit = isDark ? RGB(42, 42, 42) : RGB(255, 255, 255);
        state.clrBorder = isDark ? RGB(60, 60, 60) : RGB(200, 200, 200);

        state.hbrBg = CreateSolidBrush(state.clrBg);
        state.hbrEdit = CreateSolidBrush(state.clrEdit);

        state.hFontMain = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

        state.hFontBold = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

        static bool s_classRegistered = false;
        if (!s_classRegistered) {
            WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
            wc.lpfnWndProc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                DlgState* pState = (DlgState*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
                switch (msg) {
                    case WM_CREATE: {
                        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
                        pState = (DlgState*)cs->lpCreateParams;
                        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pState);

                        const wchar_t* lblText = Config::IsRussian() ? L"\x041D\x0430\x0437\x0432\x0430\x043D\x0438\x0435 \x0432\x043A\x043B\x0430\x0434\x043A\x0438:" : L"Tab Name:";
                        HWND hLbl = CreateWindowExW(0, L"STATIC", lblText, WS_CHILD | WS_VISIBLE, 24, 18, 280, 20, hWnd, nullptr, nullptr, nullptr);
                        SendMessageW(hLbl, WM_SETFONT, (WPARAM)pState->hFontBold, TRUE);

                        pState->hEdit = CreateWindowExW(0, L"EDIT", pState->name.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 24, 44, 308, 28, hWnd, (HMENU)101, nullptr, nullptr);
                        SendMessageW(pState->hEdit, WM_SETFONT, (WPARAM)pState->hFontMain, TRUE);
                        SendMessageW(pState->hEdit, EM_SETSEL, 0, -1);

                        int curY = 88;
                        const wchar_t* saveText = Config::IsRussian() ? L"\x0421\x043E\x0445\x0440\x0430\x043D\x0438\x0442\x044C" : L"Save";
                        const wchar_t* cancelText = Config::IsRussian() ? L"\x041E\x0442\x043C\x0435\x043D\x0430" : L"Cancel";

                        pState->hBtnSave = CreateWindowExW(0, L"BUTTON", saveText, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 148, curY, 88, 30, hWnd, (HMENU)IDOK, nullptr, nullptr);
                        pState->hBtnCancel = CreateWindowExW(0, L"BUTTON", cancelText, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 244, curY, 88, 30, hWnd, (HMENU)IDCANCEL, nullptr, nullptr);

                        SetFocus(pState->hEdit);
                        return 0;
                    }
                    case WM_ERASEBKGND: {
                        HDC hdc = (HDC)wParam;
                        RECT rc;
                        GetClientRect(hWnd, &rc);
                        FillRect(hdc, &rc, pState->hbrBg);
                        return 1;
                    }
                    case WM_CTLCOLORSTATIC: {
                        HDC hdc = (HDC)wParam;
                        SetTextColor(hdc, pState->clrText);
                        SetBkColor(hdc, pState->clrBg);
                        return (LRESULT)pState->hbrBg;
                    }
                    case WM_CTLCOLOREDIT: {
                        HDC hdc = (HDC)wParam;
                        SetTextColor(hdc, pState->clrText);
                        SetBkColor(hdc, pState->clrEdit);
                        return (LRESULT)pState->hbrEdit;
                    }
                    case WM_DRAWITEM: {
                        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
                        HDC hdc = dis->hDC;
                        RECT rc = dis->rcItem;

                        if (dis->CtlID == IDOK) {
                            bool isDown = (dis->itemState & ODS_SELECTED);
                            HBRUSH hBr = CreateSolidBrush(isDown ? RGB(0, 90, 170) : RGB(0, 103, 192));
                            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 120, 215));
                            HGDIOBJ oldBr = SelectObject(hdc, hBr);
                            HGDIOBJ oldPen = SelectObject(hdc, hPen);
                            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
                            SelectObject(hdc, oldBr);
                            SelectObject(hdc, oldPen);
                            DeleteObject(hBr);
                            DeleteObject(hPen);

                            SetTextColor(hdc, RGB(255, 255, 255));
                            SetBkMode(hdc, TRANSPARENT);
                            SelectObject(hdc, pState->hFontBold);
                            const wchar_t* str = Config::IsRussian() ? L"\x0421\x043E\x0445\x0440\x0430\x043D\x0438\x0442\x044C" : L"Save";
                            DrawTextW(hdc, str, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                            return TRUE;
                        }

                        if (dis->CtlID == IDCANCEL) {
                            bool isDown = (dis->itemState & ODS_SELECTED);
                            HBRUSH hBr = CreateSolidBrush(isDown ? RGB(60, 60, 60) : (Config::IsDarkMode() ? RGB(45, 45, 45) : RGB(225, 225, 225)));
                            HPEN hPen = CreatePen(PS_SOLID, 1, pState->clrBorder);
                            HGDIOBJ oldBr = SelectObject(hdc, hBr);
                            HGDIOBJ oldPen = SelectObject(hdc, hPen);
                            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
                            SelectObject(hdc, oldBr);
                            SelectObject(hdc, oldPen);
                            DeleteObject(hBr);
                            DeleteObject(hPen);

                            SetTextColor(hdc, pState->clrText);
                            SetBkMode(hdc, TRANSPARENT);
                            SelectObject(hdc, pState->hFontMain);
                            const wchar_t* str = Config::IsRussian() ? L"\x041E\x0442\x043C\x0435\x043D\x0430" : L"Cancel";
                            DrawTextW(hdc, str, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                            return TRUE;
                        }

                        return FALSE;
                    }
                    case WM_COMMAND: {
                        int id = LOWORD(wParam);
                        if (id == IDOK) {
                            wchar_t buf[256] = { 0 };
                            GetWindowTextW(pState->hEdit, buf, 256);
                            pState->name = buf;
                            pState->accepted = !pState->name.empty();
                            DestroyWindow(hWnd);
                            return 0;
                        }
                        if (id == IDCANCEL) {
                            pState->accepted = false;
                            DestroyWindow(hWnd);
                            return 0;
                        }
                        break;
                    }
                    case WM_DESTROY: {
                        DeleteObject(pState->hbrBg);
                        DeleteObject(pState->hbrEdit);
                        DeleteObject(pState->hFontMain);
                        DeleteObject(pState->hFontBold);
                        return 0;
                    }
                    case WM_CLOSE: {
                        pState->accepted = false;
                        DestroyWindow(hWnd);
                        return 0;
                    }
                }
                return DefWindowProcW(hWnd, msg, wParam, lParam);
            };
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = L"PerdangaCleanModalDialog";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassExW(&wc);
            s_classRegistered = true;
        }

        int dlgW = 368;
        int dlgH = 162;

        RECT parentRect;
        GetWindowRect(hParent, &parentRect);
        int dlgX = parentRect.left + (parentRect.right - parentRect.left - dlgW) / 2;
        int dlgY = parentRect.top + (parentRect.bottom - parentRect.top - dlgH) / 2;

        HWND hDlg = CreateWindowExW(
            WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
            L"PerdangaCleanModalDialog", title,
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            dlgX, dlgY, dlgW, dlgH,
            hParent, nullptr, GetModuleHandle(nullptr), &state
        );

        BOOL bDark = isDark;
        DwmSetWindowAttribute(hDlg, DWMWA_USE_IMMERSIVE_DARK_MODE, &bDark, sizeof(bDark));
        DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hDlg, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        COLORREF bdr = state.clrBorder;
        DwmSetWindowAttribute(hDlg, DWMWA_BORDER_COLOR, &bdr, sizeof(bdr));

        MSG msg;
        while (IsWindow(hDlg)) {
            if (!GetMessageW(&msg, nullptr, 0, 0)) {
                // Do not swallow WM_QUIT: repost it so the main loop can exit
                PostQuitMessage((int)msg.wParam);
                break;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                DestroyWindow(hDlg);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        g_isModalDialogOpen = false;
        SetForegroundWindow(hParent);

        if (state.accepted) {
            inOutName = state.name;
            return true;
        }
        return false;
    }

    static int ShowItemContextMenu(HWND hWnd, POINT pt, const AppItem& targetItem, int currentActiveTabIndex) {
        g_isModalDialogOpen = true;
        ApplyMenuTheme();

        HMENU hMenu = CreatePopupMenu();

        if (currentActiveTabIndex >= 0 && currentActiveTabIndex < (int)Config::g_tabs.size()) {
            const auto& activeTab = Config::g_tabs[currentActiveTabIndex];
            bool isPinnedInActive = Config::IsAppPinned(targetItem.name, activeTab.items);

            std::wstring activeLabel;
            if (activeTab.id == L"pinned") {
                activeLabel = isPinnedInActive
                    ? (Config::IsRussian() ? L"\x041E\x0442\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x043E\x0442 \x043D\x0430\x0447\x0430\x043B\x044C\x043D\x043E\x0433\x043E \x044D\x043A\x0440\x0430\x043D\x0430" : L"Unpin from Start")
                    : (Config::IsRussian() ? L"\x0417\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x043D\x0430 \x043D\x0430\x0447\x0430\x043B\x044C\x043D\x043E\x0433\x043E \x044D\x043A\x0440\x0430\x043D\x0435" : L"Pin to Start");
            } else {
                std::wstring tabName = Config::GetTabDisplayName(activeTab);
                activeLabel = isPinnedInActive
                    ? (Config::IsRussian() ? (L"\x0423\x0431\x0440\x0430\x0442\x044C \x0438\x0437 \x0432\x043A\x043B\x0430\x0434\x043A\x0438 \x00AB" + tabName + L"\x00BB") : (L"Remove from \"" + tabName + L"\""))
                    : (Config::IsRussian() ? (L"\x0414\x043E\x0431\x0430\x0432\x0438\x0442\x044C \x0432\x043E \x0432\x043A\x043B\x0430\x0434\x043A\x0443 \x00AB" + tabName + L"\x00BB") : (L"Add to \"" + tabName + L"\""));
            }
            AppendMenuW(hMenu, MF_STRING, ID_MENU_PIN_ACTION, activeLabel.c_str());
        }

        HMENU hSubTabs = CreatePopupMenu();
        for (size_t i = 0; i < Config::g_tabs.size(); ++i) {
            bool pinnedInThisTab = Config::IsAppPinned(targetItem.name, Config::g_tabs[i].items);
            UINT flags = MF_STRING;
            if (pinnedInThisTab) flags |= MF_CHECKED;

            std::wstring itemText = Config::GetTabDisplayName(Config::g_tabs[i]);
            AppendMenuW(hSubTabs, flags, ID_PIN_TO_TAB_BASE + (UINT)i, itemText.c_str());
        }
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubTabs, Config::IsRussian() ? L"\x0417\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x0432\x043E \x0432\x043A\x043B\x0430\x0434\x043A\x0443" : L"Pin to tab");

        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        if (!targetItem.isDirectory) {
            const wchar_t* adminText = Config::IsRussian() ? L"\x0417\x0430\x043F\x0443\x0441\x043A \x043E\x0442 \x0438\x043C\x0435\x043D\x0438 \x0430\x0434\x043C\x0438\x043D\x0438\x0441\x0442\x0440\x0430\x0442\x043E\x0440\x0430" : L"Run as administrator";
            AppendMenuW(hMenu, MF_STRING, ID_MENU_RUN_ADMIN, adminText);
        }
        const wchar_t* locText = Config::IsRussian() ? L"\x041F\x0435\x0440\x0435\x0439\x0442\x0438 \x043A \x0440\x0430\x0441\x043F\x043E\x043B\x043E\x0436\x0435\x043D\x0438\x044E" : L"Open file location";
        AppendMenuW(hMenu, MF_STRING, ID_MENU_OPEN_LOCATION, locText);

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, nullptr);
        DestroyMenu(hMenu);

        g_isModalDialogOpen = false;
        return cmd;
    }

    static int ShowBackgroundContextMenu(HWND hWnd, POINT pt) {
        g_isModalDialogOpen = true;
        ApplyMenuTheme();

        HMENU hMenu = CreatePopupMenu();
        const wchar_t* addAppText = Config::IsRussian() ? L"\x0417\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x043F\x0440\x043E\x0433\x0440\x0430\x043C\x043C\x0443/\x0444\x0430\x0439\x043B..." : L"Pin application/file...";
        const wchar_t* addFolderText = Config::IsRussian() ? L"\x0417\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x043F\x0430\x043F\x043A\x0433..." : L"Pin folder...";

        AppendMenuW(hMenu, MF_STRING, ID_MENU_PIN_CUSTOM_FILE, addAppText);
        AppendMenuW(hMenu, MF_STRING, ID_MENU_PIN_CUSTOM_DIR, addFolderText);

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, nullptr);
        DestroyMenu(hMenu);

        g_isModalDialogOpen = false;
        return cmd;
    }

    static int ShowTabHeaderContextMenu(HWND hWnd, POINT pt, int tabIndex) {
        g_isModalDialogOpen = true;
        ApplyMenuTheme();

        HMENU hMenu = CreatePopupMenu();
        if (tabIndex >= 0 && tabIndex < (int)Config::g_tabs.size()) {
            std::wstring tabName = Config::GetTabDisplayName(Config::g_tabs[tabIndex]);
            std::wstring renText = Config::IsRussian() ? (L"\x041F\x0435\x0440\x0435\x0438\x043C\x0435\x043D\x043E\x0432\x0430\x0442\x044C \x00AB" + tabName + L"\x00BB...") : (L"Rename \"" + tabName + L"\"...");
            const wchar_t* delText = Config::IsRussian() ? L"\x0423\x0434\x0430\x043B\x0438\x0442\x044C \x0432\x043A\x043B\x0430\x0434\x043A\x0443" : L"Delete Tab";

            AppendMenuW(hMenu, MF_STRING, ID_SETTINGS_RENAME_CURRENT, renText.c_str());
            if (Config::g_tabs.size() > 1) {
                AppendMenuW(hMenu, MF_STRING, ID_SETTINGS_DELETE_CURRENT, delText);
            }
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        }

        UINT addFlags = MF_STRING;
        if (!Config::CanAddTab()) addFlags |= MF_GRAYED;
        const wchar_t* addTabText = Config::IsRussian() ? L"+ \x0414\x043E\x0431\x0430\x0432\x0438\x0442\x044C \x0432\x043A\x043B\x0430\x0434\x043A\x0443..." : L"+ Add New Tab...";
        AppendMenuW(hMenu, addFlags, ID_SETTINGS_ADD_TAB, addTabText);

        HMENU hLangMenu = CreatePopupMenu();
        AppendMenuW(hLangMenu, MF_STRING | (Config::g_configuredLanguage == AppLanguage::Auto ? MF_CHECKED : 0), ID_LANG_AUTO, Config::IsRussian() ? L"\x0410\x0432\x0442\x043E (\x0441\x0438\x0441\x0442\x0435\x043C\x043D\x044B\x0439)" : L"Auto (System)");
        AppendMenuW(hLangMenu, MF_STRING | (Config::g_configuredLanguage == AppLanguage::English ? MF_CHECKED : 0), ID_LANG_EN, L"English");
        AppendMenuW(hLangMenu, MF_STRING | (Config::g_configuredLanguage == AppLanguage::Russian ? MF_CHECKED : 0), ID_LANG_RU, L"\x0420\x0443\x0441\x0441\x043A\x0438\x0439");

        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        const wchar_t* langTitle = Config::IsRussian() ? L"\x042F\x0437\x044B\x043A" : L"Language";
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hLangMenu, langTitle);

        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, nullptr);
        DestroyMenu(hMenu);

        g_isModalDialogOpen = false;
        return cmd;
    }
};