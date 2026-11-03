#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ole2.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <powrprof.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <atomic>
#include "Config.hpp"
#include "AppIndexer.hpp"
#include "Hooks.hpp"
#include "MenuRenderer.hpp"
#include "MenuInteraction.hpp"

#define ID_SEARCH_BOX           4001
#define TIMER_ANIMATION         5002
#define TIMER_UPDATE_RECT       5003
#define TIMER_SEARCH_DEBOUNCE   5004
#define TIMER_HOVER_TOOLTIP     5005
#define TIMER_LAUNCH_FLASH      5006
#define TIMER_RESET_DEACTIVATE  5007
#define TIMER_RECT_DEBOUNCE     5008
#define TIMER_BLOCK_START       5009
#define TIMER_INIT_TASKBAR_POLL 5010
#define TIMER_NATIVE_START_WATCHDOG 5011

enum MenuViewMode {
    MODE_TABS = 0,
    MODE_ALL_APPS = 1
};

class MenuState {
public:
    static inline HWND g_hWnd = nullptr;
    static inline HWND g_hSearchEdit = nullptr;
    static inline bool g_isVisible = false;
    static inline bool g_isAnimating = false;
    static inline bool g_animatingIn = false;
    static inline DWORD g_openTimestamp = 0;

    static inline MenuViewMode g_currentMode = MODE_TABS;
    static inline DWORD g_animStartTime = 0;
    static inline int g_targetX = 0;
    static inline int g_targetY = 0;
    static inline int g_animOffsetX = 0;
    static inline int g_animOffsetY = 20;

    static inline int g_justLaunchedIndex = -1;
    static inline DWORD g_justLaunchedTime = 0;
    static inline bool g_suppressCloseOnDeactivate = false;

    static const inline int MENU_WIDTH = 740;
    static const inline int MENU_HEIGHT = 720;
    static const inline int ANIM_DURATION = 190;

    static const inline int GRID_COLS = 6;
    static const inline int CELL_WIDTH = 106;
    static const inline int CELL_HEIGHT = 98;
    static const inline int GRID_START_X = 52;
    static const inline int GRID_START_Y = 112;
    static const inline int VIEWPORT_HEIGHT = 540;

    static const inline int TABLE_START_X = 32;
    static const inline int TABLE_END_X = 708;
    static const inline int TABLE_HEADER_Y = 66;
    static const inline int TABLE_HEADER_HEIGHT = 28;
    static const inline int TABLE_ROW_START_Y = 98;
    static const inline int TABLE_ROW_HEIGHT = 34;
    static const inline int TABLE_VIEWPORT_HEIGHT = 554;
    static inline bool g_isSearchMode = false;

    static inline int g_scrollY = 0;
    static inline int g_maxScrollY = 0;
    static inline bool g_isScrollbarDragging = false;
    static inline bool g_isScrollbarHovered = false;
    static inline int g_scrollDragStartY = 0;
    static inline int g_scrollDragStartScrollY = 0;

    static float GetScrollTrackHeight() {
        return (float)(g_isSearchMode ? TABLE_VIEWPORT_HEIGHT : VIEWPORT_HEIGHT);
    }

    static float GetScrollThumbHeight() {
        if (g_maxScrollY <= 0) return 0.0f;
        float trackH = GetScrollTrackHeight();
        return (std::max)(32.0f, trackH * trackH / (trackH + (float)g_maxScrollY));
    }

    static float GetScrollThumbY() {
        int startY = g_isSearchMode ? TABLE_ROW_START_Y : GRID_START_Y;
        if (g_maxScrollY <= 0) return (float)startY;
        float trackH = GetScrollTrackHeight();
        float thumbH = GetScrollThumbHeight();
        return (float)startY + ((float)g_scrollY / (float)g_maxScrollY) * (trackH - thumbH);
    }

    static inline int g_tabScrollX = 0;
    static inline int g_maxTabScrollX = 0;
    static inline bool g_hasTabOverflow = false;
    static inline int g_tabClipLeft = 52;
    static inline int g_tabClipRight = 520;

    static inline RECT g_scrollLeftRect = { 0, 0, 0, 0 };
    static inline RECT g_scrollRightRect = { 0, 0, 0, 0 };
    static inline bool g_scrollLeftHovered = false;
    static inline bool g_scrollRightHovered = false;

    static inline RECT g_addTabRect = { 0, 0, 0, 0 };
    static inline bool g_addTabHovered = false;

    static inline RECT g_toggleViewRect = { 0, 0, 0, 0 };
    static inline bool g_toggleViewHovered = false;

    static inline RECT g_powerButtonRect = { 0, 0, 0, 0 };
    static inline RECT g_settingsButtonRect = { 0, 0, 0, 0 };
    static inline RECT g_explorerButtonRect = { 0, 0, 0, 0 };
    static inline RECT g_downloadsButtonRect = { 0, 0, 0, 0 };
    static inline RECT g_powerFlyoutRect = { 0, 0, 0, 0 };

    static inline bool g_powerHovered = false;
    static inline bool g_settingsHovered = false;
    static inline bool g_explorerHovered = false;
    static inline bool g_downloadsHovered = false;

    static inline int g_dragPotentialIndex = -1;
    static inline int g_dragItemIndex = -1;
    static inline int g_dragTargetIndex = -1;
    static inline bool g_isDragging = false;
    static inline POINT g_dragStartPt = { 0, 0 };
    static inline POINT g_dragCurrentPt = { 0, 0 };

    static inline HFONT g_hFontSearch = nullptr;
    static inline int g_hoveredIndex = -1;
    static inline int g_contextItemIndex = -1;

    static inline bool g_showTooltip = false;
    static inline int g_tooltipItemIndex = -1;

