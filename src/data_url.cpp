#include "data_url.hpp"

#include "tiny_sha3/sha3.hpp"

#include "include/cef_parser.h"

namespace browservice {

namespace {

const size_t HashLength = 32;
const size_t HexHashLength = 64;

const string DataURLHeader = "data:text/plain;base64,";

string urlBase64Encode(string data) {
    return CefURIEncode(CefBase64Encode(data.data(), data.size()), false);
}

optional<string> urlBase64Decode(string urlBase64) {
    string base64 = CefURIDecode(
        urlBase64, false, UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS
    );
    CefRefPtr<CefBinaryValue> bin = CefBase64Decode(base64);
    if(!bin) {
        optional<string> empty;
        return empty;
    }

    vector<char> buf(bin->GetSize());
    bin->GetData(buf.data(), buf.size(), 0);

    return string(buf.begin(), buf.end());
}

char hexDigit(unsigned int val) {
    val &= 15u;
    if(val < 10) {
        return (char)('0' + val);
    } else {
        return (char)('a' + (val - 10));
    }
}

string computeHash(string data) {
    uint8_t hashRaw[32];
    sha3((const unsigned char*)data.data(), data.size(), hashRaw, 32);
    string hash;
    for(uint8_t x : hashRaw) {
        hash.push_back(hexDigit(x >> 4));
        hash.push_back(hexDigit(x & 15u));
    }
    return hash;
}

}

string generateDataURLSignKey() {
    random_device rngDev;
    string key;
    for(size_t i = 0; i < HashLength; ++i) {
        key.push_back((char)uniform_int_distribution<int>(CHAR_MIN, CHAR_MAX)(rngDev));
    }
    return key;
}

string createSignedDataURL(string data, string signKey) {
    REQUIRE(signKey.size() == HashLength);
    return DataURLHeader + urlBase64Encode(computeHash(signKey + data) + data);
}

optional<string> readSignedDataURL(string dataURL, string signKey) {
    optional<string> empty;
    if(
        dataURL.size() < DataURLHeader.size() ||
        dataURL.substr(0, DataURLHeader.size()) != DataURLHeader
    ) {
        return empty;
    }

    optional<string> urlDataMaybe =
        urlBase64Decode(dataURL.substr(DataURLHeader.size()));
    if(!urlDataMaybe) {
        return empty;
    }
    string urlData = move(*urlDataMaybe);

    if(urlData.size() < HexHashLength) {
        return empty;
    }

    string data = urlData.substr(HexHashLength);

    if(dataURL == createSignedDataURL(data, signKey)) {
        return data;
    } else {
        return empty;
    }
}

}
