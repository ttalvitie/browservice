#pragma once

#include "common.hpp"

namespace browservice {

// Signed data URLs are data URLs which contain arbitrary data as plaintext such
// that we can check whether the URL was created us (using a message
// authentication code).

string generateDataURLSignKey();

string createSignedDataURL(string data, string signKey);

// Returns the data contained by given data URL if it has been signed with given
// key, otherwise returns empty
optional<string> readSignedDataURL(string dataURL, string signKey);

}