    static inline std::vector<AppItem> g_displayItems;
    static inline std::vector<AppItem> g_searchApps;
    static inline std::vector<SearchResultEntry> g_searchResults;
    static inline std::atomic<uint64_t> g_searchGeneration{ 0 };

    static bool GetSearchRowTooltipInfo(int rowIndex,
                                        std::wstring& outName,
                                        std::wstring& outPath,
                                        std::wstring& outSize,
                                        std::wstring& outDate,
                                        HICON& outIcon,
                                        bool& outIsDir,
                                        std::wstring& outTarget,
                                        std::wstring* outFullDate) {
        if (rowIndex < 0 || rowIndex >= (int)g_searchResults.size()) return false;
        const auto& entry = g_searchResults[rowIndex];

        if (entry.source == 0 || entry.source == 1) {
            if (entry.sourceIndex >= g_searchApps.size()) return false;
            const auto& app = g_searchApps[entry.sourceIndex];
            outName = app.name;
            outPath = app.displayPath;
            outSize = app.sizeStr;
            outDate = (app.dateStr.length() >= 10) ? app.dateStr.substr(0, 10) : app.dateStr;
            outIcon = app.hIcon;
            outIsDir = app.isDirectory;
            outTarget = app.target;
            if (outFullDate) *outFullDate = app.dateStr;
            return true;
        }

        std::shared_ptr<const IndexSnapshot> snap;
        {
            std::lock_guard<std::mutex> lock(Config::g_indexMutex);
            snap = Config::g_indexSnapshot;
        }
        if (!snap || entry.sourceIndex >= snap->userFiles.size()) return false;

        const auto& cf = snap->userFiles[entry.sourceIndex];
        outName = cf.name;
        outIsDir = cf.isDirectory;

        if (cf.dirId < snap->dirPool.size()) {
            outPath = snap->dirPool[cf.dirId];
        } else {
            outPath = L"";
        }

        if (outPath.empty()) {
            outTarget = cf.name;
        } else {
            outTarget = outPath;
            if (outTarget.back() != L'\\') outTarget += L'\\';
            outTarget += cf.name;
        }

        if (cf.isDirectory) {
            outIcon = IconCache::GetDefaultFolderIcon();
            outSize = L"";
        } else {
            size_t dotPos = cf.name.find_last_of(L'.');
            std::wstring ext = (dotPos != std::wstring::npos) ? Config::ToLower(cf.name.substr(dotPos)) : L"";
            outIcon = IconCache::GetExtensionIcon(ext);
            wchar_t szBuf[32];
            Config::FormatFileSize(cf.fileSize, szBuf, 32);
            outSize = szBuf;
        }

        wchar_t dtBuf[32];
        Config::FormatFileTime(cf.lastWriteTime, dtBuf, 32, false);
        outDate = dtBuf;
        if (outFullDate) {
            wchar_t fullBuf[32];
            Config::FormatFileTime(cf.lastWriteTime, fullBuf, 32, true);
            *outFullDate = fullBuf;
        }
        return true;
    }

    static bool GetSearchRowDisplayInfo(int rowIndex,
                                        std::wstring& outName,
                                        std::wstring& outPath,
                                        std::wstring& outSize,
                                        std::wstring& outDate,
                                        HICON& outIcon,
                                        bool& outIsDir,
                                        std::wstring& outTarget) {
        return GetSearchRowTooltipInfo(rowIndex, outName, outPath, outSize, outDate, outIcon, outIsDir, outTarget, nullptr);
    }

    static AppItem GetSearchRowAppItem(int rowIndex) {
        AppItem item;
        std::wstring name, path, sizeStr, dateStr, target;
        HICON hIcon = nullptr;
        bool isDir = false;
        if (GetSearchRowDisplayInfo(rowIndex, name, path, sizeStr, dateStr, hIcon, isDir, target)) {
            item.name = name;
            item.target = target;
            item.displayPath = path;
            item.sizeStr = sizeStr;
            item.dateStr = dateStr;
            item.hIcon = hIcon;
            item.isDirectory = isDir;
            item.lowerName = Config::ToLower(name);
            item.lowerTarget = Config::ToLower(target);
        }
        return item;
    }

    static void CloseImmediately() {
        if (!g_isVisible && !g_isAnimating) return;
        if (g_isScrollbarDragging) {
            g_isScrollbarDragging = false;
            ReleaseCapture();
        }
        g_isScrollbarHovered = false;
        KillTimer(g_hWnd, TIMER_ANIMATION);
        KillTimer(g_hWnd, TIMER_SEARCH_DEBOUNCE);
        KillTimer(g_hWnd, TIMER_HOVER_TOOLTIP);
        KillTimer(g_hWnd, TIMER_LAUNCH_FLASH);
        KillTimer(g_hWnd, TIMER_RESET_DEACTIVATE);
        g_isAnimating = false;
        g_isDragging = false;
        g_dragPotentialIndex = -1;
        g_hoveredIndex = -1;
        g_showTooltip = false;
        g_tooltipItemIndex = -1;
        g_justLaunchedIndex = -1;
        g_suppressCloseOnDeactivate = false;
        MenuInteraction::g_isPowerMenuOpen = false;
        ShowWindow(g_hWnd, SW_HIDE);
        SetLayeredWindowAttributes(g_hWnd, 0, 0, LWA_ALPHA);
        g_isVisible = false;
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    }

