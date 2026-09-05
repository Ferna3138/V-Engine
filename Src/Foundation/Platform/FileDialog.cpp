#include "Foundation/Platform/FileDialog.hpp"

#if defined(_WIN32)

#include <windows.h>
#include <shobjidl.h>
#pragma comment(lib, "Ole32.lib")

namespace {

std::optional<std::string> openFileDialogWithFilter(const wchar_t* title, const wchar_t* filterName,
                                                      const wchar_t* filterPattern) {
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool needsUninit = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }

    std::optional<std::string> result;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)))) {
        COMDLG_FILTERSPEC filters[] = {
            {filterName, filterPattern},
            {L"All Files (*.*)", L"*.*"},
        };
        dialog->SetFileTypes(ARRAYSIZE(filters), filters);
        dialog->SetTitle(title);

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

}  // namespace

std::optional<std::string> vengine::openModelFileDialog() {
    return openFileDialogWithFilter(L"Import Model", L"3D Models (*.obj, *.gltf, *.glb)", L"*.obj;*.gltf;*.glb");
}

std::optional<std::string> vengine::openImageFileDialog() {
    return openFileDialogWithFilter(L"Choose Texture", L"Images (*.png, *.jpg, *.jpeg, *.bmp, *.tga)",
                                     L"*.png;*.jpg;*.jpeg;*.bmp;*.tga");
}

#else

#include <cstdio>

std::optional<std::string> vengine::openModelFileDialog() {
    std::fprintf(stderr, "Import Model dialog is not implemented on this platform yet.\n");
    return std::nullopt;
}

std::optional<std::string> vengine::openImageFileDialog() {
    std::fprintf(stderr, "Choose Texture dialog is not implemented on this platform yet.\n");
    return std::nullopt;
}

#endif
