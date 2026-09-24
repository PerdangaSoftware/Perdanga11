#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cwctype>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>

#define WM_APP_INDEX_READY      (WM_USER + 4)
#define WM_APP_SEARCH_COMPLETE  (WM_USER + 5)

enum class AppLanguage {
    Auto = 0,
    Russian = 1,
    English = 2
};

struct AppItem {
    std::wstring name;
    std::wstring target;
    std::wstring arguments;
    std::wstring lowerName;
    std::wstring lowerExt;
    std::wstring lowerTarget;
    bool isDirectory = false;
    HICON hIcon = nullptr;
    int searchScore = 0;
};

struct TabDefinition {
    std::wstring id;
    std::wstring name;
    std::wstring section;
    bool isFolder = false;
    bool visible = true;
    std::vector<AppItem> items;
    RECT rect = { 0, 0, 0, 0 };
    bool hovered = false;
};

// Immutable search index published by the background indexer thread.
// Readers grab the shared_ptr under g_indexMutex, then iterate lock-free.
struct IndexSnapshot {
    std::vector<AppItem> installedApps;
    std::vector<AppItem> frequentFolders;
    std::vector<AppItem> userFiles;
};

class Config {
public:
    static const inline size_t MAX_TABS = 128;

    static inline std::shared_ptr<const IndexSnapshot> g_indexSnapshot;
    static inline std::vector<TabDefinition> g_tabs;
    static inline int g_activeTabIndex = 0;
    // Guards g_indexSnapshot publication AND every mutation of g_tabs / tab.items,
    // so search workers can snapshot them safely while the UI thread keeps editing
    static inline std::mutex g_indexMutex;
    static inline bool g_isIndexingComplete = false;
    static inline AppLanguage g_configuredLanguage = AppLanguage::Auto;

    static inline bool g_cachedDarkMode = true;

