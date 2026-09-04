#include "Foundation/Platform/FileDialog.hpp"

#if defined(_WIN32)

#include <windows.h>
#include <shobjidl.h>
#pragma comment(lib, "Ole32.lib")

std::optional<std::string> vengine::openModelFileDialog() {
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needsUninit = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }

    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)))) {
        COMDLG_FILTERSPEC filters[] = {
            {L"3D Models (*.obj, *.gltf, *.glb)", L"*.obj;*.gltf;*.glb"},
            {L"All Files (*.*)", L"*.*"},
        };
        dialog->SetFileTypes(ARRAYSIZE(filters), filters);
        dialog->SetTitle(L"Import Model");

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR widePath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &widePath))) {
                    int byteCount = WideCharToMultiByte(CP_UTF8, 0, widePath, -1, nullptr, 0, nullptr, nullptr);
                    if (byteCount > 0) {
                        std::string utf8Path(static_cast<size_t>(byteCount) - 1, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, widePath, -1, utf8Path.data(), byteCount, nullptr, nullptr);
                        result = std::move(utf8Path);
                    }
                    CoTaskMemFree(widePath);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (needsUninit) {
        CoUninitialize();
    }
    return result;
}

#else

#include <cstdio>

std::optional<std::string> vengine::openModelFileDialog() {
    std::fprintf(stderr, "Import Model dialog is not implemented on this platform yet.\n");
    return std::nullopt;
}

#endif
