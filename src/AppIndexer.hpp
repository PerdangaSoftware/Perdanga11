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

class IconCache {
private:
    static inline std::unordered_map<std::wstring, HICON> s_extCache;
    static inline std::unordered_map<std::wstring, HICON> s_fileIconCache;
    static inline std::unordered_map<std::wstring, HICON> s_shortcutIconCache;
    static inline std::mutex s_cacheMutex;
    static inline HICON s_defaultFolderIcon = nullptr;
    static inline HICON s_defaultFileIcon = nullptr;

public:
    static HICON GetDefaultFolderIcon() {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        if (!s_defaultFolderIcon) {
            SHFILEINFOW sfi = { 0 };
            if (SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)) {
                s_defaultFolderIcon = sfi.hIcon;
            }
        }
        return s_defaultFolderIcon;
    }

    static HICON GetDefaultFileIcon() {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        if (!s_defaultFileIcon) {
            s_defaultFileIcon = LoadIconW(nullptr, IDI_APPLICATION);
        }
        return s_defaultFileIcon;
    }

    static HICON GetExtensionIcon(const std::wstring& lowerExt) {
        if (lowerExt.empty()) return GetDefaultFileIcon();

        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_extCache.find(lowerExt);
        if (it != s_extCache.end() && it->second) {
            return it->second;
        }

        SHFILEINFOW sfi = { 0 };
        if (SHGetFileInfoW(lowerExt.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES) && sfi.hIcon) {
            s_extCache[lowerExt] = sfi.hIcon;
            return sfi.hIcon;
        }

        return GetDefaultFileIcon();
    }

    static HICON GetCachedFileIcon(const std::wstring& path) {
        std::wstring norm = Config::ToLower(path);
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            auto it = s_fileIconCache.find(norm);
            if (it != s_fileIconCache.end() && it->second) {
                return it->second;
            }
        }

        HICON hExtracted = nullptr;
        ExtractIconExW(path.c_str(), 0, &hExtracted, nullptr, 1);
        if (hExtracted) {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            s_fileIconCache[norm] = hExtracted;
            return hExtracted;
        }

        size_t dotPos = norm.find_last_of(L'.');
        std::wstring ext = (dotPos != std::wstring::npos) ? norm.substr(dotPos) : L"";
        return GetExtensionIcon(ext);
    }

    // Icons referenced by .lnk shortcuts must be cached: ExtractIconExW creates a new
    // HICON on every call and pinned items are reloaded on each menu open (leak source)
    static HICON GetShortcutIcon(const std::wstring& iconPath, int iconIndex) {
        std::wstring key = Config::ToLower(iconPath) + L"|" + std::to_wstring(iconIndex);
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            auto it = s_shortcutIconCache.find(key);
            if (it != s_shortcutIconCache.end()) {
                return it->second;
            }
        }

        HICON hExtracted = nullptr;
        ExtractIconExW(iconPath.c_str(), iconIndex, &hExtracted, nullptr, 1);

        std::lock_guard<std::mutex> lock(s_cacheMutex);
        s_shortcutIconCache[key] = hExtracted;
        return hExtracted;
    }
};

class AppIndexer {
public:
    static HICON GetDefaultFolderIcon() {
        return IconCache::GetDefaultFolderIcon();
    }