    static void EnsureHoverVisible() {
        if (g_hoveredIndex < 0) return;
        if (g_isSearchMode) {
            int rowTop = g_hoveredIndex * TABLE_ROW_HEIGHT;
            int rowBottom = rowTop + TABLE_ROW_HEIGHT;
            if (rowTop < g_scrollY) {
                g_scrollY = rowTop;
            } else if (rowBottom > g_scrollY + TABLE_VIEWPORT_HEIGHT) {
                g_scrollY = rowBottom - TABLE_VIEWPORT_HEIGHT;
            }
            g_scrollY = (std::max)(0, (std::min)(g_maxScrollY, g_scrollY));
        } else {
            int row = g_hoveredIndex / GRID_COLS;
            int tileTop = row * CELL_HEIGHT;
            int tileBottom = tileTop + CELL_HEIGHT;

            if (tileTop < g_scrollY) {
                g_scrollY = tileTop;
            } else if (tileBottom > g_scrollY + VIEWPORT_HEIGHT) {
                g_scrollY = tileBottom - VIEWPORT_HEIGHT;
            }
            g_scrollY = (std::max)(0, (std::min)(g_maxScrollY, g_scrollY));
        }
    }

    static int GetItemIndexAtPoint(int x, int y) {
        if (g_isSearchMode) {
            int relY = y - TABLE_ROW_START_Y + g_scrollY;
            if (x >= TABLE_START_X && x <= TABLE_END_X && y >= TABLE_ROW_START_Y && y <= TABLE_ROW_START_Y + TABLE_VIEWPORT_HEIGHT && relY >= 0) {
                int row = relY / TABLE_ROW_HEIGHT;
                if (row >= 0 && row < (int)g_searchResults.size()) return row;
            }
            return -1;
        } else {
            int relY = y - GRID_START_Y + g_scrollY;
            if (x >= GRID_START_X && x < GRID_START_X + (GRID_COLS * CELL_WIDTH) && y >= GRID_START_Y && y <= GRID_START_Y + VIEWPORT_HEIGHT && relY >= 0) {
                int col = (x - GRID_START_X) / CELL_WIDTH;
                int row = relY / CELL_HEIGHT;
                int idx = row * GRID_COLS + col;
                if (idx >= 0 && idx < (int)g_displayItems.size()) return idx;
            }
            return -1;
        }
    }

