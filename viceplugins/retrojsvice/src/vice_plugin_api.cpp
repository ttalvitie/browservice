#include <iostream>

#include "../../../vice_plugin_api.h"

extern "C" {

int vicePluginAPI_isAPIVersionSupported(uint64_t apiVersion) {
    std::cerr << "MOI\n";
    return (int)(apiVersion == (uint64_t)1000000);
}

}