    static void UpdateThemeCache() {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD val = 0;
            DWORD sz = sizeof(val);
            if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) {
                g_cachedDarkMode = (val == 0);
                RegCloseKey(hKey);
                return;
            }
            RegCloseKey(hKey);
        }
        g_cachedDarkMode = true;
    }

    static bool IsDarkMode() {
        return g_cachedDarkMode;
    }

    static void EnsureConfigUnicode() {
        std::wstring iniPath = GetConfigPath();
        DWORD attr = GetFileAttributesW(iniPath.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            HANDLE hFile = CreateFileW(iniPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                unsigned char bom[2] = { 0xFF, 0xFE };
                DWORD written = 0;
                WriteFile(hFile, bom, sizeof(bom), &written, nullptr);
                CloseHandle(hFile);
            }
        }
    }

    static AppLanguage GetSystemLanguage() {
        LANGID uiLang = GetUserDefaultUILanguage();
        WORD primaryUi = PRIMARYLANGID(uiLang);
        if (primaryUi == LANG_RUSSIAN || primaryUi == LANG_UKRAINIAN || primaryUi == LANG_BELARUSIAN) {
            return AppLanguage::Russian;
        }

        LANGID sysLang = GetSystemDefaultUILanguage();
        WORD primarySys = PRIMARYLANGID(sysLang);
        if (primarySys == LANG_RUSSIAN || primarySys == LANG_UKRAINIAN || primarySys == LANG_BELARUSIAN) {
            return AppLanguage::Russian;
        }

        wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = { 0 };
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
            if (_wcsnicmp(localeName, L"ru", 2) == 0 ||
                _wcsnicmp(localeName, L"uk", 2) == 0 ||
                _wcsnicmp(localeName, L"be", 2) == 0) {
                return AppLanguage::Russian;
            }
        }

        LCID lcid = GetUserDefaultLCID();
        if (PRIMARYLANGID(lcid) == LANG_RUSSIAN) {
            return AppLanguage::Russian;
        }

        return AppLanguage::English;
    }

    static bool IsRussian() {
        if (g_configuredLanguage == AppLanguage::Russian) return true;
        if (g_configuredLanguage == AppLanguage::English) return false;
        return (GetSystemLanguage() == AppLanguage::Russian);
    }

    static void SetLanguage(AppLanguage lang) {
        g_configuredLanguage = lang;
        std::wstring iniPath = GetConfigPath();
        const wchar_t* val = (lang == AppLanguage::Russian) ? L"ru" : ((lang == AppLanguage::English) ? L"en" : L"auto");
        WritePrivateProfileStringW(L"Settings", L"Language", val, iniPath.c_str());
    }

    static std::wstring GetTabDisplayName(const TabDefinition& tab) {
        if (tab.id == L"pinned" || _wcsicmp(tab.name.c_str(), L"pinned") == 0 || _wcsicmp(tab.name.c_str(), L"\x0417\x0430\x043A\x0440\x0435\x043F\x043B\x0435\x043D\x043E") == 0) {
            return IsRussian() ? L"\x0417\x0430\x043A\x0440\x0435\x043F\x043B\x0435\x043D\x043E" : L"Pinned";
        }
        if (tab.id == L"folders" || _wcsicmp(tab.name.c_str(), L"folders") == 0 || _wcsicmp(tab.name.c_str(), L"\x041F\x0430\x043F\x043A\x0438") == 0) {
            return IsRussian() ? L"\x041F\x0430\x043F\x043A\x0438" : L"Folders";
        }
        return tab.name;
    }

    static bool CanAddTab() {
        return g_tabs.size() < MAX_TABS;
    }

    static bool DoesFolderExist(const std::wstring& path) {
        if (path.empty()) return false;
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return false;
        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    static bool DoesFileExist(const std::wstring& path) {
        if (path.empty()) return false;
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return false;
        return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    static std::wstring ToLower(std::wstring str) {
        if (!str.empty()) {
            CharLowerBuffW(&str[0], (DWORD)str.length());
        }
        return str;
    }

    static std::wstring GetExecutablePath() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::wstring(buffer);
    }

    static std::wstring GetConfigPath() {
        // Resolved once per process: avoids repeated write-permission probes on every access
        static std::wstring cachedPath;
        if (!cachedPath.empty()) return cachedPath;

        std::wstring exePath = GetExecutablePath();
        size_t pos = exePath.find_last_of(L"\\/");
        std::wstring exeDir = (pos != std::wstring::npos) ? exePath.substr(0, pos + 1) : L"";
        std::wstring localIni = exeDir + L"config.ini";

        if (DoesFileExist(localIni)) {
            std::wstring testTmp = exeDir + L"__perm_test.tmp";
            HANDLE hTest = CreateFileW(testTmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hTest != INVALID_HANDLE_VALUE) {
                CloseHandle(hTest);
                DeleteFileW(testTmp.c_str());
                cachedPath = localIni;
                return cachedPath;
            }
        }

        PWSTR pAppData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pAppData)) && pAppData) {
            std::wstring appDataFolder = std::wstring(pAppData) + L"\\Perdanga11";
            CoTaskMemFree(pAppData);
            CreateDirectoryW(appDataFolder.c_str(), nullptr);
            std::wstring appDataIni = appDataFolder + L"\\config.ini";

            if (!DoesFileExist(appDataIni) && DoesFileExist(localIni)) {
                CopyFileW(localIni.c_str(), appDataIni.c_str(), TRUE);
            }
            cachedPath = appDataIni;
            return cachedPath;
        }

        cachedPath = localIni;
        return cachedPath;
    }

    static bool IsAppPinned(const std::wstring& name, const std::vector<AppItem>& pinnedList) {
        for (const auto& item : pinnedList) {
            if (_wcsicmp(item.name.c_str(), name.c_str()) == 0) return true;
        }
        return false;
    }

    static bool IsFolderPinned(const std::wstring& name, const std::vector<AppItem>& folderList) {
        for (const auto& item : folderList) {
            if (_wcsicmp(item.name.c_str(), name.c_str()) == 0) return true;
        }
        return false;
    }

    static void SaveApp(const std::wstring& name, const std::wstring& path) {
        EnsureConfigUnicode();
        std::wstring iniPath = GetConfigPath();
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            WritePrivateProfileStringW(L"Folders", name.c_str(), path.c_str(), iniPath.c_str());
        } else {
            WritePrivateProfileStringW(L"Apps", name.c_str(), path.c_str(), iniPath.c_str());
        }
    }

    static void DeleteApp(const std::wstring& name) {
        std::wstring iniPath = GetConfigPath();
        WritePrivateProfileStringW(L"Apps", name.c_str(), nullptr, iniPath.c_str());
    }

    static void SaveFolder(const std::wstring& name, const std::wstring& path) {
        EnsureConfigUnicode();
        std::wstring iniPath = GetConfigPath();
        WritePrivateProfileStringW(L"Folders", name.c_str(), path.c_str(), iniPath.c_str());
    }

    static void DeleteFolder(const std::wstring& name) {
        std::wstring iniPath = GetConfigPath();
        WritePrivateProfileStringW(L"Folders", name.c_str(), nullptr, iniPath.c_str());
    }

    // Completely cleans obsolete broken keys and registers verified valid shell verbs
    static void RegisterShellContextMenu() {
        std::wstring exePath = GetExecutablePath();
        if (exePath.empty() || !DoesFileExist(exePath)) return;

        const wchar_t* parentKeys[] = {
            L"Software\\Classes\\exefile\\shell",
            L"Software\\Classes\\lnkfile\\shell",
            L"Software\\Classes\\*\\shell",
            L"Software\\Classes\\Directory\\shell",
            L"Software\\Classes\\Folder\\shell"
        };

        // Aggressively scan and wipe any legacy or mispointed keys
        for (const wchar_t* parent : parentKeys) {
            HKEY hParent = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, parent, 0, KEY_READ | KEY_WRITE, &hParent) == ERROR_SUCCESS) {
                wchar_t subKeyName[256];
                DWORD index = 0;
                DWORD nameLen = 256;
                std::vector<std::wstring> toDelete;

                while (RegEnumKeyExW(hParent, index++, subKeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    std::wstring lower = ToLower(subKeyName);
                    // Only remove our own verbs (current "Perdanga11.Pin" and legacy
                    // "PinToPerdanga11"); never touch keys belonging to other software
                    if (lower.rfind(L"perdanga11", 0) == 0 || lower == L"pintoperdanga11") {
                        toDelete.push_back(subKeyName);
                    }
                    nameLen = 256;
                }

                for (const auto& k : toDelete) {
                    RegDeleteTreeW(hParent, k.c_str());
                }
                RegCloseKey(hParent);
            }
        }

        std::wstring cmdStr = L"\"" + exePath + L"\" --pin \"%1\"";
        std::wstring iconStr = L"\"" + exePath + L"\",0";
        const wchar_t* menuText = IsRussian() ? L"\x0417\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x0432 Perdanga11" : L"Pin to Perdanga11";

        const wchar_t* registerKeys[] = {
            L"Software\\Classes\\*\\shell\\Perdanga11.Pin",
            L"Software\\Classes\\exefile\\shell\\Perdanga11.Pin",
            L"Software\\Classes\\lnkfile\\shell\\Perdanga11.Pin",
            L"Software\\Classes\\Directory\\shell\\Perdanga11.Pin",
            L"Software\\Classes\\Folder\\shell\\Perdanga11.Pin"
        };

        for (const wchar_t* subKey : registerKeys) {
            HKEY hKey = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)menuText, (DWORD)((wcslen(menuText) + 1) * sizeof(wchar_t)));
                RegSetValueExW(hKey, L"MUIVerb", 0, REG_SZ, (const BYTE*)menuText, (DWORD)((wcslen(menuText) + 1) * sizeof(wchar_t)));
                RegSetValueExW(hKey, L"Icon", 0, REG_SZ, (const BYTE*)iconStr.c_str(), (DWORD)((iconStr.length() + 1) * sizeof(wchar_t)));

                const wchar_t* emptyVal = L"";
                RegSetValueExW(hKey, L"NeverDefault", 0, REG_SZ, (const BYTE*)emptyVal, sizeof(wchar_t));
                DWORD zeroVal = 0;
                DWORD oneVal = 1;
                DWORD browserFlags = 0x00000008;
                RegSetValueExW(hKey, L"LaunchTarget", 0, REG_DWORD, (const BYTE*)&zeroVal, sizeof(DWORD));
                RegSetValueExW(hKey, L"ZoneCheck", 0, REG_DWORD, (const BYTE*)&zeroVal, sizeof(DWORD));
                RegSetValueExW(hKey, L"NoZoneCheck", 0, REG_DWORD, (const BYTE*)&oneVal, sizeof(DWORD));
                RegSetValueExW(hKey, L"BrowserFlags", 0, REG_DWORD, (const BYTE*)&browserFlags, sizeof(DWORD));

                HKEY hCmd = nullptr;
                if (RegCreateKeyExW(hKey, L"command", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hCmd, nullptr) == ERROR_SUCCESS) {
                    RegSetValueExW(hCmd, nullptr, 0, REG_SZ, (const BYTE*)cmdStr.c_str(), (DWORD)((cmdStr.length() + 1) * sizeof(wchar_t)));
                    RegCloseKey(hCmd);
                }
                RegCloseKey(hKey);
            }
        }

        // Register in SendTo folder for native bypass of execution warnings
        PWSTR pSendTo = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &pSendTo)) && pSendTo) {
            std::wstring sendToLnk = std::wstring(pSendTo) + L"\\" + (IsRussian() ? L"\x0417\x0430\x043A\x0440\x0435\x043F\x0438\x0442\x044C \x0432 Perdanga11.lnk" : L"Pin to Perdanga11.lnk");
            CoTaskMemFree(pSendTo);

            IShellLinkW* psl = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
                psl->SetPath(exePath.c_str());
                psl->SetArguments(L"--pin");
                psl->SetIconLocation(exePath.c_str(), 0);
                IPersistFile* ppf = nullptr;
                if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
                    ppf->Save(sendToLnk.c_str(), TRUE);
                    ppf->Release();
                }
                psl->Release();
            }
        }
    }

    static std::wstring GetStartupShortcutPath() {
        PWSTR pStartup = nullptr;
        std::wstring path = L"";
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &pStartup)) && pStartup) {
            path = std::wstring(pStartup) + L"\\Perdanga11.lnk";
            CoTaskMemFree(pStartup);
        }
        return path;
    }

    static bool IsAutostartEnabled() {
        std::wstring path = GetStartupShortcutPath();
        if (path.empty()) return false;
        return DoesFileExist(path);
    }

    static void SetAutostart(bool enable) {
        std::wstring shortcutPath = GetStartupShortcutPath();
        if (shortcutPath.empty()) return;

        HKEY hRunKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hRunKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hRunKey, L"Perdanga11");
            RegCloseKey(hRunKey);
        }

        if (enable) {
            std::wstring exePath = GetExecutablePath();
            IShellLinkW* psl = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
                psl->SetPath(exePath.c_str());
                std::wstring workDir = exePath.substr(0, exePath.find_last_of(L"\\/"));
                psl->SetWorkingDirectory(workDir.c_str());
                psl->SetIconLocation(exePath.c_str(), 0);

                IPersistFile* ppf = nullptr;
                if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
                    ppf->Save(shortcutPath.c_str(), TRUE);
                    ppf->Release();
                }
                psl->Release();
            }
        } else {
            DeleteFileW(shortcutPath.c_str());
        }
    }

    static std::wstring ResolveAppPath(const std::wstring& target) {
        if (target.empty()) return target;

        wchar_t expanded[MAX_PATH];
        if (ExpandEnvironmentStringsW(target.c_str(), expanded, MAX_PATH)) {
            if (GetFileAttributesW(expanded) != INVALID_FILE_ATTRIBUTES) return expanded;
        }

        if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) return target;

        wchar_t sysDir[MAX_PATH];
        GetSystemDirectoryW(sysDir, MAX_PATH);
        std::wstring check = std::wstring(sysDir) + L"\\" + target;
        if (GetFileAttributesW(check.c_str()) != INVALID_FILE_ATTRIBUTES) return check;

        wchar_t winDir[MAX_PATH];
        GetWindowsDirectoryW(winDir, MAX_PATH);
        check = std::wstring(winDir) + L"\\" + target;
        if (GetFileAttributesW(check.c_str()) != INVALID_FILE_ATTRIBUTES) return check;

        wchar_t resolved[MAX_PATH];
        if (SearchPathW(nullptr, target.c_str(), L".exe", MAX_PATH, resolved, nullptr)) {
            return resolved;
        }
        return target;
    }

    static void SaveTabItem(const TabDefinition& tab, const std::wstring& name, const std::wstring& path) {
        EnsureConfigUnicode();
        std::wstring iniPath = GetConfigPath();
        WritePrivateProfileStringW(tab.section.c_str(), name.c_str(), path.c_str(), iniPath.c_str());
    }

    static void DeleteTabItem(const TabDefinition& tab, const std::wstring& name) {
        std::wstring iniPath = GetConfigPath();
        WritePrivateProfileStringW(tab.section.c_str(), name.c_str(), nullptr, iniPath.c_str());
    }

    static void SaveAllTabItemsOrder(const TabDefinition& tab, const std::vector<AppItem>& items) {
        EnsureConfigUnicode();
        std::wstring iniPath = GetConfigPath();
        WritePrivateProfileStringW(tab.section.c_str(), nullptr, nullptr, iniPath.c_str());
        for (const auto& item : items) {
            WritePrivateProfileStringW(tab.section.c_str(), item.name.c_str(), item.target.c_str(), iniPath.c_str());
        }
    }

    static std::vector<AppItem> LoadTabItems(const TabDefinition& tab);

    static void CleanOrphanedTabSections() {
        std::wstring iniPath = GetConfigPath();
        wchar_t sectionNames[16384] = { 0 };
        DWORD len = GetPrivateProfileSectionNamesW(sectionNames, 16384, iniPath.c_str());
        if (len == 0) return;

        std::unordered_set<std::wstring> validSections;
        validSections.insert(L"settings");
        validSections.insert(L"apps");
        validSections.insert(L"folders");

        for (const auto& tab : g_tabs) {
            validSections.insert(ToLower(L"tab_" + tab.id));
            validSections.insert(ToLower(tab.section));
        }

        wchar_t* ptr = sectionNames;
        while (*ptr != L'\0') {
            std::wstring sec = ptr;
            std::wstring lowerSec = ToLower(sec);

            if (lowerSec.rfind(L"tab_", 0) == 0) {
                if (validSections.count(lowerSec) == 0) {
                    WritePrivateProfileStringW(sec.c_str(), nullptr, nullptr, iniPath.c_str());
                }
            }
            ptr += wcslen(ptr) + 1;
        }
    }

    // NOTE: LoadTabs mutates g_tabs without locking; callers must either hold
    // g_indexMutex already or run before worker threads exist (WM_CREATE)
    static void LoadTabs() {
        EnsureConfigUnicode();
        UpdateThemeCache();
        std::wstring iniPath = GetConfigPath();

        wchar_t langBuf[32] = { 0 };
        GetPrivateProfileStringW(L"Settings", L"Language", L"auto", langBuf, 32, iniPath.c_str());
        if (_wcsicmp(langBuf, L"ru") == 0) g_configuredLanguage = AppLanguage::Russian;
        else if (_wcsicmp(langBuf, L"en") == 0) g_configuredLanguage = AppLanguage::English;
        else g_configuredLanguage = AppLanguage::Auto;

        wchar_t tabsList[2048] = { 0 };
        GetPrivateProfileStringW(L"Settings", L"Tabs", L"", tabsList, 2048, iniPath.c_str());

        g_tabs.clear();

        if (wcslen(tabsList) == 0) {
            TabDefinition tPinned;
            tPinned.id = L"pinned";
            tPinned.name = L"Pinned";
            tPinned.section = L"Apps";
            tPinned.isFolder = false;
            tPinned.visible = true;

            TabDefinition tFolders;
            tFolders.id = L"folders";
            tFolders.name = L"Folders";
            tFolders.section = L"Folders";
            tFolders.isFolder = true;
            tFolders.visible = true;

            g_tabs.push_back(tPinned);
            g_tabs.push_back(tFolders);

            SaveTabs();
        } else {
            std::wstring sList = tabsList;
            size_t start = 0;
            while (start < sList.length() && g_tabs.size() < MAX_TABS) {
                size_t comma = sList.find(L',', start);
                std::wstring tabId = (comma == std::wstring::npos) ? sList.substr(start) : sList.substr(start, comma - start);
                if (!tabId.empty()) {
                    TabDefinition tab;
                    tab.id = tabId;
                    std::wstring sec = L"Tab_" + tabId;

                    wchar_t bufName[256];
                    GetPrivateProfileStringW(sec.c_str(), L"Name", tabId.c_str(), bufName, 256, iniPath.c_str());
                    tab.name = bufName;

                    wchar_t bufSec[256];
                    std::wstring defSec = (tabId == L"pinned") ? L"Apps" : ((tabId == L"folders") ? L"Folders" : sec);
                    GetPrivateProfileStringW(sec.c_str(), L"Section", defSec.c_str(), bufSec, 256, iniPath.c_str());
                    tab.section = bufSec;

                    tab.isFolder = (GetPrivateProfileIntW(sec.c_str(), L"IsFolder", (tabId == L"folders") ? 1 : 0, iniPath.c_str()) != 0);
                    tab.visible = (GetPrivateProfileIntW(sec.c_str(), L"Visible", 1, iniPath.c_str()) != 0);

                    g_tabs.push_back(tab);
                }
                if (comma == std::wstring::npos) break;
                start = comma + 1;
            }
        }

        bool anyVisible = false;
        for (const auto& tab : g_tabs) {
            if (tab.visible) { anyVisible = true; break; }
        }
        if (!anyVisible && !g_tabs.empty()) {
            g_tabs[0].visible = true;
            SaveTabs();
        }

        CleanOrphanedTabSections();

        for (auto& tab : g_tabs) {
            tab.items = LoadTabItems(tab);
        }

        if (g_activeTabIndex >= (int)g_tabs.size() || !g_tabs[g_activeTabIndex].visible) {
            g_activeTabIndex = 0;
            for (size_t i = 0; i < g_tabs.size(); ++i) {
                if (g_tabs[i].visible) {
                    g_activeTabIndex = (int)i;
                    break;
                }
            }
        }
    }

    static void SaveTabs() {
        EnsureConfigUnicode();
        std::wstring iniPath = GetConfigPath();
        std::wstring tabIdList = L"";

        for (size_t i = 0; i < g_tabs.size(); ++i) {
            if (i > 0) tabIdList += L",";
            tabIdList += g_tabs[i].id;

            std::wstring sec = L"Tab_" + g_tabs[i].id;
            WritePrivateProfileStringW(sec.c_str(), L"Name", g_tabs[i].name.c_str(), iniPath.c_str());
            WritePrivateProfileStringW(sec.c_str(), L"Section", g_tabs[i].section.c_str(), iniPath.c_str());
            WritePrivateProfileStringW(sec.c_str(), L"IsFolder", g_tabs[i].isFolder ? L"1" : L"0", iniPath.c_str());
            WritePrivateProfileStringW(sec.c_str(), L"Visible", g_tabs[i].visible ? L"1" : L"0", iniPath.c_str());
        }

        WritePrivateProfileStringW(L"Settings", L"Tabs", tabIdList.c_str(), iniPath.c_str());
    }

    static TabDefinition& GetActiveTab() {
        if (g_activeTabIndex >= 0 && g_activeTabIndex < (int)g_tabs.size()) {
            return g_tabs[g_activeTabIndex];
        }
        return g_tabs[0];
    }

    static void AddTab(const std::wstring& name) {
        if (!CanAddTab()) return;

        std::wstring safeId = L"tab_" + std::to_wstring(GetTickCount());
        TabDefinition newTab;
        newTab.id = safeId;
        newTab.name = name;
        newTab.section = L"Tab_" + safeId;
        newTab.isFolder = false;
        newTab.visible = true;

        std::lock_guard<std::mutex> lock(g_indexMutex);
        g_tabs.push_back(newTab);
        SaveTabs();
        g_activeTabIndex = (int)g_tabs.size() - 1;
    }

    static void RenameTab(int index, const std::wstring& newName) {
        std::lock_guard<std::mutex> lock(g_indexMutex);
        if (index >= 0 && index < (int)g_tabs.size()) {
            g_tabs[index].name = newName;
            SaveTabs();
        }
    }

    static void DeleteTab(int index) {
        std::lock_guard<std::mutex> lock(g_indexMutex);
        if (g_tabs.size() <= 1 || index < 0 || index >= (int)g_tabs.size()) return;
        std::wstring iniPath = GetConfigPath();
        std::wstring tabId = g_tabs[index].id;
        std::wstring tabSection = g_tabs[index].section;

        WritePrivateProfileStringW((L"Tab_" + tabId).c_str(), nullptr, nullptr, iniPath.c_str());

        if (_wcsicmp(tabSection.c_str(), L"Apps") != 0 && _wcsicmp(tabSection.c_str(), L"Folders") != 0) {
            WritePrivateProfileStringW(tabSection.c_str(), nullptr, nullptr, iniPath.c_str());
        }

        g_tabs.erase(g_tabs.begin() + index);
        SaveTabs();
        CleanOrphanedTabSections();
        LoadTabs();
    }

    static void ToggleTabVisibility(int index) {
        std::lock_guard<std::mutex> lock(g_indexMutex);
        if (index < 0 || index >= (int)g_tabs.size()) return;
        int visibleCount = 0;
        for (const auto& tab : g_tabs) if (tab.visible) visibleCount++;
        if (g_tabs[index].visible && visibleCount <= 1) return;

        g_tabs[index].visible = !g_tabs[index].visible;
        SaveTabs();
        LoadTabs();
    }

    static void StartAsyncIndexing(HWND notifyWnd);
    static HICON ExtractCleanIcon(const std::wstring& path, bool isDir = false);
    static HICON GetDefaultFolderIcon();
};

