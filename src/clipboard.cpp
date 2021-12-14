#ifdef _WIN32

#include "clipboard.hpp"

namespace browservice {

// Accessor functions defined in patched CEF (see https://bitbucket.org/toptalvitie/cef/src/master/ ).
extern "C" char* chromiumBrowserviceClipboardPaste();
extern "C" void chromiumBrowserviceClipboardFreePasteResult(char* str);
extern "C" void chromiumBrowserviceClipboardCopy(const char* str);

void copyToClipboard(string str) {
	chromiumBrowserviceClipboardCopy(str.c_str());
}

string pasteFromClipboard() {
	char* buf = chromiumBrowserviceClipboardPaste();
	REQUIRE(buf != nullptr);
	string str = buf;
	chromiumBrowserviceClipboardFreePasteResult(buf);
	return str;
}

}

#endif
