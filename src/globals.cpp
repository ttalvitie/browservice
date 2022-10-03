#include "globals.hpp"

#include "text.hpp"

#ifdef _WIN32
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
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
#endif

}

Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config),
      dotDirPath(getDotDirPath()),
      textRenderContext(TextRenderContext::create())
{
    REQUIRE(config);
}

shared_ptr<Globals> globals;

}