    static bool PinAppByName(const std::wstring& appName) {
        std::wstring clean = Hooks::CleanAppName(appName);
        if (clean.empty()) clean = appName;
        if (appName.empty()) return false;
        std::wstring lower = Config::ToLower(clean);

        if (lower == L"\x043F\x0440\x043E\x0432\x043E\x0434\x043D\x0438\x043A" || lower == L"explorer" || lower == L"file explorer") {
            Config::SaveApp(clean, L"explorer.exe");
            return true;
        }
        if (lower == L"\x043D\x043E\x0436\x043D\x0438\x0446\x044B" || lower == L"snipping tool" || lower == L"snippingtool") {
            Config::SaveApp(clean, L"shell:AppsFolder\\Microsoft.ScreenSketch_8wekyb3d8bbwe!App");
            return true;
        }
        if (lower == L"\x043F\x043E\x0433\x043E\x0434\x0430" || lower == L"weather" || lower == L"msn weather" || lower == L"msn \x043F\x043E\x0433\x043E\x0434\x0430") {
            Config::SaveApp(clean, L"shell:AppsFolder\\Microsoft.BingWeather_8wekyb3d8bbwe!App");
            return true;
        }
        if (lower == L"microsoft store" || lower == L"store" || lower == L"\x043C\x0430\x0433\x0430\x0437\x0438\x043D microsoft") {
            Config::SaveApp(clean, L"shell:AppsFolder\\Microsoft.WindowsStore_8wekyb3d8bbwe!App");
            return true;
        }
        if (lower.find(L"nvidia control panel") != std::wstring::npos || lower.find(L"\x043F\x0430\x043D\x0435\x043B\x044C \x0443\x043F\x0440\x0430\x0432\x043B\x0435\x043D\x0438\x044F nvidia") != std::wstring::npos) {
            Config::SaveApp(clean, L"shell:AppsFolder\\NVIDIACorp.NVIDIAControlPanel_56jybvy8sckqj!NVIDIACorp.NVIDIAControlPanel");
            return true;
        }
        if (lower == L"блокнот" || lower == L"notepad") {
            Config::SaveApp(clean, L"notepad.exe");
            return true;
        }
        if (lower == L"калькулятор" || lower == L"calculator") {
            Config::SaveApp(clean, L"calc.exe");
            return true;
        }
        if (lower == L"командная строка" || lower == L"cmd" || lower == L"command prompt") {
            Config::SaveApp(clean, L"cmd.exe");
            return true;
        }
        if (lower == L"диспетчер задач" || lower == L"task manager" || lower == L"taskmgr") {
            Config::SaveApp(clean, L"taskmgr.exe");
            return true;
        }
        if (lower == L"панель управления" || lower == L"control panel") {
            Config::SaveApp(clean, L"control.exe");
            return true;
        }
        if (lower == L"параметры" || lower == L"settings") {
            Config::SaveApp(clean, L"ms-settings:");
            return true;
        }

        std::shared_ptr<const IndexSnapshot> snap;
        {
            std::lock_guard<std::mutex> lock(Config::g_indexMutex);
            snap = Config::g_indexSnapshot;
        }

        if (snap) {
            for (const auto& item : snap->installedApps) {
                if (item.lowerName == lower || Config::ToLower(item.name) == lower) {
                    Config::SaveApp(item.name, item.target);
                    return true;
                }
            }
            for (const auto& item : snap->installedApps) {
                if (item.lowerName.find(lower) != std::wstring::npos || lower.find(item.lowerName) != std::wstring::npos) {
                    Config::SaveApp(item.name, item.target);
                    return true;
                }
            }
        }

        IShellItem* pAppsFolder = nullptr;
        if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&pAppsFolder))) && pAppsFolder) {
            IEnumShellItems* pEnum = nullptr;
            if (SUCCEEDED(pAppsFolder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&pEnum))) && pEnum) {
                IShellItem* pItem = nullptr;
                while (pEnum->Next(1, &pItem, nullptr) == S_OK && pItem) {
                    LPWSTR name = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &name)) && name) {
                        std::wstring sName = Hooks::CleanAppName(name);
                        std::wstring lName = Config::ToLower(sName);
                        if (lName == lower || lName.find(lower) != std::wstring::npos || lower.find(lName) != std::wstring::npos) {
                            LPWSTR parseName = nullptr;
                            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parseName)) && parseName) {
                                std::wstring target = L"shell:AppsFolder\\" + std::wstring(parseName);
                                Config::SaveApp(sName, target);
                                CoTaskMemFree(parseName);
                                CoTaskMemFree(name);
                                pItem->Release();
                                pEnum->Release();
                                pAppsFolder->Release();
                                return true;
                            }
                        }
                        CoTaskMemFree(name);
                    }
                    pItem->Release();
                }
                pEnum->Release();
            }
            pAppsFolder->Release();
        }

        std::wstring resolved = Config::ResolveAppPath(clean);
        if (GetFileAttributesW(resolved.c_str()) != INVALID_FILE_ATTRIBUTES) {
            Config::SaveApp(clean, resolved);
            return true;
        }

        resolved = Config::ResolveAppPath(clean + L".exe");
        if (GetFileAttributesW(resolved.c_str()) != INVALID_FILE_ATTRIBUTES) {
            Config::SaveApp(clean, resolved);
            return true;
        }
        HKEY hKey = nullptr;
        std::wstring subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + clean + L".exe";
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS ||
            RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t regPath[MAX_PATH] = { 0 };
            DWORD regSize = sizeof(regPath);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)regPath, &regSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                Config::SaveApp(clean, regPath);
                return true;
            }
            RegCloseKey(hKey);
        }

        return false;
    }

    static bool UnpinAppByName(const std::wstring& appName) {
        if (appName.empty()) return false;
        std::wstring clean = Hooks::CleanAppName(appName);
        if (clean.empty()) clean = appName;
        std::wstring lower = Config::ToLower(clean);

        Config::DeleteApp(clean);

        if (lower == L"\x043F\x043E\x0433\x043E\x0434\x0430" || lower == L"weather" || lower == L"msn weather" || lower == L"msn \x043F\x043E\x0433\x043E\x0434\x0430") {
            Config::DeleteApp(L"\x041F\x043E\x0433\x043E\x0434\x0430");
            Config::DeleteApp(L"Weather");
            Config::DeleteApp(L"MSN Weather");
            Config::DeleteApp(L"MSN \x041F\x043E\x0433\x043E\x0434\x0430");
        }
        if (lower == L"microsoft store" || lower == L"store" || lower == L"\x043C\x0430\x0433\x0430\x0437\x0438\x043D microsoft") {
            Config::DeleteApp(L"Microsoft Store");
            Config::DeleteApp(L"Store");
            Config::DeleteApp(L"\x041C\x0430\x0433\x0430\x0437\x0438\x043D Microsoft");
        }
        if (lower.find(L"nvidia control panel") != std::wstring::npos || lower.find(L"\x043F\x0430\x043D\x0435\x043B\x044C \x0443\x043F\x0440\x0430\x0432\x043B\x0435\x043D\x0438\x044F nvidia") != std::wstring::npos) {
            Config::DeleteApp(L"NVIDIA Control Panel");
            Config::DeleteApp(L"\x041F\x0430\x043D\x0435\x043B\x044C \x0443\x043F\x0440\x0430\x0432\x043B\x0435\x043D\x0438\x044F NVIDIA");
        }
        if (lower == L"\x043F\x0440\x043E\x0432\x043E\x0434\x043D\x0438\x043A" || lower == L"explorer" || lower == L"file explorer") {
            Config::DeleteApp(L"\x041F\x0440\x043E\x0432\x043E\x0434\x043D\x0438\x043A");
            Config::DeleteApp(L"Explorer");
            Config::DeleteApp(L"File Explorer");
        }
        if (lower == L"\x043D\x043E\x0436\x043D\x0438\x0446\x044B" || lower == L"snipping tool" || lower == L"snippingtool") {
            Config::DeleteApp(L"\x041D\x043E\x0436\x043D\x0438\x0446\x044B");
            Config::DeleteApp(L"Snipping Tool");
        }

        std::shared_ptr<const IndexSnapshot> snap;
        {
            std::lock_guard<std::mutex> lock(Config::g_indexMutex);
            snap = Config::g_indexSnapshot;
        }
        if (snap) {
            for (const auto& item : snap->installedApps) {
                if (item.lowerName == lower || Config::ToLower(item.name) == lower) {
                    Config::DeleteApp(item.name);
                    break;
                }
            }
        }
        return true;
    }

    static void EnsureTabVisible(int index) {
        if (index < 0 || index >= (int)Config::g_tabs.size() || !g_hasTabOverflow) return;

        int tabLeft = Config::g_tabs[index].rect.left + g_tabScrollX;
        int tabRight = Config::g_tabs[index].rect.right + g_tabScrollX;

        int viewLeft = g_tabClipLeft + g_tabScrollX;
        int viewRight = g_tabClipRight + g_tabScrollX;

        if (tabLeft < viewLeft) {
            g_tabScrollX = tabLeft - g_tabClipLeft;
        } else if (tabRight > viewRight) {
            g_tabScrollX = tabRight - g_tabClipRight;
        }

        g_tabScrollX = (std::max)(0, (std::min)(g_maxTabScrollX, g_tabScrollX));
        RecalculateTabLayout();
    }

    static LRESULT CALLBACK SearchEditSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
        if (uMsg == WM_KEYDOWN) {
            int total = g_isSearchMode ? (int)g_searchResults.size() : (int)g_displayItems.size();
            if (total > 0) {
                if (g_isSearchMode) {
                    if (wParam == VK_DOWN) {
                        g_hoveredIndex = (g_hoveredIndex < 0) ? 0 : (std::min)(total - 1, g_hoveredIndex + 1);
                        EnsureHoverVisible();
                        InvalidateGridArea();
                        return 0;
                    } else if (wParam == VK_UP) {
                        g_hoveredIndex = (g_hoveredIndex < 0) ? total - 1 : (std::max)(0, g_hoveredIndex - 1);
                        EnsureHoverVisible();
                        InvalidateGridArea();
                        return 0;
                    }
                } else {
                    if (wParam == VK_RIGHT) {
                        g_hoveredIndex = (g_hoveredIndex < 0) ? 0 : (g_hoveredIndex + 1) % total;
                        EnsureHoverVisible();
                        InvalidateGridArea();
                        return 0;
                    } else if (wParam == VK_LEFT) {
                        g_hoveredIndex = (g_hoveredIndex < 0) ? total - 1 : (g_hoveredIndex - 1 + total) % total;
                        EnsureHoverVisible();
                        InvalidateGridArea();
                        return 0;
                    } else if (wParam == VK_DOWN) {
                        if (g_hoveredIndex < 0) g_hoveredIndex = 0;
                        else if (g_hoveredIndex + GRID_COLS < total) g_hoveredIndex += GRID_COLS;
                        EnsureHoverVisible();
                        InvalidateGridArea();
                        return 0;
                    } else if (wParam == VK_UP) {
                        if (g_hoveredIndex - GRID_COLS >= 0) g_hoveredIndex -= GRID_COLS;
                        EnsureHoverVisible();
                        InvalidateGridArea();
                        return 0;
                    }
                }
            }

            if (wParam == VK_RETURN) {
                int targetIdx = (g_hoveredIndex >= 0 && g_hoveredIndex < total) ? g_hoveredIndex : (total > 0 ? 0 : -1);
                bool keepOpen = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                if (targetIdx >= 0) {
                    g_justLaunchedTime = GetTickCount();
                    g_justLaunchedIndex = targetIdx;
                    Hooks::DismissNativeShellOverlays();
                    if (g_isSearchMode) {
                        AppItem app = GetSearchRowAppItem(targetIdx);
                        MenuInteraction::ExecuteApp(app, false);
                    } else {
                        MenuInteraction::ExecuteApp(g_displayItems[targetIdx], false);
                    }
                    if (keepOpen) {
                        g_suppressCloseOnDeactivate = true;
                        SetTimer(g_hWnd, TIMER_LAUNCH_FLASH, 500, nullptr);
                        SetTimer(g_hWnd, TIMER_RESET_DEACTIVATE, 600, nullptr);
                        SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        SetForegroundWindow(g_hWnd);
                        SetFocus(g_hSearchEdit);
                        InvalidateGridArea();
                    } else {
                        CloseImmediately();
                    }
                } else {
                    wchar_t query[256];
                    GetWindowTextW(hWnd, query, 256);
                    if (query[0] != L'\0') {
                        ShellExecuteW(nullptr, L"open", query, nullptr, nullptr, SW_SHOWNORMAL);
                        CloseImmediately();
                    }
                }
                return 0;
            } else if (wParam == VK_ESCAPE) {
                CloseImmediately();
                return 0;
            }
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    static void InvalidateGridArea() {
        RECT rcGrid = { 0, 60, MENU_WIDTH, MENU_HEIGHT };
        InvalidateRect(g_hWnd, &rcGrid, FALSE);
    }

    static void RecalculateTabLayout() {
        if (Config::g_tabs.empty()) return;

        int togW = Config::IsRussian() ? (g_currentMode == MODE_ALL_APPS ? 94 : 144) : (g_currentMode == MODE_ALL_APPS ? 84 : 92);
        g_toggleViewRect = { MENU_WIDTH - togW - 36, 68, MENU_WIDTH - 36, 96 };

        int maxRightBound = g_toggleViewRect.left - 16;
        int maxLeftBound = 52;

        int totalWidth = 0;
        std::vector<int> tabWidths;
        for (auto& tab : Config::g_tabs) {
            if (!tab.visible) {
                tabWidths.push_back(0);
                continue;
            }
            std::wstring label = Config::GetTabDisplayName(tab);
            int charW = Config::IsRussian() ? 10 : 8;
            int tabW = (std::max)(68, (int)label.length() * charW + 28);
            tabWidths.push_back(tabW);
            totalWidth += tabW + 6;
        }
        totalWidth += 32;

        int maxAllowedWidth = maxRightBound - maxLeftBound;
        g_hasTabOverflow = (totalWidth > maxAllowedWidth);

        if (g_hasTabOverflow) {
            g_scrollLeftRect = { maxLeftBound, 68, maxLeftBound + 22, 96 };
            g_scrollRightRect = { maxRightBound - 22, 68, maxRightBound, 96 };

            g_tabClipLeft = maxLeftBound + 26;
            g_tabClipRight = maxRightBound - 26;

            int viewWidth = g_tabClipRight - g_tabClipLeft;
            g_maxTabScrollX = (std::max)(0, totalWidth - viewWidth);
            g_tabScrollX = (std::max)(0, (std::min)(g_maxTabScrollX, g_tabScrollX));
        } else {
            g_scrollLeftRect = { 0, 0, 0, 0 };
            g_scrollRightRect = { 0, 0, 0, 0 };
            g_tabClipLeft = maxLeftBound;
            g_tabClipRight = maxRightBound;
            g_maxTabScrollX = 0;
            g_tabScrollX = 0;
        }

        int curX = g_tabClipLeft - g_tabScrollX;
        for (size_t i = 0; i < Config::g_tabs.size(); ++i) {
            if (!Config::g_tabs[i].visible) continue;
            int tabW = tabWidths[i];
            Config::g_tabs[i].rect = { curX, 68, curX + tabW, 96 };
            curX += tabW + 6;
        }

        g_addTabRect = { curX, 68, curX + 26, 96 };
    }

    static void ReloadPinnedData() {
        {
            std::lock_guard<std::mutex> lock(Config::g_indexMutex);
            for (auto& tab : Config::g_tabs) {
                tab.items = Config::LoadTabItems(tab);
            }
        }
        RecalculateTabLayout();
        TriggerSearch();
    }

    static void TriggerSearch() {
        KillTimer(g_hWnd, TIMER_SEARCH_DEBOUNCE);
        SetTimer(g_hWnd, TIMER_SEARCH_DEBOUNCE, 15, nullptr);
    }

    static void PopulateItemDetails(AppItem& item) {
        if (item.target.rfind(L"shell:AppsFolder\\", 0) == 0) {
            item.displayPath = Config::IsRussian() ? L"\x041F\x0440\x0438\x043B\x043E\x0436\x0435\x043D\x0438\x0435" : L"Application";
            item.sizeStr = L"";
            item.dateStr = L"";
            return;
        }

        std::wstring checkPath = AppIndexer::ResolveItemRealTargetPath(item.target);

        size_t lastSlash = checkPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            item.displayPath = checkPath.substr(0, lastSlash);
        } else {
            item.displayPath = checkPath;
        }

        if (item.isDirectory) {
            item.sizeStr = L"";
        }

        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(checkPath.c_str(), GetFileExInfoStandard, &fad)) {
            if (!item.isDirectory) {
                ULARGE_INTEGER ul;
                ul.LowPart = fad.nFileSizeLow;
                ul.HighPart = fad.nFileSizeHigh;
                uint64_t bytes = ul.QuadPart;

                wchar_t szBuf[32];
                if (bytes < 1024) {
                    swprintf_s(szBuf, Config::IsRussian() ? L"%llu \x0411" : L"%llu B", bytes);
                } else if (bytes < 1024 * 1024) {
                    uint64_t kb = (bytes + 1023) / 1024;
                    swprintf_s(szBuf, Config::IsRussian() ? L"%llu \x041A\x0411" : L"%llu KB", kb);
                } else if (bytes < 1024ULL * 1024 * 1024) {
                    double mb = (double)bytes / (1024.0 * 1024.0);
                    swprintf_s(szBuf, Config::IsRussian() ? L"%.1f \x041C\x0411" : L"%.1f MB", mb);
                } else {
                    double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
                    swprintf_s(szBuf, Config::IsRussian() ? L"%.2f \x0413\x0411" : L"%.2f GB", gb);
                }
                item.sizeStr = szBuf;
            }

            FILETIME localFt;
            if (FileTimeToLocalFileTime(&fad.ftLastWriteTime, &localFt)) {
                SYSTEMTIME st;
                if (FileTimeToSystemTime(&localFt, &st)) {
                    wchar_t dtBuf[32];
                    swprintf_s(dtBuf, L"%02d.%02d.%04d %02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
                    item.dateStr = dtBuf;
                }
            }
        }
    }

    static void ExecuteAsyncSearch() {
        wchar_t query[128] = { 0 };
        GetWindowTextW(g_hSearchEdit, query, 128);
        std::wstring sQuery = query;

        if (sQuery.empty()) {
            g_isSearchMode = false;
            g_searchGeneration.fetch_add(1);
            g_searchResults.clear();
            g_searchApps.clear();
            if (g_currentMode == MODE_ALL_APPS) {
                std::shared_ptr<const IndexSnapshot> snap;
                {
                    std::lock_guard<std::mutex> lock(Config::g_indexMutex);
                    snap = Config::g_indexSnapshot;
                }
                g_displayItems.clear();
                if (snap) {
                    for (const auto& item : snap->installedApps) {
                        if (AppIndexer::IsSystem32Path(item.target) ||
                            AppIndexer::IsSystem32Path(item.lowerTarget)) {
                            continue;
                        }
                        g_displayItems.push_back(item);
                    }
                }
            } else {
                const auto& activeTab = Config::GetActiveTab();
                g_displayItems = activeTab.items;
            }
            int totalRows = ((int)g_displayItems.size() + GRID_COLS - 1) / GRID_COLS;
            g_maxScrollY = (std::max)(0, (totalRows * CELL_HEIGHT) - VIEWPORT_HEIGHT);
            g_scrollY = (std::max)(0, (std::min)(g_maxScrollY, g_scrollY));
            g_hoveredIndex = -1;
            InvalidateGridArea();
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            return;
        }

        g_isSearchMode = true;

        uint64_t currentGen = g_searchGeneration.fetch_add(1) + 1;
        HWND targetWnd = g_hWnd;

        std::thread([currentGen, sQuery, targetWnd]() {
            std::wstring lowerQuery = Config::ToLower(sQuery);
            std::wstring convertedQuery = AppIndexer::ConvertKeyboardLayout(lowerQuery);

            std::vector<std::wstring> tokens;
            std::vector<std::wstring> convertedTokens;
            size_t idx = 0;
            while (idx < lowerQuery.length()) {
                while (idx < lowerQuery.length() && iswspace(lowerQuery[idx])) idx++;
                if (idx >= lowerQuery.length()) break;
                size_t start = idx;
                while (idx < lowerQuery.length() && !iswspace(lowerQuery[idx])) idx++;
                std::wstring token = lowerQuery.substr(start, idx - start);
                std::wstring convToken = AppIndexer::ConvertKeyboardLayout(token);
                tokens.push_back(token);
                convertedTokens.push_back(convToken == token ? std::wstring() : convToken);
            }

            std::shared_ptr<const IndexSnapshot> snap;
            std::vector<AppItem> pinnedItems;
            {
                std::lock_guard<std::mutex> lock(Config::g_indexMutex);
                if (g_searchGeneration.load() != currentGen) return;
                snap = Config::g_indexSnapshot;
                for (const auto& tab : Config::g_tabs) {
                    pinnedItems.insert(pinnedItems.end(), tab.items.begin(), tab.items.end());
                }
            }

            std::vector<AppItem> matchedApps;
            matchedApps.reserve(64);

            std::vector<SearchResultEntry> results;
            results.reserve(snap ? (snap->userFiles.size() > 65536 ? 65536 : snap->userFiles.size()) : 1024);

            std::unordered_set<std::wstring> seenTargets;
            seenTargets.reserve(128);

            auto processItem = [&](const AppItem& item, uint8_t src) {
                if (seenTargets.count(item.lowerTarget) > 0) return;

                int score = AppIndexer::CalculateItemScoreFast(item, lowerQuery, convertedQuery, tokens, convertedTokens);
                if (score > 0) {
                    seenTargets.insert(item.lowerTarget);
                    AppItem scored = item;
                    scored.searchScore = score;
                    PopulateItemDetails(scored);
                    uint32_t appIdx = (uint32_t)matchedApps.size();
                    matchedApps.push_back(std::move(scored));

                    SearchResultEntry entry;
                    entry.source = src;
                    entry.sourceIndex = appIdx;
                    entry.score = (int16_t)score;
                    entry.isDirectory = item.isDirectory;
                    results.push_back(entry);
                }
            };

            for (const auto& item : pinnedItems) {
                processItem(item, 0);
            }
            if (g_searchGeneration.load() != currentGen) return;

            if (snap) {
                for (const auto& item : snap->installedApps) {
                    processItem(item, 1);
                }
                if (g_searchGeneration.load() != currentGen) return;

                for (const auto& item : snap->frequentFolders) {
                    processItem(item, 1);
                }
                if (g_searchGeneration.load() != currentGen) return;

                const size_t totalUserFiles = snap->userFiles.size();
                for (size_t i = 0; i < totalUserFiles; ++i) {
                    if ((i & 2047) == 0) {
                        if (g_searchGeneration.load() != currentGen) return;
                    }
                    const auto& cf = snap->userFiles[i];
                    int score = AppIndexer::CalculateCompactScoreFast(cf, lowerQuery, convertedQuery, tokens, convertedTokens);
                    if (score > 0) {
                        SearchResultEntry entry;
                        entry.source = 2;
                        entry.sourceIndex = (uint32_t)i;
                        entry.score = (int16_t)score;
                        entry.isDirectory = cf.isDirectory;
                        results.push_back(entry);
                    }
                }
            }

            if (g_searchGeneration.load() != currentGen) return;

            std::sort(results.begin(), results.end(), [&](const SearchResultEntry& a, const SearchResultEntry& b) {
                if (a.score != b.score) return a.score > b.score;
                if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
                size_t lenA = (a.source == 2 && snap) ? snap->userFiles[a.sourceIndex].name.length() : (a.sourceIndex < matchedApps.size() ? matchedApps[a.sourceIndex].name.length() : 0);
                size_t lenB = (b.source == 2 && snap) ? snap->userFiles[b.sourceIndex].name.length() : (b.sourceIndex < matchedApps.size() ? matchedApps[b.sourceIndex].name.length() : 0);
                return lenA < lenB;
            });

            if (g_searchGeneration.load() == currentGen) {
                auto* pPayload = new SearchPayload();
                pPayload->searchApps = std::move(matchedApps);
                pPayload->results = std::move(results);
                if (!PostMessageW(targetWnd, WM_APP_SEARCH_COMPLETE, (WPARAM)currentGen, (LPARAM)pPayload)) {
                    delete pPayload;
                }
            }
        }).detach();
    }

    static void Toggle(bool show) {
        if (show) {
            Hooks::DismissNativeShellOverlays();
            if ((Hooks::g_startButtonRect.right - Hooks::g_startButtonRect.left) <= 0) {
                Hooks::UpdateStartButtonRect(true);
            }

            MenuInteraction::g_isPowerMenuOpen = false;
            g_currentMode = MODE_TABS;
            g_scrollY = 0;
            g_hoveredIndex = -1;
            g_showTooltip = false;
            g_tooltipItemIndex = -1;
            g_justLaunchedIndex = -1;
            g_suppressCloseOnDeactivate = false;
            ReloadPinnedData();
            SetWindowTextW(g_hSearchEdit, L"");

            POINT ptAnchor;
            if ((Hooks::g_startButtonRect.right - Hooks::g_startButtonRect.left) > 0 &&
                (Hooks::g_startButtonRect.bottom - Hooks::g_startButtonRect.top) > 0) {
                ptAnchor.x = (Hooks::g_startButtonRect.left + Hooks::g_startButtonRect.right) / 2;
                ptAnchor.y = (Hooks::g_startButtonRect.top + Hooks::g_startButtonRect.bottom) / 2;
            } else {
                GetCursorPos(&ptAnchor);
            }

            HMONITOR hMon = MonitorFromPoint(ptAnchor, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(MONITORINFO) };
            RECT workArea;
            if (GetMonitorInfoW(hMon, &mi)) {
                workArea = mi.rcWork;
            } else {
                SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
            }

            const int margin = 12;
            int targetX = 0;
            int targetY = 0;
            int animOffX = 0;
            int animOffY = 0;

            UINT edge = Hooks::g_taskbarEdge;
            DWORD alignment = Hooks::g_taskbarAlignment;

            switch (edge) {
                case ABE_TOP: {
                    targetY = workArea.top + margin;
                    animOffX = 0;
                    animOffY = -20;

                    if (alignment == 1) {
                        targetX = workArea.left + ((workArea.right - workArea.left) - MENU_WIDTH) / 2;
                    } else {
                        int desiredX = (Hooks::g_startButtonRect.right > Hooks::g_startButtonRect.left) 
                            ? Hooks::g_startButtonRect.left 
                            : workArea.left + margin;
                        targetX = (std::max)((int)workArea.left + margin, (std::min)((int)workArea.right - MENU_WIDTH - margin, desiredX));
                    }
                    break;
                }

                case ABE_LEFT: {
                    targetX = workArea.left + margin;
                    animOffX = -20;
                    animOffY = 0;

                    if (alignment == 1) {
                        targetY = workArea.top + ((workArea.bottom - workArea.top) - MENU_HEIGHT) / 2;
                    } else {
                        int desiredY = (Hooks::g_startButtonRect.bottom > Hooks::g_startButtonRect.top) 
                            ? Hooks::g_startButtonRect.top 
                            : workArea.top + margin;
                        targetY = (std::max)((int)workArea.top + margin, (std::min)((int)workArea.bottom - MENU_HEIGHT - margin, desiredY));
                    }
                    break;
                }

                case ABE_RIGHT: {
                    targetX = workArea.right - MENU_WIDTH - margin;
                    animOffX = 20;
                    animOffY = 0;

                    if (alignment == 1) {
                        targetY = workArea.top + ((workArea.bottom - workArea.top) - MENU_HEIGHT) / 2;
                    } else {
                        int desiredY = (Hooks::g_startButtonRect.bottom > Hooks::g_startButtonRect.top) 
                            ? Hooks::g_startButtonRect.top 
                            : workArea.top + margin;
                        targetY = (std::max)((int)workArea.top + margin, (std::min)((int)workArea.bottom - MENU_HEIGHT - margin, desiredY));
                    }
                    break;
                }

                case ABE_BOTTOM:
                default: {
                    targetY = workArea.bottom - MENU_HEIGHT - margin;
                    animOffX = 0;
                    animOffY = 20;

                    if (alignment == 1) {
                        targetX = workArea.left + ((workArea.right - workArea.left) - MENU_WIDTH) / 2;
                    } else {
                        int desiredX = (Hooks::g_startButtonRect.right > Hooks::g_startButtonRect.left) 
                            ? Hooks::g_startButtonRect.left 
                            : workArea.left + margin;
                        targetX = (std::max)((int)workArea.left + margin, (std::min)((int)workArea.right - MENU_WIDTH - margin, desiredX));
                    }
                    break;
                }
            }

            g_targetX = targetX;
            g_targetY = targetY;
            g_animOffsetX = animOffX;
            g_animOffsetY = animOffY;

            g_animatingIn = true;
            g_isAnimating = true;
            g_animStartTime = GetTickCount();
            g_openTimestamp = GetTickCount();

            SetLayeredWindowAttributes(g_hWnd, 0, 0, LWA_ALPHA);
            SetWindowPos(g_hWnd, HWND_TOPMOST, g_targetX + g_animOffsetX, g_targetY + g_animOffsetY, MENU_WIDTH, MENU_HEIGHT, SWP_SHOWWINDOW | SWP_NOACTIVATE);

            SetTimer(g_hWnd, TIMER_ANIMATION, 14, nullptr);

            HWND hForeWnd = GetForegroundWindow();
            DWORD foreThread = GetWindowThreadProcessId(hForeWnd, nullptr);
            DWORD curThread = GetCurrentThreadId();

            if (foreThread != curThread) {
                AttachThreadInput(curThread, foreThread, TRUE);
                SetForegroundWindow(g_hWnd);
                SetFocus(g_hWnd);
                AttachThreadInput(curThread, foreThread, FALSE);
            } else {
                SetForegroundWindow(g_hWnd);
                SetFocus(g_hWnd);
            }

            g_isVisible = true;
        } else {
            CloseImmediately();
        }
    }

    static void StepAnimation() {
        DWORD elapsed = GetTickCount() - g_animStartTime;
        float t = (float)elapsed / (float)ANIM_DURATION;

        if (t >= 1.0f) {
            KillTimer(g_hWnd, TIMER_ANIMATION);
            g_isAnimating = false;

            if (g_animatingIn) {
                SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);
                SetWindowPos(g_hWnd, HWND_TOPMOST, g_targetX, g_targetY, MENU_WIDTH, MENU_HEIGHT, SWP_NOACTIVATE);
            } else {
                CloseImmediately();
            }
            return;
        }

        float ease = 1.0f - powf(1.0f - t, 4.0f);
        float invEase = 1.0f - ease;

        if (g_animatingIn) {
            BYTE alpha = (BYTE)(ease * 255.0f);
            int currentX = g_targetX + (int)(invEase * (float)g_animOffsetX);
            int currentY = g_targetY + (int)(invEase * (float)g_animOffsetY);
            SetLayeredWindowAttributes(g_hWnd, 0, alpha, LWA_ALPHA);
            SetWindowPos(g_hWnd, HWND_TOPMOST, currentX, currentY, MENU_WIDTH, MENU_HEIGHT, SWP_NOACTIVATE | SWP_NOZORDER);
        } else {
            CloseImmediately();
        }
    }
};