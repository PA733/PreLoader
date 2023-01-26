﻿#include "preloader.h"

#include <filesystem>
#include <set>
#include <string>

#include "logger.h"
#include "utils.h"

#include <windows.h>


using namespace pl::utils;
using namespace std::filesystem;
using std::string;
using std::wstring;

constexpr const int MAX_PATH_LENGTH = 8192;

std::set<std::string> preloadList;

namespace pl {

void addLibraryToPath() {
    auto* buffer = new (std::nothrow) WCHAR[MAX_PATH_LENGTH];
    if (!buffer)
        return;

    DWORD length = GetEnvironmentVariableW(L"PATH", buffer, MAX_PATH_LENGTH);
    std::wstring path(buffer, length);
    length = GetCurrentDirectoryW(MAX_PATH_LENGTH, buffer);
    std::wstring currentDir(buffer, length);

    SetEnvironmentVariableW(L"PATH", (currentDir + L"\\plugins\\lib;" + path).c_str());
    delete[] buffer;
}


bool loadLibrary(const string& libName, bool showFailInfo = true) {
    if (LoadLibraryW(str2wstr(libName).c_str())) {
        Info("{} Injected.", std::filesystem::path(libName).filename().u8string());
        return true;
    } else {
        if (showFailInfo) {
            DWORD error_message_id = GetLastError();
            Error("Can't load {} !", libName);
            Error("Error code: {} !", error_message_id);
            LPWSTR message_buffer = nullptr;
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                          nullptr, error_message_id, MAKELANGID(0x09, SUBLANG_DEFAULT), (LPWSTR)&message_buffer, 0,
                          nullptr);
            Error("{}", wstr2str(message_buffer));
            LocalFree(message_buffer);
        }
        return false;
    }
}

void loadRawLibraries() {
    std::filesystem::create_directories("plugins");
    directory_iterator ent("plugins");
    for (auto& file : ent) {
        if (!file.is_regular_file())
            continue;

        auto& path = file.path();
        auto fileName = path.u8string();

        string ext = path.extension().u8string();
        if (ext != ".dll") {
            continue;
        }

        if (preloadList.count(fileName))
            continue;

        string dllFileName = path.filename().u8string();
        auto lib = LoadLibrary(str2wstr(fileName).c_str());
        if (lib) {
            Info("Dll <{}> Injected", dllFileName);
        } else {
            Error("Fail to load library [{}]", dllFileName);
            Error("Error: Code[{}]", GetLastError());
        }
    }
}

bool loadLiteLoader() {
    if (!exists(path("LiteLoader.dll")))
        return false;
    return loadLibrary("LiteLoader.dll");
}

void setup() {
    if (exists(path(".\\plugins\\preload.conf"))) {
        std::ifstream dllList(".\\plugins\\preload.conf");
        if (dllList) {
            std::string dllName;
            while (getline(dllList, dllName)) {
                if (dllName.back() == '\n')
                    dllName.pop_back();
                if (dllName.back() == '\r')
                    dllName.pop_back();

                if (dllName.empty() || dllName.front() == '#')
                    continue;
                if (dllName.find("LiteLoader.dll") != std::wstring::npos)
                    continue;
                std::cout << "Preload: " << dllName << std::endl;
                loadLibrary(dllName);
                preloadList.insert(dllName);
            }
            dllList.close();
        }
    } else {
        std::ofstream dllList(".\\plugins\\preload.conf");
        dllList.close();
    }
    if (!loadLiteLoader()) {
        Warn("LiteLoader not found, PreLoader is running as DLL Loader...");
        loadRawLibraries();
    }
}

void Init() {
    loadLoggerConfig();
    addLibraryToPath();
    setup();
}
} // namespace pl
