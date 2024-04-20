#include "common.hpp"

#include "../../../vice_plugin_api.h"

namespace retrojsvice {

string sanitizeUTF8String(string str) {
    string ret;
    for(size_t i = 0; i < str.size(); ++i) {
        int ch = (int)(uint8_t)str[i];
        if(ch == 0) {
            continue;
        }
        if(ch < 128) {
            ret.push_back((char)ch);
            continue;
        }

        size_t length;
        if((ch & 0xE0) == 0xC0) {
            length = 2;
        } else if((ch & 0xF0) == 0xE0) {
            length = 3;
        } else if((ch & 0xF8) == 0xF0) {
            length = 4;
        } else {
            continue;
        }

        if(i + length > str.size()) {
            continue;
        }

        bool ok = true;
        int point = ch & (0x7F >> length);
        for(size_t j = 1; j < length; ++j) {
            int ch2 = (int)(uint8_t)str[i + j];
            if((ch2 & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            point = (point << 6) | (ch2 & 0x3F);
        }
        if(!ok) {
            continue;
        }

        if(
            (length == (size_t)2 && point >= 0x80 && point <= 0x7FF) ||
            (length == (size_t)3 && (
                (point >= 0x800 && point <= 0xD7FF) ||
                (point >= 0xE000 && point <= 0xFFFF)
            )) ||
            (length == (size_t)4 && point >= 0x10000 && point <= 0x10FFFF)
        ) {
            ret.append(str.substr(i, length));
            i += length - 1;
        }
    }
    return ret;
}

vector<string> splitStr(
    const string& str,
    char delim,
    size_t maxSplits
) {
    vector<string> ret;
    string item;
    for(char c : str) {
        if(c == delim && maxSplits != 0) {
            ret.push_back(move(item));
            item = "";
            --maxSplits;
        } else {
            item.push_back(c);
        }
    }
    ret.push_back(move(item));
    return ret;
}

bool isNonEmptyNumericStr(const string& str) {
    if(str.empty()) {
        return false;
    }
    for(char c : str) {
        if(!(c >= '0' && c <= '9')) {
            return false;
        }
    }
    return true;
}

namespace {

void defaultLogCallback(
    LogLevel logLevel,
    const char* location,
    const char* msg
) {
    const char* logLevelStr;
    if(logLevel == LogLevel::Error) {
        logLevelStr = "ERROR";
    } else if(logLevel == LogLevel::Warning) {
        logLevelStr = "WARNING";
    } else {
        REQUIRE(logLevel == LogLevel::Info);
        logLevelStr = "INFO";
    }

    stringstream ss;
    ss << logLevelStr << " @ retrojsvice-plugin " << location << " -- ";
    ss << msg << '\n';

    cerr << ss.str();
}

void defaultPanicCallback(
    const char* location,
    const char* msg
) {
    stringstream ss;
    ss << "PANIC @ retrojsvice-plugin " << location;
    if(msg[0] != '\0') {
        ss << ": " << msg;
    }
    ss << '\n';

    cerr << ss.str();
    cerr.flush();
    abort();
}

mutex logCallbackMutex;
function<void(LogLevel, const char*, const char*)> logCallback =
    defaultLogCallback;

mutex panicCallbackMutex;
function<void(const char*, const char*)> panicCallback =
    defaultPanicCallback;

}

void LogWriter::log_(string msg) {
    lock_guard<mutex> lock(logCallbackMutex);
    logCallback(logLevel_, location_.c_str(), msg.c_str());
}

void Panicker::panic_(string msg) {
    lock_guard<mutex> lock(panicCallbackMutex);
    panicCallback(location_.c_str(), msg.c_str());
    abort();
}

void setLogCallback(function<void(LogLevel, const char*, const char*)> callback) {
    lock_guard<mutex> lock(logCallbackMutex);
    if(callback) {
        logCallback = callback;
    } else {
        logCallback = defaultLogCallback;
    }
}

void setPanicCallback(function<void(const char*, const char*)> callback) {
    lock_guard<mutex> lock(panicCallbackMutex);
    if(callback) {
        panicCallback = callback;
    } else {
        panicCallback = defaultPanicCallback;
    }
}

char* createMallocString(string val) {
    size_t size = val.size() + 1;
    char* ret = (char*)malloc(size);
    REQUIRE(ret != nullptr);
    memcpy(ret, val.c_str(), size);
    return ret;
}

thread_local bool inAPIThread_ = false;

MCE mce;

}