#include "AppIndexer.hpp"

inline void Config::StartAsyncIndexing(HWND notifyWnd) {
    AppIndexer::StartAsyncIndexing(notifyWnd);
}

inline HICON Config::ExtractCleanIcon(const std::wstring& path, bool isDir) {
    return AppIndexer::ExtractCleanIcon(path, isDir);
}

inline HICON Config::GetDefaultFolderIcon() {
    return AppIndexer::GetDefaultFolderIcon();
}

inline std::vector<AppItem> Config::LoadTabItems(const TabDefinition& tab) {
    std::vector<AppItem> list;
    std::wstring iniPath = GetConfigPath();

    wchar_t buffer[16384];
    DWORD charsRead = GetPrivateProfileSectionW(tab.section.c_str(), buffer, sizeof(buffer) / sizeof(wchar_t), iniPath.c_str());

    if (charsRead == 0 && tab.isFolder && tab.id == L"folders") {
        std::vector<AppItem> defaultFrequent = AppIndexer::ScanFrequentFolders();
        SaveAllTabItemsOrder(tab, defaultFrequent);
        return defaultFrequent;
    }

    wchar_t* ptr = buffer;
    while (*ptr != L'\0') {
        std::wstring line(ptr);
        size_t eqPos = line.find(L'=');
        if (eqPos != std::wstring::npos) {
            AppItem item;
            item.name = line.substr(0, eqPos);
            item.target = line.substr(eqPos + 1);
            item.lowerName = Config::ToLower(item.name);
            item.lowerTarget = Config::ToLower(item.target);

            size_t dot = item.target.find_last_of(L'.');
            item.lowerExt = (dot != std::wstring::npos) ? Config::ToLower(item.target.substr(dot)) : L"";

            DWORD targetAttr = GetFileAttributesW(item.target.c_str());
            item.isDirectory = (targetAttr != INVALID_FILE_ATTRIBUTES && (targetAttr & FILE_ATTRIBUTE_DIRECTORY));

            if (item.isDirectory) {
                if (DoesFolderExist(item.target)) {
                    item.hIcon = AppIndexer::ExtractCleanIcon(item.target, true);
                    list.push_back(item);
                }
            } else {
                std::wstring resolved = ResolveAppPath(item.target);
                if (GetFileAttributesW(resolved.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    item.hIcon = AppIndexer::ExtractCleanIcon(item.target, false);
                    list.push_back(item);
                }
            }
        }
        ptr += wcslen(ptr) + 1;
    }

    return list;
}