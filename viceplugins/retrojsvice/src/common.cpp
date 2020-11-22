#include "common.hpp"

#include "../../../vice_plugin_api.h"

namespace retrojsvice {

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

}
