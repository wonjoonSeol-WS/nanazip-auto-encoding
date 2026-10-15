// Prints the menus the shell builds for the registered handlers, the way Explorer builds them.
//   test_shellmenu.exe click FILE...          right-click menu of the files; nothing is shown
//   test_shellmenu.exe drop FOLDER FILE...    a real right-button drop onto FOLDER; the popup menu
//                                             shows on screen for a moment and is then cancelled
// It loads the registered handler, so run the installer first.
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstdio>
#include <cwchar>
#include <io.h>
#include <fcntl.h>
#include <vector>

static volatile LONG g_dropDone = 0;

static void PrintMenu(HMENU menu) {
    const int n = GetMenuItemCount(menu);
    wprintf(L"menu items: %d\n", n);
    for (int i = 0; i < n; i++) {
        wchar_t text[512] = {};
        GetMenuStringW(menu, i, text, 512, MF_BYPOSITION);
        wprintf(L"  %ls\n", text[0] ? text : L"-----");
    }
}

// Waits for our popup menu for as long as Drop has not returned, prints it and dismisses it
static DWORD WINAPI WatchMenu(LPVOID) {
    const DWORD pid = GetCurrentProcessId();
    while (!g_dropDone) {
        Sleep(50);
        HWND w = nullptr;
        while ((w = FindWindowExW(nullptr, w, L"#32768", nullptr)) != nullptr) {
            DWORD owner = 0;
            GetWindowThreadProcessId(w, &owner);
            if (owner != pid || !IsWindowVisible(w)) continue;
            HMENU menu = nullptr;
            for (int wait = 0; wait < 20 && GetMenuItemCount(menu) <= 0; wait++) {  // let every handler add its items
                Sleep(100);
                DWORD_PTR result = 0;
                SendMessageTimeoutW(w, MN_GETHMENU, 0, 0, SMTO_ABORTIFHUNG, 1000, &result);
                menu = reinterpret_cast<HMENU>(result);
            }
            PrintMenu(menu);
            PostMessageW(w, WM_KEYDOWN, VK_ESCAPE, 0);
            return 0;
        }
    }
    wprintf(L"no menu appeared\n");
    return 0;
}

static bool ParseItems(int argc, wchar_t** argv, int first, std::vector<PIDLIST_ABSOLUTE>& items) {
    for (int i = first; i < argc; i++) {
        PIDLIST_ABSOLUTE p = nullptr;
        if (FAILED(SHParseDisplayName(argv[i], nullptr, &p, 0, nullptr))) {
            wprintf(L"bad path %ls\n", argv[i]);
            return false;
        }
        items.push_back(p);
    }
    return true;
}

static int Click(int argc, wchar_t** argv) {
    std::vector<PIDLIST_ABSOLUTE> items;
    if (!ParseItems(argc, argv, 2, items)) return 2;
    IShellFolder* parent = nullptr;
    PCUITEMID_CHILD child = nullptr;
    if (FAILED(SHBindToParent(items[0], IID_PPV_ARGS(&parent), &child))) return 2;
    std::vector<PCUITEMID_CHILD> children;
    for (PIDLIST_ABSOLUTE p : items) children.push_back(ILFindLastID(p));
    IContextMenu* cm = nullptr;
    if (FAILED(parent->GetUIObjectOf(nullptr, static_cast<UINT>(children.size()), children.data(), IID_IContextMenu, nullptr,
                                     reinterpret_cast<void**>(&cm)))) {
        return 2;
    }
    HMENU menu = CreatePopupMenu();
    cm->QueryContextMenu(menu, 0, 1, 0x7FFF, CMF_NORMAL);
    PrintMenu(menu);
    return 0;
}

static int Drop(int argc, wchar_t** argv) {
    PIDLIST_ABSOLUTE folderPidl = nullptr;
    IShellFolder* folder = nullptr;
    if (FAILED(SHParseDisplayName(argv[2], nullptr, &folderPidl, 0, nullptr)) ||
        FAILED(SHBindToObject(nullptr, folderPidl, nullptr, IID_PPV_ARGS(&folder)))) {
        wprintf(L"bad target folder\n");
        return 2;
    }
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"dropmenu", WS_POPUP | WS_VISIBLE, 200, 200, 10, 10, nullptr, nullptr, nullptr, nullptr);
    SetForegroundWindow(hwnd);
    IDropTarget* target = nullptr;
    if (FAILED(folder->CreateViewObject(hwnd, IID_PPV_ARGS(&target)))) return 2;
    std::vector<PIDLIST_ABSOLUTE> items;
    if (!ParseItems(argc, argv, 3, items)) return 2;
    IShellItemArray* array = nullptr;
    IDataObject* data = nullptr;
    if (FAILED(SHCreateShellItemArrayFromIDLists(static_cast<UINT>(items.size()), const_cast<PCIDLIST_ABSOLUTE_ARRAY>(items.data()), &array)) ||
        FAILED(array->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&data)))) {
        return 2;
    }
    // Copy only, so the shell's default action can never move the files
    POINTL pt = {220, 220};
    DWORD effect = DROPEFFECT_COPY;
    target->DragEnter(data, MK_RBUTTON, pt, &effect);
    effect = DROPEFFECT_COPY;
    target->DragOver(MK_RBUTTON, pt, &effect);
    HANDLE watcher = CreateThread(nullptr, 0, WatchMenu, nullptr, 0, nullptr);
    effect = DROPEFFECT_COPY;
    target->Drop(data, MK_RBUTTON, pt, &effect);
    InterlockedExchange(&g_dropDone, 1);
    WaitForSingleObject(watcher, 5000);
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U8TEXT);
    if (FAILED(OleInitialize(nullptr))) return 2;
    if (argc >= 3 && wcscmp(argv[1], L"click") == 0) return Click(argc, argv);
    if (argc >= 4 && wcscmp(argv[1], L"drop") == 0) return Drop(argc, argv);
    wprintf(L"usage: test_shellmenu.exe click FILE... | drop FOLDER FILE...\n");
    return 2;
}
