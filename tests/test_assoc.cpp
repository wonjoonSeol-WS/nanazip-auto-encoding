// Lists the "Open with" handlers Windows offers for a file extension: test_assoc.exe .zip
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstdio>
#include <io.h>
#include <fcntl.h>

int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U8TEXT);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
    IEnumAssocHandlers* handlers = nullptr;
    const HRESULT hr = SHAssocEnumHandlers(argc > 1 ? argv[1] : L".zip", ASSOC_FILTER_NONE, &handlers);
    if (FAILED(hr)) {
        wprintf(L"SHAssocEnumHandlers 0x%08lx\n", static_cast<unsigned long>(hr));
        return 2;
    }
    IAssocHandler* h = nullptr;
    ULONG fetched = 0;
    while (handlers->Next(1, &h, &fetched) == S_OK) {
        LPWSTR name = nullptr, ui = nullptr, icon = nullptr;
        int index = 0;
        h->GetName(&name);
        h->GetUIName(&ui);
        h->GetIconLocation(&icon, &index);
        wprintf(L"%ls | %ls | icon %ls,%d\n", ui ? ui : L"?", name ? name : L"?", icon ? icon : L"?", index);
        CoTaskMemFree(name);
        CoTaskMemFree(ui);
        CoTaskMemFree(icon);
        h->Release();
    }
    handlers->Release();
    CoUninitialize();
    return 0;
}
