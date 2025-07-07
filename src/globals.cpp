#include "globals.hpp"

#include "text.hpp"

#ifdef _WIN32
#include <fileapi.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace browservice {

namespace {

#ifdef _WIN32
wstring getAppDataPath() {
    PWSTR str;
    REQUIRE(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &str) == S_OK);
    wstring ret = str;
    CoTaskMemFree(str);
    return ret;
}

wstring getDotDirPath() {
    return getAppDataPath() + L"\\Browservice";
}

bool ensureDirExists(wstring path) {
    if(CreateDirectory(path.c_str(), nullptr)) {
        return true;
    } else {
        if(GetLastError() == ERROR_ALREADY_EXISTS) {
            DWORD attrib = GetFileAttributesW(path.c_str());
            if(attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                return true;
            }
        }
        return false;
    }
}

bool ensureDirExists(string path) {
    CefString cefPath = path;
    wstring widePath = cefPath;
    return ensureDirExists(widePath);
}
#else
string getHomeDirPath() {
    const char* path = getenv("HOME");
    if(path != nullptr) {
        return path;
    }

    uid_t uid = getuid();

    long bufSizeSuggestion = sysconf(_SC_GETPW_R_SIZE_MAX);
    size_t bufSize = bufSizeSuggestion > 0 ? (size_t)bufSizeSuggestion : (size_t)1;

    while(true) {
        struct passwd pwd;
        char* buf = (char*)malloc(bufSize);
        REQUIRE(buf != nullptr);

        int resultCode;
        struct passwd* resultPtr;
        resultCode = getpwuid_r(uid, &pwd, buf, bufSize, &resultPtr);
        if(resultCode == 0 && resultPtr == &pwd) {
            string path = pwd.pw_dir;
            free(buf);
            return path;
        }
        REQUIRE(resultCode == ERANGE && resultPtr == nullptr);
        bufSize *= 2;
    }
}

string getDotDirPath() {
    return getHomeDirPath() + "/.browservice";
}

bool ensureDirExists(string path) {
    if(mkdir(path.c_str(), 0700) == 0) {
        return true;
    } else {
        if(errno == EEXIST) {
            struct stat st;
            if(stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                return true;
            }
        }
        return false;
    }
}
#endif

}

Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config),
      dotDirPath(getDotDirPath()),
      textRenderContext(TextRenderContext::create())
{
    REQUIRE(config);

    if(!ensureDirExists(dotDirPath)) {
        PANIC("Directory '", dotDirPath, "' does not exist and creating it failed");
    }
    if(!config->dataDir.empty() && !ensureDirExists(config->dataDir)) {
        PANIC("Data directory '", config->dataDir, "' does not exist and creating it failed");
    }
}

shared_ptr<Globals> globals;

}
