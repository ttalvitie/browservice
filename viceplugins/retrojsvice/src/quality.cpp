#include "quality.hpp"

namespace retrojsvice {

bool hasPNGSupport(string userAgent) {
    for(char& c : userAgent) {
        c = tolower(c);
    }
    return
        userAgent.find("windows 3.1") == string::npos &&
        userAgent.find("win16") == string::npos &&
        userAgent.find("windows 16-bit") == string::npos;
}

int getMaxQuality(bool allowPNG) {
    if(allowPNG) {
        return MaxQuality;
    } else {
        return MaxQuality - 1;
    }
}

}
