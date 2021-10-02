#include "globals.hpp"

#include "text.hpp"
#include "xwindow.hpp"

//#include <unistd.h>
//#include <pwd.h>
//#include <sys/types.h>

namespace browservice {
/*
namespace {

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

}
*/
Globals::Globals(CKey, shared_ptr<Config> config)
    : config(config),
//      xWindow(XWindow::create()),
      textRenderContext(TextRenderContext::create())//,
//      dotDirPath(getDotDirPath())
{
    REQUIRE(config);
}

shared_ptr<Globals> globals;

}
