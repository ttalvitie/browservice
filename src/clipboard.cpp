#include "clipboard.hpp"

namespace browservice {

// Accessor functions defined in patched CEF (see https://bitbucket.org/toptalvitie/cef/src/master/ ).
extern "C" char* cef_chromiumBrowserviceClipboardPaste();
extern "C" void cef_chromiumBrowserviceClipboardFreePasteResult(char* str);
extern "C" void cef_chromiumBrowserviceClipboardCopy(const char* str);

void copyToClipboard(string str) {
	cef_chromiumBrowserviceClipboardCopy(str.c_str());
}

string pasteFromClipboard() {
	char* buf = cef_chromiumBrowserviceClipboardPaste();
	REQUIRE(buf != nullptr);
	string str = buf;
	cef_chromiumBrowserviceClipboardFreePasteResult(buf);
	return str;
}

}
