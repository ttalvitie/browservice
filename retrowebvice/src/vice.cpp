#include "common.hpp"

// Vice (view service) API definition:
extern "C" {
    void* vicePlugin_createContext(uint64_t apiVersion);
}

struct Context {};

void* vicePlugin_createContext(uint64_t apiVersion) {
    cerr << "vicePlugin_createContext called!\n";
    if(apiVersion == (uint64_t)1) {
        return (void*)(new Context);
    } else {
        return nullptr;
    }
}
