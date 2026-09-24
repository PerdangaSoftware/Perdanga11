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

    // Visual feedback and focus management for Shift+Click multi-launching
    static inline int g_justLaunchedIndex = -1;
    static inline DWORD g_justLaunchedTime = 0;
    static inline bool g_suppressCloseOnDeactivate = false;

    // Window dimensions
    static const inline int MENU_WIDTH = 740;
    static const inline int MENU_HEIGHT = 720;
    static const inline int ANIM_DURATION = 190;

    static const inline int GRID_COLS = 6;
    static const inline int CELL_WIDTH = 106;
    static const inline int CELL_HEIGHT = 98;
    static const inline int GRID_START_X = 52;
    static const inline int GRID_START_Y = 112;
    static const inline int VIEWPORT_HEIGHT = 540;

    static inline int g_scrollY = 0;
    static inline int g_maxScrollY = 0;

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
    static inline std::atomic<uint64_t> g_searchGeneration{ 0 };

    static void CloseImmediately() {
        if (!g_isVisible && !g_isAnimating) return;
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
    }

    static void EnsureHoverVisible() {
        if (g_hoveredIndex < 0) return;
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
            int total = (int)g_displayItems.size();
            if (total > 0) {
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

            if (wParam == VK_RETURN) {
                int targetIdx = (g_hoveredIndex >= 0 && g_hoveredIndex < total) ? g_hoveredIndex : (total > 0 ? 0 : -1);
                bool keepOpen = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                if (targetIdx >= 0) {
                    MenuInteraction::ExecuteApp(g_displayItems[targetIdx], false);
                    if (keepOpen) {
                        g_suppressCloseOnDeactivate = true;
                        g_justLaunchedIndex = targetIdx;
                        g_justLaunchedTime = GetTickCount();
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
                    if (wcslen(query) > 0) {
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
        RECT gridArea = { 0, GRID_START_Y - 40, MENU_WIDTH, MENU_HEIGHT - 52 };
        InvalidateRect(g_hWnd, &gridArea, FALSE);
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
            // Tab items are shared with search workers: mutate them under the index lock
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
        SetTimer(g_hWnd, TIMER_SEARCH_DEBOUNCE, 40, nullptr);
    }

    static void ExecuteAsyncSearch() {
        wchar_t query[128] = { 0 };
        GetWindowTextW(g_hSearchEdit, query, 128);
        std::wstring sQuery = query;

        if (sQuery.empty()) {
            g_searchGeneration.fetch_add(1);
            if (g_currentMode == MODE_ALL_APPS) {
                std::shared_ptr<const IndexSnapshot> snap;
                {
                    std::lock_guard<std::mutex> lock(Config::g_indexMutex);
                    snap = Config::g_indexSnapshot;
                }
                g_displayItems = snap ? snap->installedApps : std::vector<AppItem>();
            } else {
                const auto& activeTab = Config::GetActiveTab();
                g_displayItems = activeTab.items;
            }
            int totalRows = ((int)g_displayItems.size() + GRID_COLS - 1) / GRID_COLS;
            g_maxScrollY = (std::max)(0, (totalRows * CELL_HEIGHT) - VIEWPORT_HEIGHT);
            g_scrollY = (std::max)(0, (std::min)(g_maxScrollY, g_scrollY));
            g_hoveredIndex = -1;
            InvalidateGridArea();
            return;
        }

        uint64_t currentGen = g_searchGeneration.fetch_add(1) + 1;
        HWND targetWnd = g_hWnd;

        std::thread([currentGen, sQuery, targetWnd]() {
            std::wstring lowerQuery = Config::ToLower(sQuery);

            // Convert the query (and every token) once per search instead of once
            // per candidate item: this used to allocate inside the scoring hot loop
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

            // Grab an immutable snapshot of the index and a copy of pinned tab items
            // under a short lock, then score everything without holding the mutex
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

            std::vector<AppItem> candidates;
            candidates.reserve(256);

            std::unordered_set<std::wstring> seenTargets;
            seenTargets.reserve(512);

            auto processItem = [&](const AppItem& item) {
                if (seenTargets.count(item.lowerTarget) > 0) return;

                int score = AppIndexer::CalculateItemScoreFast(item, lowerQuery, convertedQuery, tokens, convertedTokens);
                if (score > 0) {
                    seenTargets.insert(item.lowerTarget);
                    AppItem scored = item;
                    scored.searchScore = score;
                    candidates.push_back(std::move(scored));
                }
            };

            for (const auto& item : pinnedItems) {
                processItem(item);
            }
            if (g_searchGeneration.load() != currentGen) return;

            if (snap) {
                for (const auto& item : snap->installedApps) {
                    processItem(item);
                }
                if (g_searchGeneration.load() != currentGen) return;

                for (const auto& item : snap->frequentFolders) {
                    processItem(item);
                }
                if (g_searchGeneration.load() != currentGen) return;

                for (size_t i = 0; i < snap->userFiles.size(); ++i) {
                    if ((i & 511) == 0) {
                        if (g_searchGeneration.load() != currentGen) return;
                    }
                    processItem(snap->userFiles[i]);
                }
            }

            if (g_searchGeneration.load() != currentGen) return;

            std::sort(candidates.begin(), candidates.end(), [](const AppItem& a, const AppItem& b) {
                if (a.searchScore != b.searchScore) return a.searchScore > b.searchScore;
                if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
                return a.name.length() < b.name.length();
            });

            if (candidates.size() > 180) {
                candidates.resize(180);
            }

            if (g_searchGeneration.load() == currentGen) {
                auto* pHeapResults = new std::vector<AppItem>(std::move(candidates));
                if (!PostMessageW(targetWnd, WM_APP_SEARCH_COMPLETE, (WPARAM)currentGen, (LPARAM)pHeapResults)) {
                    delete pHeapResults;
                }
            }
        }).detach();
    }

    static void Toggle(bool show) {
        if (show) {
            // Hide native Start / Search overlays first so they never overlap our menu
            Hooks::DismissNativeShellOverlays();
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

            RECT workArea;
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

            g_targetX = workArea.left + ((workArea.right - workArea.left) - MENU_WIDTH) / 2;
            g_targetY = workArea.bottom - MENU_HEIGHT - 14;

            g_animatingIn = true;
            g_isAnimating = true;
            g_animStartTime = GetTickCount();
            g_openTimestamp = GetTickCount();

            SetLayeredWindowAttributes(g_hWnd, 0, 0, LWA_ALPHA);
            SetWindowPos(g_hWnd, HWND_TOPMOST, g_targetX, g_targetY + 20, MENU_WIDTH, MENU_HEIGHT, SWP_SHOWWINDOW | SWP_NOACTIVATE);

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

        if (g_animatingIn) {
            BYTE alpha = (BYTE)(ease * 255.0f);
            int currentY = g_targetY + (int)((1.0f - ease) * 20.0f);
            SetLayeredWindowAttributes(g_hWnd, 0, alpha, LWA_ALPHA);
            SetWindowPos(g_hWnd, HWND_TOPMOST, g_targetX, currentY, MENU_WIDTH, MENU_HEIGHT, SWP_NOACTIVATE | SWP_NOZORDER);
        } else {
            CloseImmediately();
        }
    }
};