    static std::wstring ResolveLnkTarget(const std::wstring& lnkPath, std::wstring& outIconPath, int& outIconIndex) {
        IShellLinkW* psl = nullptr;
        std::wstring targetPath = lnkPath;
        outIconPath = L"";
        outIconIndex = 0;

        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl))) {
            IPersistFile* ppf = nullptr;
            if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
                if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
                    wchar_t szTarget[MAX_PATH] = { 0 };
                    WIN32_FIND_DATAW wfd;
                    if (SUCCEEDED(psl->GetPath(szTarget, MAX_PATH, &wfd, SLGP_UNCPRIORITY))) {
                        if (wcslen(szTarget) > 0) targetPath = szTarget;
                    }

                    wchar_t szIcon[MAX_PATH] = { 0 };
                    int nIcon = 0;
                    if (SUCCEEDED(psl->GetIconLocation(szIcon, MAX_PATH, &nIcon))) {
                        if (wcslen(szIcon) > 0) {
                            outIconPath = szIcon;
                            outIconIndex = nIcon;
                        }
                    }
                }
                ppf->Release();
            }
            psl->Release();
        }
        return targetPath;
    }

    static HICON ExtractCleanIcon(const std::wstring& path, bool isDir = false) {
        if (isDir) {
            if (path.length() <= 3 && path.find(L':') != std::wstring::npos) {
                SHFILEINFOW sfi = { 0 };
                if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) && sfi.hIcon) {
                    return sfi.hIcon;
                }
            }
            return IconCache::GetDefaultFolderIcon();
        }

        std::wstring targetPath = path;
        std::wstring iconPath = L"";
        int iconIndex = 0;

        wchar_t expandedInput[MAX_PATH];
        if (ExpandEnvironmentStringsW(path.c_str(), expandedInput, MAX_PATH)) {
            targetPath = expandedInput;
        }

        size_t dotPos = targetPath.find_last_of(L'.');
        std::wstring ext = (dotPos != std::wstring::npos) ? Config::ToLower(targetPath.substr(dotPos)) : L"";

        if (ext == L".lnk") {
            targetPath = ResolveLnkTarget(targetPath, iconPath, iconIndex);
            dotPos = targetPath.find_last_of(L'.');
            ext = (dotPos != std::wstring::npos) ? Config::ToLower(targetPath.substr(dotPos)) : L"";
        }

        if (!iconPath.empty()) {
            HICON hShortcutIcon = IconCache::GetShortcutIcon(iconPath, iconIndex);
            if (hShortcutIcon) return hShortcutIcon;
        }

        if (ext == L".exe") {
            return IconCache::GetCachedFileIcon(targetPath);
        }

        if (!ext.empty()) {
            return IconCache::GetExtensionIcon(ext);
        }

        return IconCache::GetDefaultFileIcon();
    }

    static std::wstring CleanAppName(const std::wstring& rawName) {
        std::wstring s = rawName;
        for (auto& c : s) {
            if (c == 0x00A0) c = L' ';
        }
        while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
        while (!s.empty() && iswspace(s.back())) s.pop_back();

        static const wchar_t* archTags[] = { L" (x64)", L" (x86)", L" (64-bit)", L" (32-bit)", L" - 64-bit", L" - 32-bit" };
        for (const wchar_t* tag : archTags) {
            size_t pos = s.find(tag);
            if (pos != std::wstring::npos) {
                s.erase(pos, wcslen(tag));
            }
        }
        return s;
    }

    static bool IsJunkOrSecondaryShortcut(const std::wstring& name, const std::wstring& fullShortcutPath, const std::wstring& resolvedTarget) {
        std::wstring lowerName = Config::ToLower(name);
        std::wstring lowerPath = Config::ToLower(fullShortcutPath);
        std::wstring lowerTarget = Config::ToLower(resolvedTarget);

        if (lowerPath.find(L"\\administrative tools") != std::wstring::npos ||
            lowerPath.find(L"\\windows tools") != std::wstring::npos ||
            lowerPath.find(L"\\system tools") != std::wstring::npos ||
            lowerPath.find(L"\\accessibility") != std::wstring::npos) {
            return true;
        }

        if (lowerName == L"voiceaccess" || lowerName == L"voice access" ||
            lowerName == L"narrator" || lowerName == L"magnify" ||
            lowerName == L"on-screen keyboard" || lowerName == L"livecaptions") {
            return true;
        }

        static const wchar_t* junkKeywords[] = {
            L"uninstall", L"deinstall", L"uninst", L"installer", L"setup",
            L"error report", L"crash report", L"crashreporter", L"bug report", L"feedback",
            L"recovery", L"diagnostic", L"troubleshoot", L"repair", L"cleaner",
            L"readme", L"read me", L"read_me", L"license", L"eula", L"terms", L"privacy",
            L"help", L"manual", L"guide", L"faq", L"documentation", L"release notes", L"changelog",
            L"website", L"homepage", L"online support",
            L"sample desktop", L"sample uwp", L"tools for desktop", L"tools for uwp",
            L"application verifier", L"app cert kit", L"cert kit", L"developer command",
            L"developer powershell", L"database compare", L"spreadsheet compare",
            L"private browsing", L"auto-start", L"autostart", L"updater", L"check for update",
            L"check updates", L"native tools", L"cross tools", L"install additional",
            L"module docs", L"manuals",
            L"\x0434\x0435\x0438\x043D\x0441\x0442\x0430\x043B\x043B",
            L"\x0443\x0434\x0430\x043B\x0438\x0442\x044C",
            L"\x0443\x0434\x0430\x043B\x0435\x043D\x0438\x0435",
            L"\x043E\x0431\x043D\x043E\x0432\x043B\x0435\x043D\x0438\x0435",
            L"\x0436\x0443\x0440\x043D\x0430\x043B",
            L"\x044F\x0437\x044B\x043A\x043E\x0432\x044B\x0435",
            L"\x0441\x043F\x0440\x0430\x0432\x043A\x0430",
            L"\x0440\x0443\x043A\x043E\x0432\x043E\x0434\x0441\x0442\x0432\x043E",
            L"\x043F\x043E\x043C\x043E\x0449\x044C",
            L"\x043A\x043E\x0440\x0437\x0438\x043D\x0430"
        };

        for (const wchar_t* junk : junkKeywords) {
            if (lowerName.find(junk) != std::wstring::npos) return true;
        }

        if (lowerName == L"git cmd" || lowerName == L"git gui") return true;
        if (lowerName == L"dfrgui" || lowerName == L"services") return true;

        size_t dotPos = lowerTarget.find_last_of(L'.');
        if (dotPos == std::wstring::npos) return true;
        std::wstring ext = lowerTarget.substr(dotPos);
        return (ext != L".exe");
    }

    static void ScanDirectoryForShortcuts(const std::wstring& directory, std::vector<AppItem>& outList,
                                          std::unordered_set<std::wstring>& seenNames,
                                          std::unordered_set<std::wstring>& seenTargets) {
        std::wstring searchPath = directory + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

                std::wstring fullPath = directory + L"\\" + fd.cFileName;

                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    ScanDirectoryForShortcuts(fullPath, outList, seenNames, seenTargets);
                } else {
                    std::wstring fileName = fd.cFileName;
                    size_t dotPos = fileName.find_last_of(L'.');
                    if (dotPos != std::wstring::npos) {
                        std::wstring ext = fileName.substr(dotPos);
                        for (auto& c : ext) c = towlower(c);

                        if (ext == L".lnk") {
                            std::wstring appName = fileName.substr(0, dotPos);

                            std::wstring iconPath;
                            int iconIdx = 0;
                            std::wstring resolvedTarget = ResolveLnkTarget(fullPath, iconPath, iconIdx);
                            resolvedTarget = Config::ResolveAppPath(resolvedTarget);

                            if (IsJunkOrSecondaryShortcut(appName, fullPath, resolvedTarget)) continue;

                            appName = CleanAppName(appName);

                            std::wstring normTarget = Config::ToLower(resolvedTarget);
                            std::wstring normName = Config::ToLower(appName);

                            if (seenNames.count(normName) > 0) continue;
                            if (!normTarget.empty() && seenTargets.count(normTarget) > 0) continue;

                            seenNames.insert(normName);
                            if (!normTarget.empty()) seenTargets.insert(normTarget);

                            AppItem item;
                            item.name = appName;
                            item.target = fullPath;
                            item.lowerName = normName;
                            item.lowerExt = L".lnk";
                            item.lowerTarget = normTarget;
                            item.isDirectory = false;
                            item.hIcon = ExtractCleanIcon(fullPath, false);
                            outList.push_back(item);
                        }
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    static std::vector<AppItem> ScanFrequentFolders() {
        std::vector<AppItem> folders;
        std::unordered_set<std::wstring> seenPaths;

        auto addFolderItem = [&](const std::wstring& name, const std::wstring& path) {
            if (path.empty()) return;
            if (!Config::DoesFolderExist(path)) return;

            std::wstring norm = Config::ToLower(path);
            while (norm.length() > 3 && (norm.back() == L'\\' || norm.back() == L'/')) {
                norm.pop_back();
            }
            if (seenPaths.count(norm) > 0) return;
            seenPaths.insert(norm);

            AppItem item;
            item.name = name;
            item.target = path;
            item.lowerName = Config::ToLower(name);
            item.lowerExt = L"";
            item.lowerTarget = norm;
            item.isDirectory = true;
            item.hIcon = ExtractCleanIcon(path, true);
            folders.push_back(item);
        };

        const struct FolderDef {
            const wchar_t* name;
            KNOWNFOLDERID fid;
        } standardFolders[] = {
            { L"Desktop",   FOLDERID_Desktop },
            { L"Downloads", FOLDERID_Downloads },
            { L"Documents", FOLDERID_Documents },
            { L"Pictures",  FOLDERID_Pictures },
            { L"Videos",    FOLDERID_Videos },
            { L"Music",     FOLDERID_Music }
        };

        for (const auto& fdef : standardFolders) {
            PWSTR pPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(fdef.fid, 0, nullptr, &pPath)) && pPath) {
                addFolderItem(fdef.name, pPath);
                CoTaskMemFree(pPath);
            }
        }

        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (drives & (1 << i)) {
                wchar_t driveLetter[] = { (wchar_t)(L'A' + i), L':', L'\\', L'\0' };
                UINT type = GetDriveTypeW(driveLetter);
                if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                    wchar_t volName[MAX_PATH] = { 0 };
                    std::wstring driveName;
                    if (GetVolumeInformationW(driveLetter, volName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0) && wcslen(volName) > 0) {
                        driveName = std::wstring(volName) + L" (" + std::wstring(driveLetter, 2) + L")";
                    } else {
                        driveName = std::wstring(L"Disk (") + std::wstring(driveLetter, 2) + L")";
                    }
                    addFolderItem(driveName, driveLetter);
                }
            }
        }

        return folders;
    }

    static bool IsSupportedUserFileExtension(const std::wstring& lowerExt) {
        static const std::unordered_set<std::wstring> allowedExtensions = {
            L".txt", L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".webp",
            L".pdf", L".doc", L".docx", L".xls", L".xlsx", L".ppt", L".pptx",
            L".rtf", L".csv", L".md", L".log", L".json", L".xml", L".ini",
            L".zip", L".rar", L".7z", L".tar", L".gz",
            L".mp3", L".wav", L".flac", L".mp4", L".mkv", L".avi",
            L".cpp", L".h", L".hpp", L".cs", L".py", L".js", L".ts", L".html", L".css"
        };
        return allowedExtensions.count(lowerExt) > 0;
    }

    static bool ShouldSkipDirectory(const wchar_t* dirName) {
        if (!dirName || dirName[0] == L'\0') return true;
        if (dirName[0] == L'$' || dirName[0] == L'.') return true;

        static const wchar_t* skipList[] = {
            L"System Volume Information", L"Windows", L"Program Files", L"Program Files (x86)",
            L"ProgramData", L"AppData", L"node_modules", L"Recovery", L"MSOCache",
            L"Config.Msi", L"WindowsApps", L"WpSystem", L"steamapps", L"vendor",
            L"bin", L"obj", L".git", L".svn", L".vs"
        };
        for (const wchar_t* skip : skipList) {
            if (_wcsicmp(dirName, skip) == 0) return true;
        }
        return false;
    }

    static void ScanDirectoryForItems(const std::wstring& directory, int maxDepth, int currentDepth,
                                      std::vector<AppItem>& outItems, std::unordered_set<std::wstring>& seen) {
        if (currentDepth > maxDepth || outItems.size() > 40000) return;

        std::wstring pattern = directory;
        if (pattern.back() != L'\\') pattern += L'\\';
        pattern += L'*';

        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            if (fd.cFileName[0] == L'$') continue;

            std::wstring fullPath = directory;
            if (fullPath.back() != L'\\') fullPath += L'\\';
            fullPath += fd.cFileName;

            std::wstring norm = Config::ToLower(fullPath);

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (ShouldSkipDirectory(fd.cFileName)) continue;

                if (seen.count(norm) == 0) {
                    seen.insert(norm);
                    AppItem folderItem;
                    folderItem.name = fd.cFileName;
                    folderItem.target = fullPath;
                    folderItem.lowerName = Config::ToLower(fd.cFileName);
                    folderItem.lowerExt = L"";
                    folderItem.lowerTarget = norm;
                    folderItem.isDirectory = true;
                    folderItem.hIcon = IconCache::GetDefaultFolderIcon();
                    outItems.push_back(folderItem);
                }

                ScanDirectoryForItems(fullPath, maxDepth, currentDepth + 1, outItems, seen);
            } else {
                std::wstring fName = fd.cFileName;
                if (_wcsicmp(fName.c_str(), L"desktop.ini") == 0) continue;

                size_t dotPos = fName.find_last_of(L'.');
                if (dotPos == std::wstring::npos) continue;

                std::wstring ext = Config::ToLower(fName.substr(dotPos));
                if (!IsSupportedUserFileExtension(ext)) continue;

                if (seen.count(norm) == 0) {
                    seen.insert(norm);
                    AppItem item;
                    item.name = fName;
                    item.target = fullPath;
                    item.lowerName = Config::ToLower(fName);
                    item.lowerExt = ext;
                    item.lowerTarget = norm;
                    item.isDirectory = false;
                    item.hIcon = IconCache::GetExtensionIcon(ext);
                    outItems.push_back(item);
                }
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }

    static std::vector<AppItem> ScanUserFilesAndFolders() {
        std::vector<AppItem> items;
        std::unordered_set<std::wstring> seen;

        const struct TargetLoc {
            KNOWNFOLDERID fid;
            int maxDepth;
        } locations[] = {
            { FOLDERID_Desktop,       3 },
            { FOLDERID_PublicDesktop, 2 },
            { FOLDERID_Documents,     2 },
            { FOLDERID_Downloads,     1 },
            { FOLDERID_Pictures,      2 }
        };

        for (const auto& loc : locations) {
            PWSTR pPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(loc.fid, 0, nullptr, &pPath)) && pPath) {
                ScanDirectoryForItems(pPath, loc.maxDepth, 0, items, seen);
                CoTaskMemFree(pPath);
            }
        }

        wchar_t sysDriveLetter = L'C';
        wchar_t winDir[MAX_PATH] = { 0 };
        if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
            sysDriveLetter = (wchar_t)towupper(winDir[0]);
        }

        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if (drives & (1 << i)) {
                wchar_t driveLetterChar = (wchar_t)(L'A' + i);
                wchar_t driveRoot[] = { driveLetterChar, L':', L'\\', L'\0' };

                UINT type = GetDriveTypeW(driveRoot);
                if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                    if (towupper(driveLetterChar) == sysDriveLetter) {
                        ScanDirectoryForItems(driveRoot, 2, 0, items, seen);
                    } else {
                        ScanDirectoryForItems(driveRoot, 3, 0, items, seen);
                    }
                }
            }
        }

        return items;
    }

    static void StartAsyncIndexing(HWND notifyWnd) {
        std::thread([notifyWnd]() {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

            std::vector<AppItem> apps;
            std::unordered_set<std::wstring> seenNames;
            std::unordered_set<std::wstring> seenTargets;

            PWSTR commonPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, nullptr, &commonPath)) && commonPath) {
                ScanDirectoryForShortcuts(commonPath, apps, seenNames, seenTargets);
                CoTaskMemFree(commonPath);
            }

            PWSTR userPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &userPath)) && userPath) {
                ScanDirectoryForShortcuts(userPath, apps, seenNames, seenTargets);
                CoTaskMemFree(userPath);
            }

            PWSTR desktopPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktopPath)) && desktopPath) {
                ScanDirectoryForShortcuts(desktopPath, apps, seenNames, seenTargets);
                CoTaskMemFree(desktopPath);
            }

            PWSTR pubDesktopPath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop, 0, nullptr, &pubDesktopPath)) && pubDesktopPath) {
                ScanDirectoryForShortcuts(pubDesktopPath, apps, seenNames, seenTargets);
                CoTaskMemFree(pubDesktopPath);
            }

            std::sort(apps.begin(), apps.end(), [](const AppItem& a, const AppItem& b) {
                return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
            });

            std::vector<AppItem> frequentFolders = ScanFrequentFolders();
            std::vector<AppItem> userItems = ScanUserFilesAndFolders();

            auto snapshot = std::make_shared<IndexSnapshot>();
            snapshot->installedApps = std::move(apps);
            snapshot->frequentFolders = std::move(frequentFolders);
            snapshot->userFiles = std::move(userItems);

            {
                // Publish the immutable snapshot under a short lock; search workers
                // hold their own shared_ptr copy and iterate it without locking
                std::lock_guard<std::mutex> lock(Config::g_indexMutex);
                Config::g_indexSnapshot = std::move(snapshot);
                Config::g_isIndexingComplete = true;
            }

            if (notifyWnd) {
                PostMessageW(notifyWnd, WM_APP_INDEX_READY, 0, 0);
            }

            CoUninitialize();
        }).detach();
    }

    static std::wstring ConvertKeyboardLayout(const std::wstring& input) {
        static const std::unordered_map<wchar_t, wchar_t> ruToEn = {
            {L'й', L'q'}, {L'ц', L'w'}, {L'у', L'e'}, {L'к', L'r'}, {L'е', L't'}, {L'н', L'y'},
            {L'г', L'u'}, {L'ш', L'i'}, {L'щ', L'o'}, {L'з', L'p'}, {L'х', L'['}, {L'ъ', L']'},
            {L'ф', L'a'}, {L'ы', L's'}, {L'в', L'd'}, {L'а', L'f'}, {L'п', L'g'}, {L'р', L'h'},
            {L'о', L'j'}, {L'л', L'k'}, {L'д', L'l'}, {L'ж', L';'}, {L'э', L'\''}, {L'я', L'z'},
            {L'ч', L'x'}, {L'с', L'c'}, {L'м', L'v'}, {L'и', L'b'}, {L'т', L'n'}, {L'ь', L'm'},
            {L'б', L','}, {L'ю', L'.'}
        };

        std::wstring result = input;
        for (auto& c : result) {
            wchar_t low = towlower(c);
            auto it = ruToEn.find(low);
            if (it != ruToEn.end()) {
                c = it->second;
            }
        }
        return result;
    }

    // Levenshtein using fixed stack buffer to avoid heap vector allocations during search
    static int LevenshteinDistance(const std::wstring& s1, const std::wstring& s2) {
        const size_t len1 = s1.size(), len2 = s2.size();
        if (len1 == 0) return (int)len2;
        if (len2 == 0) return (int)len1;
        if (len2 >= 64) return 99;

        int col[65];
        for (size_t y = 0; y <= len2; ++y) col[y] = (int)y;

        for (size_t x = 1; x <= len1; ++x) {
            int lastDiag = col[0];
            col[0] = (int)x;
            for (size_t y = 1; y <= len2; ++y) {
                int oldCol = col[y];
                int cost = (s1[x - 1] == s2[y - 1]) ? 0 : 1;
                col[y] = (std::min)({ col[y] + 1, col[y - 1] + 1, lastDiag + cost });
                lastDiag = oldCol;
            }
        }
        return col[len2];
    }

    // High-speed matching that relies on query variants precomputed once per search
    static int CalculateItemScoreFast(const AppItem& item,
                                      const std::wstring& lowerQuery,
                                      const std::wstring& convertedQuery,
                                      const std::vector<std::wstring>& tokens,
                                      const std::vector<std::wstring>& convertedTokens) {
        if (lowerQuery.empty()) return 100;

        // 1. Exact match on item name
        if (item.lowerName == lowerQuery) {
            return 100;
        }

        // 2. Direct prefix match on item name
        if (item.lowerName.rfind(lowerQuery, 0) == 0) {
            return 96;
        }

        // 3. Substring match on item name
        size_t namePos = item.lowerName.find(lowerQuery);
        if (namePos != std::wstring::npos) {
            return 92 - (int)(std::min)((size_t)15, namePos);
        }

        // 4. Converted keyboard layout match on item name
        if (convertedQuery != lowerQuery) {
            if (item.lowerName == convertedQuery) return 98;
            if (item.lowerName.rfind(convertedQuery, 0) == 0) return 94;
            size_t cpos = item.lowerName.find(convertedQuery);
            if (cpos != std::wstring::npos) return 88 - (int)(std::min)((size_t)15, cpos);
        }

        // 5. Multi-token match without per-item string conversions
        if (tokens.size() > 1) {
            bool allInName = true;
            for (size_t t = 0; t < tokens.size(); ++t) {
                if (item.lowerName.find(tokens[t]) == std::wstring::npos) {
                    if (convertedTokens[t].empty() || item.lowerName.find(convertedTokens[t]) == std::wstring::npos) {
                        allInName = false;
                        break;
                    }
                }
            }
            if (allInName) {
                return 95;
            }

            bool allFound = true;
            for (size_t t = 0; t < tokens.size(); ++t) {
                bool inThis = (item.lowerName.find(tokens[t]) != std::wstring::npos) ||
                              (!item.lowerExt.empty() && item.lowerExt.find(tokens[t]) != std::wstring::npos) ||
                              (!item.lowerTarget.empty() && item.lowerTarget.find(tokens[t]) != std::wstring::npos);
                if (!inThis && !convertedTokens[t].empty()) {
                    inThis = (item.lowerName.find(convertedTokens[t]) != std::wstring::npos) ||
                             (!item.lowerExt.empty() && item.lowerExt.find(convertedTokens[t]) != std::wstring::npos) ||
                             (!item.lowerTarget.empty() && item.lowerTarget.find(convertedTokens[t]) != std::wstring::npos);
                }
                if (!inThis) {
                    allFound = false;
                    break;
                }
            }
            if (allFound) {
                return 72;
            }
        }

        // 6. Direct extension filter
        if (!item.lowerExt.empty()) {
            if (lowerQuery == item.lowerExt || (lowerQuery.length() + 1 == item.lowerExt.length() && item.lowerExt.compare(1, lowerQuery.length(), lowerQuery) == 0)) {
                return 80;
            }
        }

        // 7. Substring match on file path
        if (!item.lowerTarget.empty() && item.lowerTarget.find(lowerQuery) != std::wstring::npos) {
            return 68;
        }

        // 8. Fuzzy match for longer queries
        if (lowerQuery.length() >= 4 && item.lowerName.length() >= 3 && item.lowerName.length() <= 32) {
            int dist = LevenshteinDistance(item.lowerName.substr(0, (std::min)(item.lowerName.length(), lowerQuery.length())), lowerQuery);
            if (dist <= 2) return 55 - (dist * 10);
        }

        return 0;
    }
};