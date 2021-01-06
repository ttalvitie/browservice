#include "quality.hpp"

#include "globals.hpp"

namespace browservice {

bool hasPNGSupport(string userAgent) {
    for(char& c : userAgent) {
        c = tolower(c);
    }
    return
        userAgent.find("windows 3.1") == string::npos &&
        userAgent.find("win16") == string::npos &&
        userAgent.find("windows 16-bit") == string::npos;
}

int getDefaultQuality(bool allowPNG) {
    int ret = MaxQuality;
    if(!allowPNG && ret == MaxQuality) {
        --ret;
    }
    REQUIRE(ret >= MinQuality && ret <= getMaxQuality(allowPNG));
    return ret;
}

int getMaxQuality(bool allowPNG) {
    if(allowPNG) {
        return MaxQuality;
    } else {
        return MaxQuality - 1;
    }
}

}
