#include "data_url.hpp"

#include "include/cef_parser.h"

#include <Poco/Crypto/DigestEngine.h>

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

string computeHash(string data) {
    Poco::Crypto::DigestEngine hasher("SHA3-256");
    hasher.update(data);
    string hash = hasher.digestToHex(hasher.digest());
    CHECK(hash.size() == HexHashLength);
    return hash;
}

}

string generateDataURLSignKey() {
    random_device rng;
    string key;
    for(size_t i = 0; i < HashLength; ++i) {
        key.push_back(uniform_int_distribution<char>(CHAR_MIN, CHAR_MAX)(rng));
    }
    return key;
}

string createSignedDataURL(string data, string signKey) {
    CHECK(signKey.size() == HashLength);
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
